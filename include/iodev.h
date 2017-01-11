//
// Created by David Nugent on 3/02/2016.
//
// Generic device type

#ifndef GENERIC_IODEV_H
#define GENERIC_IODEV_H

// common device interface

#include <stddef.h>
#include <stdarg.h>

#include "buffer.h"

enum devState {
    IODEV_NONE,             // default state
    IODEV_INACTIVE,         // closed, do not open
    IODEV_CLOSED,           // closed (queue for open)
    IODEV_CLOSING,          // closing (draining)
    IODEV_PENDING,          // waiting for open to complete
    IODEV_OPEN,             // open, inactive, idle
    IODEV_CONNECTED,        // open, active, connected or listening
    IODEV_ACTIVE            // open, active, communicating
};

enum closeFlags {
    IOFLAG_NONE = 0x00,     // no special handling
    IOFLAG_FLUSH = 0x01,    // send or drain data before close
    IOFLAG_INACTIVE = 0x02, // final close -> inactive (do not reopen)
};

typedef struct iodev_s iodev_t;
typedef struct selector_s selector_t;
typedef struct iodev_cfg_s iodev_cfg_t;
typedef struct stringstore_s stringstore_t;


struct iodev_cfg_s {
    const char *name;       // driver identifier
    void (*free_cfg)(iodev_cfg_t *);
};

iodev_cfg_t *iodev_alloc_cfg(size_t size, const char *name, void (*free_cfg)(iodev_cfg_t*));
void iodev_free_cfg(iodev_cfg_t *cfg);


struct iodev_s {

    unsigned int alloc;         // allocation marker
    iodev_cfg_t *cfg;           // device configuration
    selector_t *selector;       // selector / handler
    int fd;                     // file descriptor / socket
    int listener;               // non-zero = listener
    int state;                  // current state
    size_t bufsize;             // default buffer size (accept sockets)
    buffer_t rbuf;              // receive buffer
    buffer_t tbuf;              // transmit buffer
    stringstore_t *linebuf;     // received command line buffer

    // device control
    int (*open)(iodev_t *dev);
    void (*close)(iodev_t *dev, int flags);
    int (*configure)(iodev_t *dev, void *data);

    // raw I/O
    int (*set_masks)(iodev_t *dev, fd_set *rs, fd_set *ws, fd_set *xs);
    int (*is_set)(iodev_t *dev, fd_set *fds);
    ssize_t (*read_handler)(iodev_t *dev);
    ssize_t (*write_handler)(iodev_t *dev);
    ssize_t (*except_handler)(iodev_t *dev);

    // buffered I/O
    ssize_t (*read)(iodev_t *dev, void *buf, size_t len);
    ssize_t (*write)(iodev_t *dev, void const *buf, size_t len);

    int (*sendOk)(iodev_t *dev);
};


extern iodev_cfg_t *iodev_getcfg(iodev_t *iodev);
extern const char *iodev_driver(iodev_t *iodev);

extern int iodev_getfd(iodev_t *dev);
extern int iodev_is_listener(iodev_t *dev);
extern int iodev_is_open(iodev_t *dev);

extern int iodev_getstate(iodev_t *dev);
extern int iodev_setstate(iodev_t *dev, int state);

extern buffer_t *iodev_tbuf(iodev_t *dev);
extern buffer_t *iodev_rbuf(iodev_t *dev);
extern stringstore_t *iodev_stringstore(iodev_t *dev);

extern selector_t *getselector(iodev_t *dev);
extern void setselector(iodev_t *dev, selector_t *selector);

extern void iodev_seterrfunc(int (*func)(char const *fmt, va_list args));
extern void iodev_setnotify(int (*func)(char const *fmt, va_list args));

extern int iodev_error(char const *fmt, ...) __attribute__((format (printf, 1, 2)));
extern int iodev_notify(char const *fmt, ...) __attribute__((format (printf, 1, 2)));

extern iodev_t *iodev_init(iodev_t *dev, iodev_cfg_t *cfg, size_t bufsize);
extern void iodev_free(iodev_t *dev);

extern ssize_t iodev_write(iodev_t *dev, void const *buf, size_t len);
extern ssize_t iodev_read(iodev_t *dev, void *buf, size_t len);

#endif //GENERIC_IODEV_H
