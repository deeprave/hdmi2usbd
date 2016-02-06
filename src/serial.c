//
// Created by David Nugent on 31/01/2016.
//

#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "serial.h"

// Miscellaneous serial functions

// baud rate mapping, note that we only support a subset
static unsigned long baudmap[][2] = {
    {B300,     300      },
    {B1200,    1200     },
    {B2400,    2400     },
    {B4800,    4800     },
    {B9600,    9600     },
    {B19200,   19200    },
    {B38400,   38400    },
#if defined(B57600)
    {B57600,   57600    },
#endif
#if defined(B115200)
    {B115200,  115200   },
#endif
#if defined(B230400)
    {B230400,  230400   },
#endif
#if defined(B460800)
    {B460800,  460800   },
#endif
#if defined(B576000)
    {B576000,  576000   },
#endif
#if defined(B921600)
    {B921600,  921600   },
#endif
    {B0,       0        }
};

// System specific speed to baud translations
// Baud rate values are those define for serial ports for the host OS

unsigned long
speed_to_baud(unsigned long speed) {
    for (int i =0; baudmap[i][1] != 0; i++)
        if (baudmap[i][1] == speed)
            return baudmap[i][0];
    return BINVALID;
}

unsigned long
baud_to_speed(unsigned long baud) {
    for (int i =0; baudmap[i][1] != 0; i++)
        if (baudmap[i][0] == baud)
            return baudmap[i][1];
    return BINVALID;
}


// serial_dev_t support

serial_cfg_t *
serial_getcfg(iodev_t *sdev) {
    return (serial_cfg_t *)iodev_getcfg(sdev);
}


// device control
static int
serial_open(iodev_t *dev) {
    serial_cfg_t *cfg = serial_getcfg(dev);

    if (iodev_getstate(dev) >= IODEV_OPEN)
        iodev_close(dev, IOFLAG_NONE);

    int opts = O_NONBLOCK | O_NOCTTY | O_RDWR;
    dev->fd = open(cfg->portname, opts);
    if (dev->fd == -1) { // failed
        iodev_error(dev, "serial open '%s' (%d): %s", cfg->portname, errno, strerror(errno));
    } else {
        iodev_setstate(dev, IODEV_OPEN);
        if (tcsetattr(dev->fd, TCSANOW, cfg->termctl) == -1)
            iodev_error(dev, "serial setup '%s' (%d): %s", cfg->portname, errno, strerror(errno));
        else {
            if (ioctl(dev->fd, TIOCCBRK) == -1)
                iodev_notify(dev, "ioctl(TIOCCBRK) '%s' (%d): %s", cfg->portname, errno, strerror(errno));
            // set serial devices directly to "connected" state after successfully opened
            iodev_setstate(dev, IODEV_CONNECTED);
            return dev->fd;
        }
        // error fallthrough
        iodev_close(dev, IOFLAG_NONE);
    }
    return dev->fd;
}


static void
serial_close(iodev_t *dev, int flags) {
    serial_cfg_t *cfg = serial_getcfg(dev);

    switch (iodev_getstate(dev)) {
        case IODEV_NONE:
        case IODEV_INACTIVE:
        case IODEV_CLOSED:
            iodev_notify(dev, "serial close '%s' already closed", cfg->portname);
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
                tcflush(dev->fd, TCOFLUSH);
            }
        case IODEV_PENDING: // never flush if only pending
            close(dev->fd);
            dev->fd = -1;
            iodev_setstate(dev, IODEV_CLOSED);
            break;
    }
}


static int
serial_configure(iodev_t *dev, void *data) {
    return 0;
}


static int
serial_read_handler(iodev_t *dev) {
    return 0;
}


static int
serial_write_handler(iodev_t *dev) {
    return 0;
}


static int
serial_except_handler(iodev_t *dev) {
    return 0;
}


static int
serial_read(iodev_t *dev, void *buf, size_t len) {
    return 0;
}


static int
serial_write(iodev_t *dev, void const *buf, size_t len) {
    return 0;
}


iodev_t *
serial_create(char const *devname, unsigned long baudrate, size_t bufsize) {
    // First create the basic (slightly larger) config
    iodev_cfg_t *cfg = alloc_cfg(sizeof(serial_cfg_t), "serial", 0);
    iodev_t *serial = iodev_create(cfg, bufsize);

    // Initialise the extras
    serial_cfg_t *scfg = serial_getcfg(serial);
    scfg->portname = devname;
    scfg->baudrate = baudrate;

    // Set up the initial termios
    struct termios *termctl = calloc(1, sizeof(struct termios));
    cfmakeraw(termctl);
    cfsetospeed(termctl, (speed_t)scfg->baudrate);
    cfsetispeed(termctl, (speed_t)scfg->baudrate);
    termctl->c_cflag &= ~(CSTOPB);
    termctl->c_cflag &= ~(CSIZE);
    termctl->c_cflag |= CS8;
    termctl->c_cflag &= ~(PARENB);
    termctl->c_cflag &= ~(CLOCAL);
    termctl->c_cflag &= ~(HUPCL);
    termctl->c_cflag |= CREAD;
    termctl->c_cflag &= ~(CRTSCTS);
    termctl->c_iflag &= ~(IXON | IXOFF | IXANY);
    termctl->c_iflag |= IGNBRK;
    scfg->termctl = termctl;

    serial->open = serial_open;
    serial->close = serial_close;
    serial->configure = serial_configure;
    serial->read_handler = serial_read_handler;
    serial->write_handler = serial_write_handler;
    serial->except_handler = serial_except_handler;
    serial->read = serial_read;
    serial->write = serial_write;

    return serial;
}
