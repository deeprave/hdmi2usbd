//
// Created by David Nugent on 3/02/2016.
//

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/errno.h>

#include "selector.h"
#include "nettcp.h"
#include "serial.h"
#include "logging.h"


#define SELECTOR_ALLOC 0xa51d15a

selector_t *
selector_init(selector_t *selector) {
    if (selector != NULL)
        memset(selector, '\0', sizeof(selector_t));
    else {
        selector = calloc(1, sizeof(selector_t));
        selector->alloc = SELECTOR_ALLOC;
    }
    array_init(&selector->devs, sizeof(iodev_t), 8);
    return selector;
}


void
selector_free(selector_t *selector) {
    // Make sure that all devices are in closed state
    for (size_t i =0; i < array_count(&selector->devs); i++) {
        iodev_t *dev = selector_get_device(selector, i);
        if (iodev_getstate(dev) > IODEV_CLOSED)
            dev->close(dev, IOFLAG_INACTIVE);
    }
    // Free the device array
    array_free(&selector->devs);
    // Free the selector if we originally allocated it
    if (selector->alloc == SELECTOR_ALLOC) {
        selector->alloc = 0;
        free(selector);
    }
}


size_t
selector_device_count(selector_t *selector) {
    return array_count(&selector->devs);
}


iodev_t *
selector_get_device(selector_t *selector, size_t index) {
    return (iodev_t *)array_get(&selector->devs, index);
}


iodev_t *
selector_set(selector_t *selector, iodev_t *dev) {
    dev->selector = selector;
    return dev;
}


static iodev_t *
selector_new_device(selector_t *selector) {
    // first, look for an inactive slot
    for (size_t index =1; index < array_count(&selector->devs); ++index) {
        iodev_t *dev = array_get(&selector->devs, index);
        if (iodev_getstate(dev) == IODEV_INACTIVE)
            return dev;
    }
    // otherwise, append new one at end
    return (iodev_t *)array_new(&selector->devs);
}

// Four device type creators

// Allocate a serial iodev
iodev_t *
selector_new_device_serial(selector_t *selector, char const *devname, unsigned long baudrate, size_t bufsize) {
    return selector_set(selector, serial_create(selector_new_device(selector), devname, baudrate, bufsize));
}

// Allocate a listen socket iodev
iodev_t *
selector_new_device_listen(selector_t *selector, struct sockaddr *local, size_t bufsize) {
    return selector_set(selector, tcp_create_listen(selector_new_device(selector), local, bufsize));
}

// Allocate a socket connection iodev
iodev_t *
selector_new_device_connect(selector_t *selector, struct sockaddr *remote, size_t bufsize) {
    return selector_set(selector, tcp_create_connect(selector_new_device(selector), remote, bufsize));
}

// Allocate an accepted socket connection iodev
iodev_t *
selector_new_device_accept(selector_t *selector, int fd, struct sockaddr *remote, size_t bufsize) {
    return selector_set(selector, tcp_create_accepted(selector_new_device(selector), fd, remote, bufsize));
}


typedef struct selector_status_s selector_status_t;
struct selector_status_s {
    int active_count;
    int highest_fd;
};


static selector_status_t
selector_ioset(selector_t *selector, fd_set *r, fd_set *w, fd_set *x) {
    selector_status_t stat  = { 0, 0 };
    array_t *devs = &selector->devs;
    for (size_t index =0; index < array_count(devs); ++index) {
        iodev_t *dev = array_get(devs, index);
        if (dev->set_masks(dev, r, w, x)) {
            stat.active_count++;
            int fd = iodev_getfd(dev);
            if (fd > stat.highest_fd)
                stat.highest_fd = fd;
        }
    }
    return stat;
}


static int
selector_dispatch(selector_t *selector, int ready, fd_set *r, fd_set *w, fd_set *x) {
    int rc = 0;
    array_t *devs = &selector->devs;
    for (size_t index =0; ready > 0 && index < array_count(devs); ++index) {
        iodev_t *dev = array_get(devs, index);
        if (dev->is_set(dev, r))
            ready--, dev->read_handler(dev);
        if (dev->is_set(dev, w))
            ready--, dev->write_handler(dev);
        if (dev->is_set(dev, x))
            ready--, dev->except_handler(dev);
    }
    return rc;
}


static void
selector_debug(selector_t *selector, int result, int error, fd_set *rd_set, fd_set *wr_set, fd_set *ex_set) {
    // does nothing for now
}


////////// main selector loop //////////
//
// First argument is an initialised selector
// Loop automatically exits when:
//   - there are no iodevs to do any work for (all, if any, are marked inactive), returns 0
//   - provided non-zero timeout expires since last activity, returns 0
//   - a non-recoverable error occurred, returns -1
// timeout value is in milliseconds

int
selector_loop(selector_t *selector, unsigned long timeout) {
    int rc = 0;
    fd_set rd_set, wr_set, ex_set;

    FD_ZERO(&rd_set);
    FD_ZERO(&wr_set);
    FD_ZERO(&ex_set);

    while (rc == 0) {
        // set up fd_sets
        selector_status_t stat = selector_ioset(selector, &rd_set, &wr_set, &ex_set);
        if (stat.active_count == 0)
            break;
        struct timeval to = {
            .tv_sec = timeout / 1000L,
            .tv_usec = (unsigned)((timeout % 1000) * 1000)
        };
        int rdy = select(stat.highest_fd + 1, &rd_set, &wr_set, &ex_set, timeout == 0 ? NULL : &to);
        if (rdy > 0)
            rc = selector_dispatch(selector, rdy, &rd_set, &wr_set, &ex_set);
        else {
            if (rdy < 0) {
                int select_errno = errno;
                selector_debug(selector, rdy, select_errno, &rd_set, &wr_set, &ex_set);
                log_warning("select error(%d): %s", select_errno, strerror(select_errno));
            }
            break;
        }
    }
    return rc;
}
