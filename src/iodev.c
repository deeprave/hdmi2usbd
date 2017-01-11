//
// Created by David Nugent on 3/02/2016.
//

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>

#include "iodev.h"
#include "stringstore.h"

#define IODEV_ALLOC 0x25a1da5


iodev_cfg_t *iodev_getcfg(iodev_t *iodev) { return iodev->cfg; }
const char *iodev_driver(iodev_t *iodev) { return iodev->cfg->name; }
int iodev_getstate(iodev_t *dev) { return dev->state; }
int iodev_setstate(iodev_t *dev, int state) { return dev->state = state; }
int iodev_getfd(iodev_t *dev) { return dev->fd; }
int iodev_is_listener(iodev_t *dev) { return dev->listener; }
int iodev_is_open(iodev_t *dev) { return iodev_getstate(dev) >= IODEV_OPEN; }
buffer_t *iodev_tbuf(iodev_t *dev) { return &dev->tbuf; }
buffer_t *iodev_rbuf(iodev_t *dev) { return &dev->rbuf; }
stringstore_t *iodev_stringstore(iodev_t *dev) { return dev->linebuf; }

selector_t *getselector(iodev_t *dev) { return dev->selector; }
void setselector(iodev_t *dev, selector_t *selector) { dev->selector = selector; }


// Error message handling

static int
iodev_error_default(char const *fmt, va_list args) {
    char _fmt[strlen(fmt)+2];
    strcpy(_fmt, fmt);
    strcat(_fmt, "\n");
    return vfprintf(stderr, _fmt, args);
}

static int
iodev_notify_default(char const *fmt, va_list args) {
    char _fmt[strlen(fmt)+2];
    strcpy(_fmt, fmt);
    strcat(_fmt, "\n");
    return vfprintf(stdout, _fmt, args);
}

static int (*iodev_errfunc)(char const *fmt, va_list args) = iodev_error_default;
static int (*iodev_notifyfunc)(char const *fmt, va_list args) = iodev_notify_default;

void
iodev_seterrfunc(int (*func)(char const *, va_list)) {
    iodev_errfunc = func;
}

void
iodev_setnotify(int (*func)(char const *, va_list)) {
    iodev_notifyfunc = func;
}

int
iodev_error(char const *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    int rc = iodev_errfunc(fmt, arg);
    va_end(arg);
    return rc;
}

int
iodev_notify(char const *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    int rc = iodev_notifyfunc(fmt, arg);
    va_end(arg);
    return rc;
}

static int
not_implemented(iodev_t *dev, char const *name) {
    return iodev_error("unimplemented function '%s' called, device = %s", name, iodev_driver(dev));
}


static void free_cfg_default(iodev_cfg_t *dev) { }

iodev_cfg_t *
iodev_alloc_cfg(size_t size, const char *name, void (*free_cfg)(iodev_cfg_t *)) {
    iodev_cfg_t *cfg = calloc(1, size);
    cfg->name = name;
    cfg->free_cfg = free_cfg ? free_cfg : free_cfg_default;
    return cfg;
}


void
iodev_free_cfg(iodev_cfg_t *cfg) {
    if (cfg != NULL) {
        cfg->free_cfg(cfg);
        free(cfg);
    }
}


// redirect to device-specific routines

static int
iodev_open(iodev_t *dev) {
    return not_implemented(dev, "open");
}


static void
iodev_close(iodev_t *dev, int flags) {
    not_implemented(dev, "close");
}


static int
iodev_configure(iodev_t *dev, void *data) {
    return not_implemented(dev, "configure");
}


static int
iodev_is_set(iodev_t *dev, fd_set *fds) {
    return dev->fd == -1 ? 0 : FD_ISSET(dev->fd, fds);
}


static int
iodev_set_masks(iodev_t *dev, fd_set *r, fd_set *w, fd_set *x) {
    int is_active = 0;
    if (dev->fd >= 0) {
        // clear all by default
        FD_CLR(dev->fd, r);
        FD_CLR(dev->fd, w);
        FD_CLR(dev->fd, x);
    }
    switch (iodev_getstate(dev)) {
        case IODEV_INACTIVE:    // device closed/dead
        default:
            break;
        case IODEV_CLOSING:     // pre-close flushing
            if (buffer_used(iodev_tbuf(dev)) > 0) {
                if (dev->sendOk(dev)) {
                    FD_SET(dev->fd, w);
                    FD_SET(dev->fd, x);
                }
                is_active++;
            } else
                dev->close(dev, IOFLAG_NONE);
            break;
        case IODEV_NONE:        // default (startup) state
        case IODEV_CLOSED:      // currently closed, due for reopen
            dev->open(dev);
            break;
        case IODEV_PENDING:     // waiting for open to complete
            FD_SET(dev->fd, r);
            is_active++;
            break;
        case IODEV_OPEN:        // open/operating
        case IODEV_CONNECTED:   // connected
        case IODEV_ACTIVE:      // connected with I/O pending
            if (buffer_available(iodev_rbuf(dev)) > 0)
                FD_SET(dev->fd, r);
            if (buffer_used(iodev_tbuf(dev)) > 0)
                FD_SET(dev->fd, w);
            FD_SET(dev->fd, x);
            is_active++;
            break;
    }
    return is_active;
}


