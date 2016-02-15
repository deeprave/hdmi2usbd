//
// Created by David Nugent on 14/02/2016.
//

#ifndef GENERIC_DEVICE_H
#define GENERIC_DEVICE_H

// controller serial device llist node

struct ctrldev {
    struct ctrldev *next;
    char *devname;
};

extern struct ctrldev *find_serial_all(char const *filespec);
extern void ctrldev_free(struct ctrldev *first_dev);

extern char *find_serial(char const *filespec);

#endif //GENERIC_DEVICE_H
