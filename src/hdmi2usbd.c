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
#include <signal.h>
#include <sys/time.h>

#ifdef __APPLE__
#undef daemon
extern int daemon(int, int);
#else
#include <unistd.h>
#endif

#define COMMAND_PACE 500000       // 500ms == 500000us


#include "hdmi2usbd.h"
#include "logging.h"
#include "device.h"
#include "netutils.h"
#include "stringstore.h"


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
} signal_stack[MAX_SIGVEC];

static int
push_sighandler(int sig, void (*new_handler)(int)) {
    signal_stack[sigvec_index].sig = sig;
    signal_stack[sigvec_index].handler = signal(sig, new_handler);
    return sigvec_index < MAX_SIGVEC ? ++sigvec_index : sigvec_index;
}

static int
pop_sighandler() {
    if (sigvec_index) {
        --sigvec_index;
        signal(signal_stack[sigvec_index].sig, signal_stack[sigvec_index].handler);
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
        rc = EX_STARTUP;
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
                    struct sockaddr_in *s4 = (void *) addr;
                    if (s4->sin_addr.s_addr == INADDR_ANY) {
                        listen_ports |= 4;
                    } else if ((listen_ports & 1) == 0) {  // create ipv4 listen socket
                        inet_ntop(addr->sa_family, sockaddr_addr(addr), buf, sizeof(buf) - 1);
                        log_debug("Listening on IPv4 address %s", buf);
                        selector_new_device_listen(&app->selector, addr, app->opts.iobufsize);
                        listen_ports |= 1;
                    }
                    break;
                }
                case AF_INET6: {
                    struct sockaddr_in6 *s6 = (void *) addr;
                    struct in6_addr in6addr = IN6ADDR_ANY_INIT;
                    if (IN6_ARE_ADDR_EQUAL(&s6->sin6_addr, &in6addr)) {
                        listen_ports |= 4;
                    } else if ((listen_ports & 2) == 0) {   // create ipv6 listen socket
                        inet_ntop(addr->sa_family, sockaddr_addr(addr), buf, sizeof(buf) - 1);
                        log_debug("Listening on IPv6 address %s", buf);
                        selector_new_device_listen(&app->selector, addr, app->opts.iobufsize);
                        listen_ports |= 2;
                    }
                    break;
                }
                default:
                    continue;
            }
            if (listen_ports & 4) {
                log_debug("Listening on ALL interfaces port %u", sockaddr_port(addr));
                selector_new_device_listen(&app->selector, addr, app->opts.iobufsize);
                break;
            } else if (listen_ports == 3)
                break;
        }
        ipaddrs_free(addrs);

        if (rc == EX_SUCCESS && app->opts.daemonize) {
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


static size_t
hdmi2usb_process_serial_data(struct hdmi2usb *app, iodev_t *serial) {
    size_t s_bytes = iodev_is_open(serial) ? buffer_used(iodev_rbuf(serial)) : 0;
    if (s_bytes) {
        // process the datas here
        //... TODO (maybe?)
        // pick up anything left over and queue for output to network connections
        s_bytes = buffer_move(&app->copy, iodev_rbuf(serial), s_bytes);
    }
    return s_bytes;
}

//
// hdmi2usb_process_client_data()
// read pending input from network connections and buffer this
// for later processing in hdmi2usb_process_client_commands()
// we don't send this directly/immediately because there are
// limitations on the number of commands we can process at once
// and the rate at which they can be processed.

static void
hdmi2usb_process_client_data(struct hdmi2usb *app, iodev_t *dev) {
    buffer_t *rbuf = iodev_rbuf(dev);
    size_t r_bytes = buffer_used(rbuf);
    if (r_bytes) {
        stringstore_t *linebuf = iodev_stringstore(dev);
        if (linebuf) {
            void *data = alloca(r_bytes);
            r_bytes = buffer_get(rbuf, data, r_bytes);
            stringstore_append(linebuf, data, r_bytes);
        }
    }
}

//
// hdmi2usb_process_client_commands()
// read lines of text from the connection's input buffer and
// send these to the hdmi2usb device
// commands send across all connections need to be rate limited as
// some of these will result in the device not accepting input for
// a short period when the command is executed.

static void
hdmi2usb_process_client_commands(struct hdmi2usb *app, iodev_t *serial, iodev_t *dev) {
    // Skip even checking unless it is time to send another command
    if (timer_expired(&app->last_command)) {
        // check we are have commands to send to this device
        stringstore_t *linebuf = dev->linebuf;
        if (linebuf != NULL && stringstore_length(linebuf) > 0) {
            stringstore_iterator_t iter = stringstore_iterator(linebuf);
            size_t length =0;
            // Get the next command (if there is one)
            char const *command = stringstore_nextstr(&iter, &length);
            if (command != NULL && length > 0) {
                // There is one: send it to the serial device
                iodev_write(serial, command, length);
                // Remove the command from the line buffer, and reset time last command was sent
                stringstore_consume(linebuf, length);
                // Need more accurate time here, don't want the latency of the processing loop omitted
                timer_reset(&app->last_command, COMMAND_PACE);
            }
        }

    }
}

// Process cycle for the application

static int
hdmi2usb_process(struct hdmi2usb *app, int rc) {
    // basic stuff
    // The serial device is alaways at index 0 in the managed devices array.
    // It must be open and active, if not everything else is pointless
    iodev_t *serial = selector_get_device(&app->selector, 0);
    if (serial == NULL || iodev_getstate(serial) == IODEV_INACTIVE)
        return EX_NORMAL;

    // At least one listen port must also be open, check for this
    // when we iterate ports for application I/O processing
    size_t s_bytes = hdmi2usb_process_serial_data(app, serial);
    int listener_count = 0;
    int connect_count = 0;
    for (size_t index = 1; index < selector_device_count(&app->selector); index++) {
        iodev_t *dev = selector_get_device(&app->selector, index);
        if (iodev_is_listener(dev))
            ++listener_count;
        else {
            ++connect_count;
            // copy processed serial data to non-listener network sockets
            if (s_bytes)
                buffer_copy(iodev_tbuf(dev), &app->copy, s_bytes);
            // process input from network connection
            hdmi2usb_process_client_data(app, dev);
            // send any pending input on this connection to the device (maybe)
            hdmi2usb_process_client_commands(app, serial, dev);
        }
    }
    // reset the copy buffer
    buffer_flush(&app->copy);
    // exit if there are no active listeners
    return !listener_count ? EX_NORMAL : rc;
}



//// main application loop ////

int
hdmi2usb_main(struct hdmi2usb *app) {
    int rc = hdmi2usb_init(app, 0);
    while (rc == 0) {
        rc = selector_loop(&app->selector, app->opts.loop_time);
        // Main loop
        switch (signal_received) {
            case SIGHUP:
                log_critical("Reloading on SIGHUP");
                rc = hdmi2usb_init(app, hdmi2usb_close(app, EX_SUCCESS));
                break;
            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
                log_critical("Keyboard Quit");
                rc = EX_REQUEST;
                break;
            default:
                rc = hdmi2usb_process(app, rc);
                break;
        }
    }
    return hdmi2usb_close(app, rc);
}

