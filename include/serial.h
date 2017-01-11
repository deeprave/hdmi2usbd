//
// Created by David Nugent on 31/01/2016.
//

#ifndef GENERIC_SERIAL_H
#define GENERIC_SERIAL_H

#define BINVALID ((unsigned long)-1)

unsigned long baud_to_speed(unsigned long baud);
unsigned long speed_to_baud(unsigned long speed);

#include "iodev.h"
#include "timer.h"

typedef struct serial_cfg_s serial_cfg_t;

struct serial_cfg_s {
    iodev_cfg_t cfg;
    char const *portname;
    unsigned long baudrate;
    struct termios *termctl;
    microtimer_t pacer;
};

extern serial_cfg_t *serial_getcfg(iodev_t *sdev);
extern iodev_t *serial_create(iodev_t *dev, char const *devname, unsigned long baudrate, size_t bufsize);

#endif //GENERIC_SERIAL_H
