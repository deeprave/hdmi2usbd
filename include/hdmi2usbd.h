//
// Created by David Nugent on 31/01/2016.
//

#ifndef HDMI2USBD_HDMI2USBD_H
#define HDMI2USBD_HDMI2USBD_H

#include "selector.h"
#include "timer.h"

#define HDMI2USBD_VERSION "1.0"
#define HDMI2USBD_NAME "hdmi2usbd"

#define MAX_SERIAL_SPECS 31

enum {
    EX_SUCCESS,
    EX_NORMAL,
    EX_REQUEST,
    EX_STARTUP,
    EX_FAILURE,
    EX_CRITICAL,
    EX_FATAL,
};


// configuration data
struct hdmi2usb_opts {
    int verbose;
    int logflags;
    char const *logfile;
    int daemonize;
    unsigned long baudrate;
    char const *port;
    char const *listen_addr;
    unsigned short listen_port;
    int listen_flags;
    unsigned iobufsize;
    unsigned long loop_time;
    unsigned long command_time;
};

typedef unsigned long millitime_t;

// working data
struct hdmi2usb {
    struct hdmi2usb_opts opts;
    selector_t selector;        // selector (including device array)
    buffer_t proc;              // serial input (pre-processing)
    buffer_t copy;              // output to network connections (post-processing)
    microtimer_t last_command;       // timestamp of last command
};


int hdmi2usb_main(struct hdmi2usb *app);

#endif //HDMI2USBD_HDMI2USBD_H
