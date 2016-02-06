//
// Created by David Nugent on 3/02/2016.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "iodev.h"


iodev_cfg_t *iodev_getcfg(iodev_t *iodev) { return iodev->cfg; }
const char *iodev_name(iodev_t *iodev) { return iodev->cfg->name; }
int iodev_getstate(iodev_t *dev) { return dev->state; }
int iodev_setstate(iodev_t *dev, int state) { return dev->state = state; }
int iodev_getfd(iodev_t *dev) { return dev->fd; }
buffer_t *iodev_tbuf(iodev_t *dev) { return &dev->tbuf; }
buffer_t *iodev_rbuf(iodev_t *dev) { return &dev->rbuf; }


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
iodev_seterrfunc(int (*func)(char const *fmt, va_list args)) {
    iodev_errfunc = func;
}

void
iodev_setnotify(int (*func)(char const *fmt, va_list args)) {
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
    return iodev_error("unimplemented function '%s' called, device = %s", name, iodev_name(dev));
}



iodev_cfg_t *
alloc_cfg(size_t size, const char *name, int listener) {
    iodev_cfg_t *cfg = calloc(1, size);
    cfg->name = name;
    cfg->fd = -1;
    cfg->listener = listener;
    return cfg;
}


iodev_t *
iodev_create(iodev_cfg_t *cfg, size_t bufsize) {
    iodev_t *dev = malloc(sizeof(iodev_t));
    memset(dev, '\0', sizeof(iodev_t));
    dev->cfg = cfg;
    dev->fd = -1;
    dev->selector = NULL;
    dev->state = IODEV_NONE;
    buffer_init(&dev->rbuf, bufsize);
    buffer_init(&dev->rbuf, bufsize);
    return dev;
}


void
iodev_free(iodev_t *dev) {
    buffer_free(&dev->rbuf);
    buffer_free(&dev->tbuf);
    free(dev);
}


// redirect to device-specific routines

int
iodev_open(iodev_t *dev) {
    return dev->open ? dev->open(dev) : not_implemented(dev, "open");
}

void
iodev_close(iodev_t *dev, int flags) {
    dev->close ? dev->close(dev, flags) : not_implemented(dev, "close");
}

int
iodev_configure(iodev_t *dev, void *data) {
    return dev->configure ? dev->configure(dev, data) : not_implemented(dev, "configure");
}

int
iodev_read_handler(iodev_t *dev) {
    return dev->read_handler ? dev->read_handler(dev) : not_implemented(dev, "read_handler");
}

int iodev_write_handler(iodev_t *dev) {
    return dev->write_handler ? dev->write_handler(dev) : not_implemented(dev, "write_handler");
}

int iodev_except_handler(iodev_t *dev) {
    return dev->except_handler ? dev->except_handler(dev) : not_implemented(dev, "except_handler");
}

int
iodev_read(iodev_t *dev, void *buf, size_t len) {
    return dev->read ? dev->read(dev, buf, len) : not_implemented(dev, "read");
}

int
iodev_write(iodev_t *dev, void const *buf, size_t len) {
    return dev->write ? dev->write(dev, buf, len) : not_implemented(dev, "write");
}
