//
// Created by David Nugent on 3/02/2016.
//

#include <iostream>
#include "gtest/gtest.h"

extern "C" {
//#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "netutils.h"
#include "logging.h"
}

namespace {

    static int
    ipaddrs_errors(char const *fmt, va_list args) {
        return log_log(V_INFO, fmt, args);
    }

    // Test fixture
    class IpaddrsFunctions : public ::testing::Test {

    public:
        static char const *hostnames[];

        void SetUp() {
            netutils_seterrfunc(ipaddrs_errors);
        }

        void TearDown() {

        }

    };

    char const *IpaddrsFunctions::hostnames[] = {
            NULL,
            "localhost",
            "www.google.com",
            "www.google.com.au",
            "www.microsoft.com",
            "www.google.co.nz",
            "example.com",
            "github.com",
            "198.142.186.242",
            "2404:6800:4003:c01::93",
            NULL
    };

    TEST_F(IpaddrsFunctions, ipAddrInitAutoAndFree) {
        // Create an ipaddrs_t object on the stack
        ipaddrs_t ipaddrs;
        ipaddrs_t *myipaddrs = ipaddrs_init(&ipaddrs, NULL, 0, AF_UNSPEC, SOCK_STREAM);
        ASSERT_EQ(myipaddrs, &ipaddrs);
        ipaddrs_free(myipaddrs);
        // Now create the same thing on the heap
        myipaddrs = ipaddrs_init(NULL, NULL, 0, AF_UNSPEC, SOCK_STREAM);
        ASSERT_NE(myipaddrs, &ipaddrs);
        ipaddrs_free(myipaddrs);
    }

    TEST_F(IpaddrsFunctions, ipAddrAdd) {
        struct addrinfo hints = {
            .ai_flags = AI_PASSIVE,
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
        };
        struct addrinfo *ainfo;
        for (size_t hostindex = 0; hostindex == 0 || hostnames[hostindex]; ++hostindex) {
            // don't normally need to do this, but we need to handle this manually for testing
            ipaddrs_t *ipaddrs = ipaddrs_init_hint(NULL, NULL, &hints);
            const char *hostname = hostnames[hostindex];
            int rc = getaddrinfo(hostname, hostname == NULL ? "http" : NULL, &hints, &ainfo);
            if (rc != 0) {
                netutils_error("getaddrinfo(%s) error %d: %s", hostname, rc, gai_strerror(rc));
            } else {
                char buf[64];
                int index = 0;
                log_info("Hostname = %s", hostname);
                for (struct addrinfo *ai = ainfo; ai != NULL; ai = ai->ai_next, ++index) {
                    log_info("[%02d] %s port=%d flags=%d family=%d type=%d proto=%d canonical=%s", index+1,
                             inet_ntop(ai->ai_family, sockaddr_addr(ai->ai_addr), buf, sizeof(buf) - 1),
                             sockaddr_port(ai->ai_addr), ai->ai_flags, ai->ai_family, ai->ai_socktype,
                             ai->ai_protocol, ai->ai_canonname ? ai->ai_canonname : "NULL");
                    struct sockaddr *s = ipaddrs_add(ipaddrs, ai->ai_addr);
                    EXPECT_EQ(s->sa_family, ai->ai_addr->sa_family);
                    EXPECT_EQ(sockaddr_len(s), sockaddr_len(ai->ai_addr));
                    EXPECT_TRUE(memcmp(s->sa_data, ai->ai_addr->sa_data, ai->ai_addrlen) == 0);
                }
                freeaddrinfo(ainfo);
                log_info("^^^^ returned: %d stored: %lu ^^^^", index, ipaddrs_count(ipaddrs));
            }
            ipaddrs_free(ipaddrs);
        }
    }

