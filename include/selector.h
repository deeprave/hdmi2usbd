//
// Created by David Nugent on 2/02/2016.
//
// Generic selector type for registering filehandles in a non-blocking select(2)

#ifndef GENERIC_SELECTOR_H
#define GENERIC_SELECTOR_H

#include "array.h"
#include "iodev.h"

typedef struct selector_s selector_t;

struct selector_s {
    int alloc;
    array_t devs;
};

struct sockaddr;

extern selector_t *selector_init(selector_t *selector);
extern void selector_free(selector_t *selector);
extern size_t selector_device_count(selector_t *selector);
extern iodev_t *selector_get_device(selector_t *selector, size_t index);
extern void selector_add_device(selector_t *selector, iodev_t *dev);
extern iodev_t *selector_set(selector_t *selector, iodev_t *dev);

extern iodev_t *selector_new_device_serial(selector_t *selector, char const *devname, unsigned long baudrate, unsigned bufsize);
extern iodev_t *selector_new_device_listen(selector_t *selector, struct sockaddr *local, unsigned bufsize);
extern iodev_t *selector_new_device_connect(selector_t *selector, struct sockaddr *remote, unsigned bufsize);
extern iodev_t *selector_new_device_accepted(selector_t *selector, int fd, unsigned bufsize);

extern int selector_loop(selector_t *selector, unsigned long timeout);



#endif //GENERIC_SELECTOR_H
