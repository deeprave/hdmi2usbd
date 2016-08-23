//
// Created by David Nugent on 2/02/2016.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "nettcp.h"
#include "netutils.h"
#include "selector.h"
#include "stringstore.h"


tcp_cfg_t *
tcp_getcfg(iodev_t *sdev) {
    return (tcp_cfg_t *)iodev_getcfg(sdev);
}


void
tcp_free_cfg(iodev_cfg_t *cfg) {
    if (cfg != NULL) {
        tcp_cfg_t *tcpcfg = (tcp_cfg_t *)cfg;
        free(tcpcfg->local);
        free(tcpcfg->remote);
    }
}


// device control
static int
tcp_open(iodev_t *dev) {
//  tcp_cfg_t *cfg = tcp_getcfg(dev);

    if (iodev_getstate(dev) >= IODEV_OPEN)
        dev->close(dev, IOFLAG_NONE);

    return dev->fd;
}


static int
tcp_accept(iodev_t *dev, int fd, struct sockaddr *addr) {
    char paddr[64];
    iodev_notify("accepted connection from %s on fd=%d",
                 inet_ntop(addr->sa_family, sockaddr_addr(addr), paddr, sizeof(paddr)),
                 fd);
    iodev_setstate(dev, IODEV_OPEN);
    dev->fd = fd;
    // set non-blocking
    int opts = fcntl(dev->fd, F_GETFL);
    if (opts < 0)
        iodev_error("fcntl(%d, F_GETFL) error(%d): %s", dev->fd, errno, strerror(errno));
    else {
        opts |= O_NONBLOCK;
        if (fcntl(dev->fd, F_SETFL, opts) < 0)
            iodev_error("fcntl(%d, F_SETFL) error(%d): %s", dev->fd, errno, strerror(errno));
    }

    iodev_setstate(dev, IODEV_CONNECTED);
    return fd;
}


//// listen socket support functions ////

#define BACKLOG_SIZE 16

