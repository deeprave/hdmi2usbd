//
// Created by David Nugent on 11/02/2016.
//

#ifndef GENERIC_NETUTILS_H
#define GENERIC_NETUTILS_H

#include "array.h"

typedef struct ipaddrs_s ipaddrs_t;

struct ipaddrs_s {
    unsigned int alloc;
    array_t na_addr;
};

struct sockaddr;
struct addrinfo;

// public API

extern void netutils_seterrfunc(int (*errfunc)(char const *fmt, va_list args));

extern ipaddrs_t *ipaddrs_resolve(char const *hostname, char const *svc, int flags, int family, int socktype);
extern ipaddrs_t *ipaddrs_resolve_stream(char const *hostname, char const *svc, int flags);
extern void ipaddrs_free(ipaddrs_t *addrs);
extern size_t ipaddrs_count(ipaddrs_t *addrs);
struct sockaddr *ipaddrs_get(ipaddrs_t *addrs, size_t index);

extern int netutils_error(const char *fmt, ...) __attribute__((format (printf, 1, 2)));

// helper
extern socklen_t sockaddr_len(struct sockaddr *addr);
extern void *sockaddr_addr(struct sockaddr *addr);
extern unsigned short sockaddr_port(struct sockaddr *addr);
extern struct sockaddr *sockaddr_dup(struct sockaddr *addr);

// public internals

ipaddrs_t *ipaddrs_init_hint(ipaddrs_t *addrs, char const *svc, struct addrinfo *addr);
ipaddrs_t *ipaddrs_init(ipaddrs_t *addrs, char const *svc, int flags, int family, int socktype);
struct sockaddr *ipaddrs_add(ipaddrs_t *addrs, struct sockaddr *addr);
void ipaddrs_delete(ipaddrs_t *addrs, size_t index);

// sockaddr iterator
typedef struct ipaddriter_s ipaddriter_t;

struct ipaddriter_s {
    unsigned int alloc;
    size_t current;
    ipaddrs_t *ipaddrs;
};

extern ipaddriter_t ipaddriter_create(ipaddrs_t *addrs);
extern ipaddriter_t *ipaddriter_new(ipaddriter_t *iter, ipaddrs_t *addrs);
extern void ipaddriter_free(ipaddriter_t *iter);
extern size_t ipaddriter_count(ipaddriter_t *iter);
extern int ipaddriter_hasnext(ipaddriter_t *iter);
extern struct sockaddr *ipaddriter_next(ipaddriter_t *iter);
extern int ipaddriter_hasprev(ipaddriter_t *iter);
extern struct sockaddr *ipaddriter_prev(ipaddriter_t *iter);

#endif //GENERIC_NETUTILS_H
