//
// Created by David Nugent on 31/01/2016.
//

#ifdef __APPLE__
// borrowed from http://www.opensource.apple.com/source/mDNSResponder/mDNSResponder-258.18/mDNSPosix/PosixDaemon.c
// In Mac OS X 10.5 and later trying to use the daemon function gives a “‘daemon’ is deprecated”
// error, which prevents compilation because we build with "-Werror".
// Since this is supposed to be portable cross-platform code, we don't care that daemon is
// deprecated on Mac OS X 10.5, so we use this preprocessor trick to eliminate the error message.
#define daemon yes_we_know_that_daemon_is_deprecated_in_os_x_10_5_thankyou
#endif

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/errno.h>

#ifdef __APPLE__
#undef daemon
extern int daemon(int, int);
#endif


#include "hdmi2usbd.h"
#include "logging.h"
#include "device.h"
#include "netutils.h"


//// Logging interface ////


int
hdmi2usb_error(char const *fmt, va_list arg) {
    return log_log(V_ERROR, fmt, arg);
}

int
hdmi2usb_notify(char const *fmt, va_list arg) {
    return log_log(V_INFO, fmt, arg);
}


//// signal handling ////

static volatile unsigned short signal_received = 0;

static void
break_handler(int sig) {
    // try graceful exit first, but quit immediately for double ctrl-c
    if (sig == SIGINT && signal_received == SIGINT) {
        fputs("Keyboard Quit\n", stderr);
        exit(31);
    }
    signal_received = (unsigned short)(sig & 0xffffffff);
}

static int sigvec_index = 0;

#define MAX_SIGVEC  7
static struct {
    int sig;
    void (*handler)(int);
} sigstack[MAX_SIGVEC];

static int
push_sighandler(int sig, void (*new_handler)(int)) {
    sigstack[sigvec_index].sig = sig;
    sigstack[sigvec_index].handler = signal(sig, new_handler);
    return sigvec_index < MAX_SIGVEC ? ++sigvec_index : sigvec_index;
}

static int
pop_sighandler() {
    if (sigvec_index) {
        --sigvec_index;
        signal(sigstack[sigvec_index].sig, sigstack[sigvec_index].handler);
    }
    return sigvec_index;
}

static int nochdir = 1;
static int noclose = 1;

//// init and close functions ////

static int
hdmi2usb_init(struct hdmi2usb *app, int rc) {
    // Redirect generic module error & notification messages to the logger
    iodev_seterrfunc(hdmi2usb_error);
    iodev_setnotify(hdmi2usb_notify);
    netutils_seterrfunc(hdmi2usb_error);
// array_seterrfunc(hdmi2usb_error); // autoset by netutils (which uses array)
    // signal handlers
    push_sighandler(SIGHUP, break_handler);
    push_sighandler(SIGINT, break_handler);
    // initialise selector, set up serial device and network listeners
    selector_init(&app->selector);
    // first, the serial device. We need to exit if we can't open this one
    char *port = find_serial(app->opts.port);
    if (port == NULL) {
        log_critical("No available serial device matching '%s'", app->opts.port);
        rc = 2;
    } else {
        log_debug("Selected serial port %s baud %lu bufsize %u", port, app->opts.baudrate, app->opts.iobufsize);
        selector_new_device_serial(&app->selector, port, app->opts.baudrate, app->opts.iobufsize);
        // Set up our listen port(s)
        // Also need to exit with error message if it fails
        unsigned listen_ports = 0;
        char buf[64];
        snprintf(buf, sizeof(buf) - 1, "%u", app->opts.listen_port);
        ipaddrs_t *addrs = ipaddrs_resolve_stream(app->opts.listen_addr, buf, app->opts.listen_flags);
        for (ipaddriter_t iter = ipaddriter_create(addrs); ipaddriter_hasnext(&iter); ) {
            struct sockaddr *addr = ipaddriter_next(&iter);
            switch (addr->sa_family) {
                case AF_INET: {
                    struct sockaddr_in *s4 = (struct sockaddr_in *) addr;
                    if (s4->sin_addr.s_addr == INADDR_ANY) {
                        listen_ports |= 4;
                    } else if ((listen_ports & 1) == 0) {  // create ipv4 listen socket
                        inet_ntop(addr->sa_family, sockaddr_addr(addr), buf, sizeof(buf) - 1);
                        log_debug("Listening on IPv4 address %s", buf);
                        app->listen[0] = selector_new_device_listen(&app->selector, addr, app->opts.iobufsize);
                        listen_ports |= 1;
                    }
                    break;
                }
                case AF_INET6: {
                    struct sockaddr_in6 *s6 = (struct sockaddr_in6 *) addr;
                    struct in6_addr in6addr = IN6ADDR_ANY_INIT;
                    if (IN6_ARE_ADDR_EQUAL(&s6->sin6_addr, &in6addr)) {
                        listen_ports |= 4;
                    } else if ((listen_ports & 2) == 0) {   // create ipv6 listen socket
                        inet_ntop(addr->sa_family, sockaddr_addr(addr), buf, sizeof(buf) - 1);
                        log_debug("Listening on IPv6 address %s", buf);
                        app->listen[1] = selector_new_device_listen(&app->selector, addr, app->opts.iobufsize);
                        listen_ports |= 2;
                    }
                    break;
                }
                default:
                    continue;
            }
            if (listen_ports & 4) {
                log_debug("Listening on ALL interfaces port %u", sockaddr_port(addr));
                app->listen[0] = selector_new_device_listen(&app->selector, addr, app->opts.iobufsize);
                break;
            } else if (listen_ports == 3)
                break;
        }
        ipaddrs_free(addrs);

        if (rc == 0 && app->opts.daemonize) {
            if (daemon(nochdir, noclose) == -1)
                log_warning("daemon() failed(%d): %s", errno, strerror(errno));
            app->opts.daemonize = 0;    // only do this once
        }
    }
    return rc;
}


static int
hdmi2usb_close(struct hdmi2usb *app, int rc) {
    while (pop_sighandler())
        ;
    return rc;
}


//// main outide loop ////

int
hdmi2usb_main(struct hdmi2usb *app) {
    int rc = hdmi2usb_init(app, 0);
    while (rc == 0) {
        rc = selector_loop(&app->selector, app->opts.loop_time);
        // Main loop
        switch (signal_received) {
            case SIGHUP:
                log_critical("Reloading on SIGHUP");
                rc = hdmi2usb_init(app, hdmi2usb_close(app, 0));
                break;
            case SIGINT:
                log_critical("Keyboard Quit");
                rc = 3;
                break;
            default:

                break;
        }
    }
    return hdmi2usb_close(app, rc);
}