static int
tcp_open_listen(iodev_t *dev) {
    if (iodev_getstate(dev) >= IODEV_OPEN)
        dev->close(dev, IOFLAG_NONE);

    tcp_cfg_t *cfg = tcp_getcfg(dev);
    if (dev->fd == -1) {
        dev->fd = socket(cfg->local->sa_family, SOCK_STREAM, IPPROTO_IP);
        if (dev->fd == -1) {
            iodev_error("socket create error(%d): %s", errno, strerror(errno));
            iodev_setstate(dev, IODEV_INACTIVE);
            return dev->fd;
        }
        iodev_setstate(dev, IODEV_PENDING);
        int yes = 1;
        // immediately reusable
        if (setsockopt(dev->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            iodev_error("setsockopt(%d) error(%d): %s", dev->fd, errno, strerror(errno));

        iodev_setstate(dev, IODEV_OPEN);
        // set non-blocking
        int opts = fcntl(dev->fd, F_GETFL);
        if (opts < 0)
            iodev_error("fcntl(%d, F_GETFL) error(%d): %s", dev->fd, errno, strerror(errno));
        else {
            opts |= O_NONBLOCK;
            if (fcntl(dev->fd, F_SETFL, opts) < 0)
                iodev_error("fcntl(%d, F_SETFL) error(%d): %s", dev->fd, errno, strerror(errno));
        }

        // finally bind it and listen
        if (bind(dev->fd, cfg->local, cfg->addrlen) == -1) {
            iodev_error("socket bind error(%d): %s", errno, strerror(errno));
            dev->close(dev, IOFLAG_INACTIVE);
        } else if (listen(dev->fd, BACKLOG_SIZE) < 0) {
            iodev_error("socket bind error(%d): %s", errno, strerror(errno));
            dev->close(dev, IOFLAG_INACTIVE);
        } else {
            iodev_setstate(dev, IODEV_CONNECTED);
        }
    }
    return dev->fd;
}


static int
tcp_set_masks_listen(iodev_t *dev, fd_set *r, fd_set *w, fd_set *x) {
    int is_active = 0;
    if (dev->fd >= 0) {
        // clear all by default
        FD_CLR(dev->fd, r);
        FD_CLR(dev->fd, w);
        FD_CLR(dev->fd, x);
    }
    switch (iodev_getstate(dev)) {
        case IODEV_NONE:        // default (startup) state
        case IODEV_CLOSED:      // currently closed, due for reopen
            dev->open(dev);
        default:
            break;
        case IODEV_PENDING:     // waiting for open to complete
        case IODEV_OPEN:        // open/operating
        case IODEV_CONNECTED:   // connected
        case IODEV_ACTIVE:      // connected with I/O pending
            FD_SET(dev->fd, r);
            FD_SET(dev->fd, x);
            is_active++;
            break;
    }
    return is_active;
}



static ssize_t
tcp_accept_handler(iodev_t *dev) {
    // we got here via read event from select on this socket
    struct sockaddr_storage sock;
    socklen_t socklen = sizeof(sock);

    int fd = accept(dev->fd, (struct sockaddr *)&sock, &socklen);
    if (fd < 0)
        iodev_notify("accept failure(%d): %s", errno, strerror(errno));
    else {
        selector_new_device_accept(dev->selector, fd, (struct sockaddr *)&sock, dev->bufsize);
    }
    return fd;
}


static void
tcp_close(iodev_t *dev, int flags) {
//  tcp_cfg_t *cfg = tcp_getcfg(dev);

    switch (iodev_getstate(dev)) {
        case IODEV_NONE:
        case IODEV_INACTIVE:
        case IODEV_CLOSED:
        //  iodev_notify(dev, "serial close '%s' already closed", cfg->portname);
            // do nothing for these states, we are already closed
            break;
        default:
            // all states other than closing, we move to closing state
            iodev_setstate(dev, IODEV_CLOSING);
        case IODEV_CLOSING:
            // check to see if we have sent all data
            if (flags & IOFLAG_FLUSH) {
                if (buffer_available(iodev_tbuf(dev)))
                    break;  // still data in buffer, retry later
            //  tcflush(dev->fd, TCOFLUSH);
            }
        case IODEV_PENDING: // never flush if only pending
            close(dev->fd);
            dev->fd = -1;
            iodev_setstate(dev, IODEV_CLOSED);
            break;
    }
}


static void
tcp_close_accept(iodev_t *dev, int flags) {
    tcp_close(dev, flags);
    // Move closed -> inactive as we never reuse accept()ed connections
    if (iodev_getstate(dev) == IODEV_CLOSED)
        iodev_setstate(dev, IODEV_INACTIVE);
}


static int
tcp_configure(iodev_t *dev, void *data) {
    return 0;
}

static iodev_t *
tcp_create(iodev_t *dev, struct sockaddr *local, struct sockaddr *remote, size_t bufsize, int with_linebuf) {
    // First create the basic (slightly larger) config
    iodev_cfg_t *cfg = iodev_alloc_cfg(sizeof(tcp_cfg_t), "tcp", tcp_free_cfg);
    iodev_t *tcp = iodev_init(dev, cfg, bufsize);

    // Initialise the extras
    tcp_cfg_t *tcfg = tcp_getcfg(tcp);
    tcfg->addrlen = local != NULL ? sockaddr_len(local) : remote != NULL ? sockaddr_len(remote) : 0;
    tcfg->local = sockaddr_dup(local);
    tcfg->remote = sockaddr_dup(remote);
    if (with_linebuf)
        dev->linebuf = stringstore_init(NULL);

    // default functions
    tcp->open = tcp_open;
    tcp->close = tcp_close;
    tcp->configure = tcp_configure;

    return tcp;
}


iodev_t *
tcp_create_listen(iodev_t *dev, struct sockaddr *local, size_t bufsize) {
    iodev_t *tcp = tcp_create(dev, local, NULL, 0, 0);

    // we don't use bufsize for this socket since there is no IO
    // but use it for devices created via accept(), so record it here
    tcp->bufsize = bufsize;

    // special "open" and "read" for listen sockets
    tcp->open = tcp_open_listen;
    tcp->read_handler = tcp_accept_handler;
    tcp->set_masks = tcp_set_masks_listen;

    return tcp;
}


iodev_t *
tcp_create_connect(iodev_t *dev, struct sockaddr *remote, size_t bufsize) {
    iodev_t *tcp = tcp_create(dev, NULL, remote, bufsize, 1);

    // tcp->close = tcp_close_connect;

    return tcp;
}


iodev_t *
tcp_create_accepted(iodev_t *dev, int fd, struct sockaddr *remote, size_t bufsize) {
    iodev_t *tcp = tcp_create(dev, NULL, remote, bufsize, 1);
    tcp_accept(tcp, fd, remote);
    tcp->close = tcp_close_accept;
    return tcp;
}