static ssize_t
iodev_read_handler(iodev_t *dev) {
    ssize_t rc = -1;
    iodev_cfg_t *cfg = iodev_getcfg(dev);

    if (dev->fd == -1)
        iodev_error("iodev %s read error: device is closed", cfg->name);
    else {
        size_t available = buffer_available(&dev->rbuf);
        void *tmp = alloca(available);
        rc = iodev_read(dev, tmp, available);
        if (rc > 0)
            buffer_put(&dev->rbuf, tmp, (size_t)rc);
        else {
            if (rc < 0)
                iodev_error("iodev %s read error(%d): %s, closing", cfg->name, errno, strerror(errno));
            else // (rc == 0) // eof
                iodev_notify("iodev %s EOF from fd %d, closing", cfg->name, dev->fd);
            buffer_flush(&dev->rbuf);
            buffer_flush(&dev->tbuf);
            dev->close(dev, IODEV_CLOSED);
        }
    }
    return rc;
}


static ssize_t
iodev_write_handler(iodev_t *dev) {
    ssize_t rc = -1;
    iodev_cfg_t *cfg = iodev_getcfg(dev);

    if (dev->fd == -1)
        iodev_error("iodev %s write error: device is closed", cfg->name);
    else {
        size_t available = buffer_used(&dev->tbuf);
        if (!available)
            rc = available;
        else {
            void *tmp = alloca(available);
            buffer_peek(&dev->tbuf, tmp, available);
            rc = write(dev->fd, tmp, available);
            if (rc < 0) {
                iodev_error("iodev %s write error(%d): %s", cfg->name, errno, strerror(errno));
                buffer_flush(&dev->rbuf);
                buffer_flush(&dev->tbuf);
                dev->close(dev, IODEV_NONE);
            } else { // advance the counter by amount written
                buffer_get(&dev->tbuf, NULL, (size_t)rc);
            }
        }
    }
    return rc;
}


static ssize_t
iodev_except_handler(iodev_t *dev) {
    int fd = dev->fd, err = errno;
    dev->close(dev, IODEV_NONE);
    return iodev_error("exception fd %d, device = %s error(%d): %s", fd, iodev_driver(dev), err, strerror(err));
}


ssize_t
iodev_read(iodev_t *dev, void *buf, size_t len) {
    if (dev == NULL || !iodev_is_open(dev)) {
        iodev_error("iodev.read() called with invalid or closed device");
        return -1;
    }
    return read(dev->fd, buf, len);
}

ssize_t
iodev_write(iodev_t *dev, void const *buf, size_t len) {
    if (dev == NULL || !iodev_is_open(dev)) {
        iodev_error("iodev.write() called with invalid or closed device");
        return -1;
    }
    if (buf != NULL && len > 0)
        buffer_put(&dev->tbuf, buf, len);
    return len;
}

static int
iodev_sendok(iodev_t *dev) {
    return 1;
}


iodev_t *
iodev_init(iodev_t *dev, iodev_cfg_t *cfg, size_t bufsize) {
    if (dev == NULL) {
        dev = calloc(1, sizeof(iodev_t));
        dev->alloc = IODEV_ALLOC;
    } else
        memset(dev, '\0', sizeof(iodev_t));
    dev->cfg = cfg;
    dev->fd = -1;
    dev->listener = bufsize == 0;
    dev->selector = NULL;
    dev->state = IODEV_NONE;
    dev->bufsize = bufsize;
    buffer_init(&dev->rbuf, bufsize);
    buffer_init(&dev->tbuf, bufsize);
    // default i/o functions
    dev->open = iodev_open;
    dev->close = iodev_close;
    dev->configure = iodev_configure;
    dev->set_masks = iodev_set_masks;
    dev->is_set = iodev_is_set;
    dev->read_handler = iodev_read_handler;
    dev->write_handler = iodev_write_handler;
    dev->except_handler = iodev_except_handler;
    dev->read = iodev_read;
    dev->write = iodev_write;
    dev->sendOk = iodev_sendok;
    return dev;
}


void
iodev_free(iodev_t *dev) {
    if (dev != NULL) {
        buffer_free(&dev->rbuf);
        buffer_free(&dev->tbuf);
        iodev_free_cfg(dev->cfg);
        stringstore_free(dev->linebuf);
        if (dev->alloc == IODEV_ALLOC) {
            dev->alloc = 0;
            free(dev);
        }
    }
}

