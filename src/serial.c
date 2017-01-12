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

#define CHARACTER_PACING   5000

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


static struct termios *
serial_termios(struct termios *termctl, speed_t baudrate) {
    // Raw mode (input and output processing disabled)
    cfmakeraw(termctl);
    termctl->c_lflag &= ~(ECHO|ECHONL|ECHOE|ECHOK|ICANON|ISIG|IEXTEN);
    termctl->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF|IXANY);
    // Disable parity, 8N1, flow control disabled
    termctl->c_cflag &= ~(CSTOPB|CSIZE|PARENB|HUPCL|CRTSCTS);
    termctl->c_cflag |= CS8|CREAD|CLOCAL;
    termctl->c_iflag |= IGNBRK;
    termctl->c_oflag &= ~OPOST;
    // Set the input and output baudrate
    cfsetospeed(termctl, baudrate);
    cfsetispeed(termctl, baudrate);
    return termctl;
}


// device control
static int
serial_open(iodev_t *dev) {
    serial_cfg_t *cfg = serial_getcfg(dev);

    if (iodev_getstate(dev) >= IODEV_OPEN)
        dev->close(dev, IOFLAG_NONE);

    int opts = O_NONBLOCK | O_NOCTTY | O_RDWR;
    dev->fd = open(cfg->portname, opts);
    if (dev->fd == -1) { // failed
        iodev_error("serial open '%s' (%d): %s", cfg->portname, errno, strerror(errno));
        iodev_setstate(dev, IODEV_INACTIVE);
    } else {
        iodev_setstate(dev, IODEV_OPEN);
        if (tcgetattr(dev->fd, cfg->termctl) == -1)
            iodev_error("serial tcgetattr '%s' (%d): %s", cfg->portname, errno, strerror(errno));
        else {
            serial_termios(cfg->termctl, cfg->baudrate);
            if (tcsetattr(dev->fd, TCSANOW, cfg->termctl) == -1)
                iodev_error("serial tcsetattr '%s' (%d): %s", cfg->portname, errno, strerror(errno));
            else {
#if 1
                ioctl(dev->fd, TIOCCBRK);   // ignore errors
#else
                if (ioctl(dev->fd, TIOCCBRK) == -1)
                    iodev_notify("ioctl(TIOCCBRK) '%s' (%d): %s", cfg->portname, errno, strerror(errno));
#endif
                // set serial devices directly to "connected" state after successfully opened
                iodev_setstate(dev, IODEV_CONNECTED);
                return dev->fd;
            }
        }
        // error fallthrough
        dev->close(dev, IOFLAG_NONE);
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
            iodev_notify("serial close: %s is already closed", cfg->portname);
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

static ssize_t
serial_write_handler(iodev_t *dev) {
    ssize_t rc = -1;
    iodev_cfg_t *cfg = iodev_getcfg(dev);
    serial_cfg_t *scfg = serial_getcfg(dev);

    if (dev->fd == -1)
        iodev_error("iodev %s write error: device is closed", cfg->name);
    else {
        size_t available = buffer_used(&dev->tbuf);
        if (!available || !timer_expired(&scfg->pacer))
            rc = available;
        else { // write 1 character at a time...
            unsigned char ch;
            buffer_peek(&dev->tbuf, &ch, 1);
            rc = write(dev->fd, &ch, 1);
            if (rc < 0) {
                iodev_error("iodev %s write error(%d): %s", cfg->name, errno, strerror(errno));
                dev->close(dev, IODEV_NONE);
            } else { // advance the counter by amount written
                buffer_get(&dev->tbuf, NULL, (size_t)rc);
                timer_reset(&scfg->pacer, CHARACTER_PACING);
            }
        }
    }
    return rc;
}


static int
serial_configure(iodev_t *dev, void *data) {
    return 0;
}

static int
serial_sendok(iodev_t *dev) {
    serial_cfg_t *scfg = serial_getcfg(dev);
    return timer_expired(&scfg->pacer);
}

iodev_t *
serial_create(iodev_t *dev, char const *devname, unsigned long baudrate, size_t bufsize) {
    // First create the basic (slightly larger) config
    iodev_cfg_t *cfg = iodev_alloc_cfg(sizeof(serial_cfg_t), "serial", NULL);
    iodev_t *serial = iodev_init(dev, cfg, bufsize);

    // Initialise the extras
    serial_cfg_t *scfg = serial_getcfg(serial);
    scfg->portname = devname;
    scfg->baudrate = baudrate;

    // Set up the initial termios
    scfg->termctl = serial_termios(calloc(1, sizeof(struct termios)), (speed_t)baudrate);
    timer_reset(&scfg->pacer, 0);   // creates a pre-expired timer

    serial->open = serial_open;
    serial->close = serial_close;
    serial->write_handler = serial_write_handler;
    serial->configure = serial_configure;
    serial->sendOk = serial_sendok;

    return serial;
}
