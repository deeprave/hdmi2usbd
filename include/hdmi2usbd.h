//
// Created by David Nugent on 31/01/2016.
//

#ifndef HDMI2USBD_HDMI2USBD_H
#define HDMI2USBD_HDMI2USBD_H

#define HDMI2USBD_VERSION "0.1"
#define HDMI2USBD_NAME "hdmi2usbd"

#define MAX_SERIAL_SPECS 31


// configuration data
struct hdmi2usb_opts {
    int verbose;
    int logflags;
    char const *logfile;
    int daemonize;
    long baudrate;
    char const *port;
    char const *listen_addr;
    short listen_port;
};

// working data
struct hdmi2usb {
    struct hdmi2usb_opts opts;
};


// controller serial device llist node
struct ctrldev {
    struct ctrldev *next;
    char *devname;
};

struct ctrldev *find_serial_all(const struct hdmi2usb_opts *opts);
void ctrldev_free(struct ctrldev *first_dev);
char *find_serial(const struct hdmi2usb_opts *opts);

#endif //HDMI2USBD_HDMI2USBD_H
