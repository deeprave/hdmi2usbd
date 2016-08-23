//
// Created by David Nugent on 30/01/2016.
//
// main.c
//
// Startup, argument parsing and handoff to daemon process
// Very little application specific code to see here, move on...

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "hdmi2usbd.h"
#include "logging.h"
#include "serial.h"


// short options
const char shortopts[] = "p:s:l:b:L:c:equF46vV::d::Dh";
// long options
const struct option longopts[] = {
//  { char*name, int has_arg, int *flag, int val }
    { "port",       required_argument,  NULL,           'p' },
    { "speed",      required_argument,  NULL,           's' },
    { "bufsize",    required_argument,  NULL,           'b' },
    { "listen",     required_argument,  NULL,           'l' },
    { "log",        required_argument,  NULL,           'L' },
    { "ctime",      required_argument,  NULL,           'c' },
    { "echo",       no_argument,        NULL,           'e' },
    { "quiet",      no_argument,        NULL,           'q' },
    { "utc",        no_argument,        NULL,           'u' },
    { "fsync",      no_argument,        NULL,           'F' },
    { "inet4",      no_argument,        NULL,           '4' },
    { "inet6",      no_argument,        NULL,           '6' },
    { "daemon",     no_argument,        NULL,           'D' },
    { "verbose",    optional_argument,  NULL,           'V' },
    { "debug",      optional_argument,  NULL,           'd' },
    { "version",    no_argument,        NULL,           'v' },
    { "help",       no_argument,        NULL,           'h' },
};
// options text (for help)
const char *helpopts[][3] = {
//  { char*default, char*arg_help, char*description }
    { "auto",           "auto|device [device...]",  "set serial port names (may contain wildcards)" },
    { "115200",         "baudrate",                 "set baud rate" },
    { "2048",           "buffer_size",              "set default iobuffer size" },
    { "localhost:8501", "[ip/hostname]:portnum",    "set listen address"},
    { NULL,             "FILENAME",                 "log to FILENAME (may contain strftime(3) strings)" },
    { "2000",           "TIMEOUT (ms)",             "minimum wait time between sending commands" },
    { NULL,             NULL,                       "echo log to stdout (twice for stderr)" },
    { NULL,             NULL,                       "don't echo log" },
    { NULL,             NULL,                       "log dates as UTC"},
    { NULL,             NULL,                       "force sync after each log write" },
    { NULL,             NULL,                       "listen on only ipv4 address(es)" },
    { NULL,             NULL,                       "listen on only ipv6 address(es)" },
    { NULL,             NULL,                       "detatch and fork into background" },
    { NULL,             "0-7",                      "increase or set verbosity level" },
    { NULL,             "0-7",                      "same as --verbose -V" },
    { NULL,             NULL,                       "display version info" },
    { NULL,             NULL,                       "display this help message" },
};


#define HELP_INDENT 50

static int
usage(FILE *out, int ec) {
    fprintf(out, "%s [ options ]\n Options:\n", HDMI2USBD_NAME);
    for(int index =0; index < sizeof(longopts)/sizeof(struct option); index++) {
        int len = 0;
        switch (longopts[index].has_arg) {
            case optional_argument:
                len = fprintf(out, "  -%c --%s [%s]", longopts[index].val, longopts[index].name, helpopts[index][1]);
                break;
            case required_argument:
                len = fprintf(out, "  -%c --%s %s", longopts[index].val, longopts[index].name, helpopts[index][1]);
                break;
            default:
            case no_argument:
                len = fprintf(out, "  -%c --%s", longopts[index].val, longopts[index].name);
                break;
        }
        char const *desc = helpopts[index][2] == NULL ? "" : helpopts[index][2];
        fprintf(out, "%*s %s\n", len > HELP_INDENT ? 0 : len - HELP_INDENT, "", desc);
    }
    return ec;
}


