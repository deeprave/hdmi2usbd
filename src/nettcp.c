//
// Created by David Nugent on 2/02/2016.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/fcntl.h>
#include <stdlib.h>

#include "nettcp.h"
#include "netutils.h"


tcp_cfg_t *
tcp_getcfg(iodev_t *sdev) {
    return (tcp_cfg_t *)iodev_getcfg(sdev);
}


void
tcp_free_cfg(iodev_cfg_t *cfg) {
    tcp_cfg_t *tcpcfg = (tcp_cfg_t *)cfg;
    free(tcpcfg->local);
    free(tcpcfg->remote);
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
tcp_accepted(iodev_t *dev, int fd) {
//  tcp_cfg_t *cfg = tcp_getcfg(dev);

    iodev_setstate(dev, IODEV_OPEN);
    return dev->fd = fd;
}


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

        // set non-blocking
        int opts = fcntl(dev->fd, F_GETFL);
        if (opts < 0)
            iodev_error("fcntl(%d, F_GETFL) error(%d): %s", dev->fd, errno, strerror(errno));
        opts |= O_NONBLOCK;
        if (fcntl(dev->fd, F_SETFL, opts) < 0)
            iodev_error("fcntl(%d, F_SETFL) error(%d): %s", dev->fd, errno, strerror(errno));

        // finally bind it
        if (bind(dev->fd, cfg->local, cfg->local->sa_len) == -1) {
            iodev_error("socket bind error(%d): %s", errno, strerror(errno));
            dev->(dev, IOFLAG_INACTIVE);
        } else
            iodev_setstate(dev, IODEV_CONNECTED);
    }

    return dev->fd;
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


static int
tcp_configure(iodev_t *dev, void * data) {
    return 0;
}


static iodev_t *
tcp_create(iodev_t *dev, struct sockaddr *local, struct sockaddr *remote, size_t bufsize) {
    // First create the basic (slightly larger) config
    iodev_cfg_t *cfg = iodev_alloc_cfg(sizeof(tcp_cfg_t), "tcp", tcp_free_cfg);
    iodev_t *tcp = iodev_init(dev, cfg, bufsize);

    // Initialise the extras
    tcp_cfg_t *tcfg = tcp_getcfg(tcp);
    tcfg->local = sockaddr_dup(local);
    tcfg->remote = sockaddr_dup(remote);

    // default functions
    tcp->open = tcp_open;
    tcp->close = tcp_close;
    tcp->configure = tcp_configure;

    return tcp;
}


iodev_t *
tcp_create_listen(iodev_t *dev, struct sockaddr *local, unsigned bufsize) {
    iodev_t *tcp = tcp_create(dev, local, NULL, 0);
    // we don't use bufsize for this socket since there is no IO
    // but use it for devices created via accept(), so record it here
    tcp->bufsize = bufsize;
    // special "open" for listener
    tcp->open = tcp_open_listen;
    return tcp;
}


iodev_t *
tcp_create_connect(iodev_t *dev, struct sockaddr *remote, size_t bufsize) {
    iodev_t *tcp = tcp_create(dev, NULL, remote, bufsize);
    // tcp->close = tcp_close_connect;
    return tcp;
}


iodev_t *
tcp_create_accepted(iodev_t *dev, int fd, size_t bufsize) {
    iodev_t *net = tcp_create(dev, NULL, NULL, bufsize);
    tcp_accepted(dev, fd);
    return net;
}
