//
// Created by David Nugent on 2/02/2016.
//

#ifndef GENERIC_NETTCP_H
#define GENERIC_NETTCP_H

#include "iodev.h"

typedef struct tcp_cfg_s tcp_cfg_t;

struct tcp_cfg_s {
    iodev_cfg_t cfg;
    struct sockaddr *local;
    struct sockaddr *remote;
};

extern tcp_cfg_t *tcp_getcfg(iodev_t *sdev);
extern iodev_t *tcp_create_listen(struct sockaddr *local);
extern iodev_t *tcp_create_connect(struct sockaddr *remote, size_t bufsize);

#endif //GENERIC_NETTCP_H