unsigned short
parse_port(char *at, int *rc) {
    unsigned short result = 0;
    if (*at != '\0' && *(++at) != '\0') {
        char *endptr = at;
        result = (unsigned short) strtoul(at, &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "invalid port number '%s'", at);
            *rc = 2;
        }
    }
    return result;
}

int
parse_args(int argc, char * const *argv, struct hdmi2usb_opts *opts) {
    int rc =EX_SUCCESS, r =0;
    int longindex = 0;

    while (rc == 0 && (r = getopt_long(argc, argv, shortopts, longopts, &longindex)) != EOF) {
        switch (r) {
            case 'h':
                exit(usage(stdout, 0));
                // notreached
            case 'v':
                printf("%s version %s\n", HDMI2USBD_NAME, HDMI2USBD_VERSION);
                exit(EX_SUCCESS);
                // notreached
            case 'V':
            case 'd':
                if (optarg == NULL)
                    opts->verbose++;
                else {
                    char *endptr = optarg;
                    opts->verbose = (int)strtol(optarg, &endptr, 10) & 7;
                    if (endptr == NULL || *endptr != '\0') {
                        fprintf(stderr, "invalid verbosity (0-7) '%s'\n", optarg);
                        rc = usage(stderr, 2);
                    }
                }
                break;
            case 'c': {
                char *endptr = optarg;
                unsigned long cmdtime = strtoul(optarg, &endptr, 10);
                if (endptr != NULL && *endptr == '\0') {
                    opts->command_time = cmdtime;
                    break;
                }
                fprintf(stderr, "invalid or missing command time: '%s'\n", optarg);
                rc = usage(stderr, EX_STARTUP);
                break;
            }
            case '4':
                opts->logflags ^= AF_INET;
                break;
            case '6':
                opts->logflags ^= AF_INET6;
                break;
            case 'D':
                opts->daemonize = 1;
                break;
            case 's': {
                char *endptr = optarg;
                unsigned long baudrate = strtoul(optarg, &endptr, 10);
                if (endptr != NULL && *endptr == '\0') {
                    opts->baudrate = speed_to_baud(baudrate);
                    if (opts->baudrate != BINVALID)
                        break;
                }
                fprintf(stderr, "invalid speed '%s'\n", optarg);
                rc = usage(stderr, EX_STARTUP);
                break;
            }
            case 'b': {
                char *endptr = optarg;
                unsigned bufsize = (unsigned)strtoul(optarg, &endptr, 10);
                if (endptr != NULL && *endptr == '\0') {
                    opts->iobufsize = bufsize;
                    break;
                }
                fprintf(stderr, "invalid buffsize '%s'\n", optarg);
                rc = usage(stderr, EX_STARTUP);
                break;
            }
            case 'l': {
                char *at, *str = optarg;
                if (str == NULL)
                    fprintf(stderr, "-l missing argument");
                else if (*str == '[') {  // [address]:port sytnax
                    at = strchr(str, ']');
                    if (at != NULL) {
                        size_t i = at - str - 1;
                        ++at;
                        if (i > 0) {
                            char listen_addr[i + 1];
                            strncpy(listen_addr, str + 1, i);
                            listen_addr[i] = '\0';
                            opts->listen_addr = strdup(listen_addr);
                        }
                        opts->listen_port = parse_port(at, &rc);
                        if (!rc)
                            break;
                    } else
                        fprintf(stderr, "invalid address '%s'", optarg);
                } else {
                    at = strchr(str, ':');
                    if (at == NULL)
                        at = str + strlen(str);
                    size_t i = at - optarg;
                    if (i > 0) {
                        char listen_addr[i + 1];
                        strncpy(listen_addr, optarg, i);
                        listen_addr[i] = '\0';
                        opts->listen_addr = strdup(listen_addr);
                    }
                    opts->listen_port = parse_port(at, &rc);
                    if (!rc)
                        break;
                }
                rc = usage(stderr, EX_STARTUP);
                break;
            }
            case 'p': {
                const char *args[MAX_SERIAL_SPECS + 1];
                int count = 0, index = optind - 1;
                for (count =0; index < argc && index < MAX_SERIAL_SPECS; count++) {
                    const char *next = argv[index++];
                    if (next == NULL || *next == '-' || *next == '\0')
                        break;
                    args[count] = next;
                }
                optind = index - 1;
                if (count > 0) {
                    size_t length =0;
                    for (int i =0; i < count; i++)
                        length += strlen(args[i]) + 1;
                    char *ports = malloc(length);
                    *ports = '\0';
                    for (int i =0; i < count; i++) {
                        strcat(ports, args[i]);
                        strcat(ports, "|");
                    }
                    opts->port = ports;
                    break;
                }
                // fallthru
            }
            case 'L':
                opts->logfile = optarg;
                break;
            case 'e':   // -> echo -> stderr -> off
                if (opts->logflags & LOG_ECHO) {
                    if (!(opts->logflags & LOG_STDERR)) { // echo -> stderr
                        opts->logflags |= LOG_STDERR;
                        break;
                    }
                    // fallthru: stderr -> off
                } else { // quiet -> echo
                    opts->logflags |= LOG_ECHO;
                    opts->logflags &= ~(LOG_NOECHO | LOG_STDERR);
                    break;
                }
                // fallthru
            case 'q':
                opts->logflags &= ~(LOG_ECHO|LOG_STDERR);
                opts->logflags |= LOG_NOECHO;
                break;
            case'u':
                opts->logflags |= LOG_UTC;
                break;
            case 'F':
                opts->logflags |= LOG_SYNC;
                break;
            default:
                fprintf(stderr, "parameter -%c is not being handled", r);
            case ':':
            case '?':
                rc = usage(stderr, EX_STARTUP);
                break;
        }
    }
    if (opts->logflags & AF_INET6 && opts->logflags & AF_INET)
        opts->logflags &= ~(AF_INET|AF_INET6);
    return rc;
}


