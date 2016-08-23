//
// Created by David Nugent on 2/02/2016.
//

#ifndef GENERIC_NETTCP_H
#define GENERIC_NETTCP_H

#include "iodev.h"

typedef struct tcp_cfg_s tcp_cfg_t;

struct tcp_cfg_s {
    iodev_cfg_t cfg;
    socklen_t addrlen;          // length of address info
    struct sockaddr *local;
    struct sockaddr *remote;
};

extern tcp_cfg_t *tcp_getcfg(iodev_t *sdev);

extern iodev_t *tcp_create_listen(iodev_t *dev, struct sockaddr *local, size_t bufsize);
extern iodev_t *tcp_create_accepted(iodev_t *dev, int fd, struct sockaddr *remote, size_t bufsize);
extern iodev_t *tcp_create_connect(iodev_t *dev, struct sockaddr *remote, size_t bufsize);

#endif //GENERIC_NETTCP_H