    TEST_F(IpaddrsFunctions, ipAddrDelete) {
        struct addrinfo hints = {
                .ai_flags = AI_PASSIVE,
                .ai_family = AF_UNSPEC,
                .ai_socktype = SOCK_STREAM,
        }, *ainfo;
        for (size_t hostindex = 0; hostindex == 0 || hostnames[hostindex]; ++hostindex) {
            // don't normally need to do this, but we need to handle this manually for testing
            ipaddrs_t *ipaddrs = ipaddrs_init_hint(NULL, NULL, &hints);
            const char *hostname = hostnames[hostindex];
            size_t index = 0;
            int rc = getaddrinfo(hostname, hostname == NULL ? "http" : NULL, &hints, &ainfo);
            if (rc != 0) {
                netutils_error("getaddrinfo(%s) error %d: %s", hostname, rc, gai_strerror(rc));
                EXPECT_EQ(0, rc);
            } else {
                for (struct addrinfo *ai = ainfo; ai != NULL; ai = ai->ai_next, index++)
                    ipaddrs_add(ipaddrs, ai->ai_addr);
                freeaddrinfo(ainfo);
            }
            size_t count = ipaddrs_count(ipaddrs);
            EXPECT_EQ(index, count);
            while (ipaddrs_count(ipaddrs)) {
                ipaddrs_delete(ipaddrs, 0);
                count--;
                EXPECT_EQ(count, ipaddrs_count(ipaddrs));
            }
            ipaddrs_free(ipaddrs);
        }
    }

    TEST_F(IpaddrsFunctions, ipAddrResolve) {
        // Simple higher level interface, resolve a hostname to address
        for (size_t hostindex = 0; hostindex == 0 || hostnames[hostindex]; ++hostindex) {
            const char *hostname = hostnames[hostindex];
            log_info("Hostname = %s", hostname);
            ipaddrs_t *addrs = ipaddrs_resolve_stream(hostname, hostname == NULL ? "telnet" : NULL, 0);
            EXPECT_NE((size_t)0, ipaddrs_count(addrs));
            for (size_t index = 0; index < ipaddrs_count(addrs); index++) {
                struct sockaddr *s = ipaddrs_get(addrs, index);
                EXPECT_NE((struct sockaddr *)0, s);
                char buf[64];
                log_info("[%02lu] %s port=%d family=%d", index+1,
                         inet_ntop(s->sa_family, sockaddr_addr(s), buf, sizeof(buf) - 1),
                         sockaddr_port(s), s->sa_family);
            }
        }
    }

    TEST_F(IpaddrsFunctions, ipAddrIterator) {
        // Same as above, just using iterators
        for (size_t hostindex = 0; hostindex == 0 || hostnames[hostindex]; ++hostindex) {
            const char *hostname = hostnames[hostindex];
            log_info("Hostname = %s", hostname);
            ipaddrs_t *addrs = ipaddrs_resolve_stream(hostname, hostname == NULL ? "telnet" : NULL, 0);
            EXPECT_NE((size_t)0, ipaddrs_count(addrs));
            // forward direction
            ipaddriter_t *iter = ipaddriter_new(NULL, addrs);
            EXPECT_TRUE(ipaddriter_hasnext(iter));
            while (ipaddriter_hasnext(iter)) {
                struct sockaddr *s = ipaddriter_next(iter);
                EXPECT_NE((struct sockaddr *)0, s);
                char buf[64];
                log_info("> %s port=%d family=%d", inet_ntop(s->sa_family, sockaddr_addr(s), buf, sizeof(buf) - 1),
                         sockaddr_port(s), s->sa_family);
            }
            EXPECT_FALSE(ipaddriter_hasnext(iter));
            ipaddriter_free(iter);
            // backwards, use an
            ipaddriter_t it = ipaddriter_create(addrs);
            EXPECT_TRUE(ipaddriter_hasprev(&it));
            while (ipaddriter_hasprev(&it)) {
                struct sockaddr *s = ipaddriter_prev(&it);
                EXPECT_NE((struct sockaddr *)0, s);
                char buf[64];
                log_info("< %s port=%d family=%d", inet_ntop(s->sa_family, sockaddr_addr(s), buf, sizeof(buf) - 1),
                         sockaddr_port(s), s->sa_family);
            }
            EXPECT_FALSE(ipaddriter_hasprev(&it));
            ipaddriter_free(&it);
        }
    }

} // namespace