int
main(int argc, char * const *argv) {
    struct hdmi2usb app = {
        .opts = {  // Defauts
            .verbose = 0,
            .logflags = 0,
            .logfile = NULL,
            .daemonize = 0,
            .baudrate = speed_to_baud(115200),
            .port = "auto",
            .iobufsize = 2048,
            .listen_addr = "localhost",
            .listen_port = 8501,
            .listen_flags = 0,
            .loop_time = 20UL,
            .command_time = 2000UL
        }
    };

    int rc = parse_args(argc, argv, &app.opts);
    if (rc == 0) {
        buffer_init(&app.proc, app.opts.iobufsize * 2);
        buffer_init(&app.copy, app.opts.iobufsize * 2);
        log_init(app.opts.logflags,
                 (enum Verbosity)app.opts.verbose,
                 app.opts.logfile);
        log_critical("%s version %s starting", HDMI2USBD_NAME, HDMI2USBD_VERSION);
        log_debug("       Device : %s", app.opts.port);
        log_debug("     Baudrate : %ld", baud_to_speed(app.opts.baudrate));
        log_debug(" Bind Address : %s", app.opts.listen_addr);
        log_debug("    Bind Port : %u", app.opts.listen_port);
        log_debug(" I/O Buffsize : %u", app.opts.iobufsize);
        log_debug("   Logging To : %s", app.opts.logfile ? app.opts.logfile : "<not set>");
        log_debug("Log Verbosity : %d", app.opts.verbose);
        log_debug("    Log Times : %s", app.opts.logflags & LOG_UTC ? "UTC" : "Local");
        log_debug("     Log Sync : %s", app.opts.logflags & LOG_SYNC ? "enabled" : "disabled");
        log_debug("    Daemonize : %s", app.opts.daemonize ? "Yes" : "No");
        rc = hdmi2usb_main(&app);
        log_critical("%s ended (exitcode=%d)", HDMI2USBD_NAME, rc);
    }
    return rc < EX_STARTUP ? 0 : rc;
}
