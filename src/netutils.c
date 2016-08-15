//
// Created by David Nugent on 13/02/2016.
//

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>

#include "netutils.h"


#define ALLOC_MAGIC 0x49a562a



ipaddrs_t *
ipaddrs_init_hint(ipaddrs_t *addrs, char const *svc, struct addrinfo *addr) {
    if (addrs == NULL) {
        addrs = calloc(1, sizeof(ipaddrs_t));
        addrs->alloc =  ALLOC_MAGIC;
    } else
        addrs->alloc = 0;
    array_init(&addrs->na_addr, sizeof(struct sockaddr_storage), 4);
    return addrs;
}


ipaddrs_t *
ipaddrs_init(ipaddrs_t *addrs, char const *svc, int flags, int family, int socktype) {
    struct addrinfo hint = {
        .ai_flags = flags,
        .ai_family = family,
        .ai_socktype = socktype
    };
    return ipaddrs_init_hint(addrs, svc, &hint);
}


void
ipaddrs_free(ipaddrs_t *addrs) {
    if (addrs) {
        array_free(&addrs->na_addr);
        if (addrs->alloc == ALLOC_MAGIC)
            free(addrs);
    }
}


static int
netutils_error_default(char const *fmt, va_list args) {
    char fmtbuf[strlen(fmt) + 2];
    strcpy(fmtbuf, fmt);
    strcat(fmtbuf, "\n");
    return vfprintf(stderr, fmt, args);
}

static int (*netutils_errfunc)(char const *, va_list) = netutils_error_default;

void
netutils_seterrfunc(int (*errfunc)(char const *, va_list)) {
    netutils_errfunc = errfunc;
    array_seterrfunc(errfunc);
}


int
netutils_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = netutils_errfunc(fmt, args);
    va_end(args);
    return len;
}


socklen_t
sockaddr_len(struct sockaddr *addr) {
    if (addr != NULL)
        switch (addr->sa_family) {
            case AF_INET:
                return sizeof(struct sockaddr_in);
            case AF_INET6:
                return sizeof(struct sockaddr_in6);
            default:
                break;
        }
    return 0;
}

void *
sockaddr_addr(struct sockaddr *addr) {
    if (addr != NULL)
        switch (addr->sa_family) {
            case AF_INET: {
                struct sockaddr_in *sin = (void *)addr;
                return &sin->sin_addr;
            }
            case AF_INET6: {
                struct sockaddr_in6 *sin = (void *)addr;
                return &sin->sin6_addr;
            }
            default:
                break;
        }
    return NULL;
}

unsigned short
sockaddr_port(struct sockaddr *addr) {
    switch (addr->sa_family) {
        case AF_INET: {
            struct sockaddr_in *sin = (void *)addr;
            return ntohs(sin->sin_port);
        }
        case AF_INET6: {
            struct sockaddr_in6 *sin = (void *)addr;
            return ntohs(sin->sin6_port);
        }
        default:
            return 0;
    }
}


struct sockaddr *
sockaddr_dup(struct sockaddr *addr) {
    socklen_t addrlen = sockaddr_len(addr);
    if (addrlen == 0)
        return NULL;
    void * dst = calloc(sizeof(struct sockaddr_storage), 1);
    return memcpy(dst, addr, addrlen);
}



struct sockaddr *
ipaddrs_add(ipaddrs_t *addrs, struct sockaddr *addr) {
    struct sockaddr *ss = array_append(&addrs->na_addr, addr);
    return memcpy(ss, addr, sockaddr_len(addr));
}


void
ipaddrs_delete(ipaddrs_t *addrs, size_t index) {
    array_delete(&addrs->na_addr, index);
}


// IP version agnostic function to return a socket address from hostname

ipaddrs_t *
ipaddrs_resolve(char const *hostname, char const *svc, int flags, int family, int socktype) {
    struct addrinfo hints, *srvinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_flags = flags; // ? flags : hostname ? (AI_CANONNAME | AI_ADDRCONFIG) : AI_ADDRCONFIG;
    hints.ai_family = family;
    hints.ai_socktype = socktype;

    int rc = getaddrinfo(hostname, svc, &hints, &srvinfo);
    if (rc != 0) {
        netutils_error("getaddrinfo(%d) error: %s\n", rc, gai_strerror(rc));
        return NULL;
    }

    ipaddrs_t *addrs = ipaddrs_init_hint(NULL, svc, &hints);
    // iterate over results (if any) and add them to the array
    for (struct addrinfo *rp = srvinfo; rp != NULL; rp = rp->ai_next)
        ipaddrs_add(addrs, rp->ai_addr);

    freeaddrinfo(srvinfo);
    return addrs;
}


ipaddrs_t *
ipaddrs_resolve_stream(char const *hostname, char const *svc, int flags)
{
    return ipaddrs_resolve(hostname, svc, flags, AF_UNSPEC, SOCK_STREAM);
}


size_t
ipaddrs_count(ipaddrs_t *addrs) {
    return array_count(&addrs->na_addr);
}

struct sockaddr *
ipaddrs_get(ipaddrs_t *addrs, size_t index) {
    return array_get(&addrs->na_addr, index);
}


ipaddriter_t *
ipaddriter_new(ipaddriter_t *iter, ipaddrs_t *addrs) {
    if (iter == NULL) {
        iter = calloc(1, sizeof(ipaddriter_t));
        iter->alloc = ALLOC_MAGIC;
    } else
        iter->alloc = 0;
    iter->ipaddrs = addrs;
    iter->current = (size_t)-1;
    return iter;
}


ipaddriter_t
ipaddriter_create(ipaddrs_t *addrs) {
    ipaddriter_t iter;
    ipaddriter_new(&iter, addrs);
    return iter;
}


void
ipaddriter_free(ipaddriter_t *iter) {
    if (iter) {
        iter->current = (size_t)-1;
        iter->ipaddrs = NULL;
        if (iter->alloc == ALLOC_MAGIC)
            free(iter);
    }
}

size_t
ipaddriter_count(ipaddriter_t *iter) {
    return ipaddrs_count(iter->ipaddrs);
}

int
ipaddriter_hasnext(ipaddriter_t *iter) {
    return iter->current + 1 < ipaddriter_count(iter);
}

int
ipaddriter_hasprev(ipaddriter_t *iter) {
    return iter->current > 0;
}

struct sockaddr *
ipaddriter_next(ipaddriter_t *iter) {
    if (ipaddriter_hasnext(iter))
        return array_get(&iter->ipaddrs->na_addr, ++iter->current);
    return NULL;
}

struct sockaddr *
ipaddriter_prev(ipaddriter_t *iter) {
    if (ipaddriter_hasprev(iter)) {
        if (iter->current == (size_t)-1)
            iter->current = ipaddriter_count(iter);
        return array_get(&iter->ipaddrs->na_addr, --iter->current);
    }
    return NULL;
}
