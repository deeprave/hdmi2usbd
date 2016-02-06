//
// Created by David Nugent on 2/02/2016.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "nettcp.h"


tcp_cfg_t *
tcp_getcfg(iodev_t *sdev) {
    return (tcp_cfg_t *)iodev_getcfg(sdev);
}


// device control
static int
tcp_open(iodev_t *dev) {
//  tcp_cfg_t *cfg = tcp_getcfg(dev);

    if (iodev_getstate(dev) >= IODEV_OPEN)
        iodev_close(dev, IOFLAG_NONE);

    return dev->fd;
}


static int
tcp_accepted(iodev_t *dev, int fd) {
//  tcp_cfg_t *cfg = tcp_getcfg(dev);

    iodev_setstate(dev, IODEV_OPEN);
    return dev->fd = fd;
}


static int
tcp_listen_open(iodev_t *dev) {
//  tcp_cfg_t *cfg = tcp_getcfg(dev);

    if (iodev_getstate(dev) >= IODEV_OPEN)
        iodev_close(dev, IOFLAG_NONE);

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
tcp_configure(iodev_t *dev, void *data) {
    return 0;
}


static int
tcp_read_handler(iodev_t *dev) {
    return 0;
}


static int
tcp_write_handler(iodev_t *dev) {
    return 0;
}


static int
tcp_except_handler(iodev_t *dev) {
    return 0;
}


static int
tcp_read(iodev_t *dev, void *buf, size_t len) {
    return 0;
}


static int
tcp_write(iodev_t *dev, void const *buf, size_t len) {
    return 0;
}


static iodev_t *
tcp_create(struct sockaddr *local, struct sockaddr *remote, size_t bufsize) {
    // First create the basic (slightly larger) config
    iodev_cfg_t *cfg = alloc_cfg(sizeof(tcp_cfg_t), "tcp", 0);
    iodev_t *tcp = iodev_create(cfg, bufsize);

    // Initialise the extras
    tcp_cfg_t *tcfg = tcp_getcfg(tcp);
    tcfg->local = local;
    tcfg->remote = remote;

    tcp->open = tcp_open;
    tcp->close = tcp_close;
    tcp->configure = tcp_configure;
    tcp->read_handler = tcp_read_handler;
    tcp->write_handler = tcp_write_handler;
    tcp->except_handler = tcp_except_handler;
    tcp->read = tcp_read;
    tcp->write = tcp_write;

    return tcp;
}


iodev_t *
tcp_create_listen(struct sockaddr *local) {
    iodev_t *net = tcp_create(local, NULL, 0);
    return net;
}


iodev_t *
tcp_create_connect(struct sockaddr *remote, size_t bufsize) {
    iodev_t *net = tcp_create(NULL, remote, bufsize);
    return net;
}
