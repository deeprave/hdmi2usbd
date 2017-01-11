//
// Created by David Nugent on 11/1/17.
// implementation of a simple egg timer

#include <memory.h>
#include <stdlib.h>
#include <sys/time.h>
#include "timer.h"

#define TIMER_ALLOC 0xf276509a
#define USECS_PER_SEC 1000000
#define RIGHT_NOW ((utime_t)0)
#define NEVER     ((utime_t)0)

// retrieve the current time in milliseconds
utime_t
timer_getmillitime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (utime_t)((now.tv_sec * USECS_PER_SEC) + now.tv_usec);
}

// returns the raw difference between two millisecond time stamps
// if now < len then result may be negative
long
timer_calc_difference(utime_t now, utime_t then) {
    return now >= then ? (long)(now - then) : -(long)(then - now);
}


timer_t *
timer_init(timer_t *timer, utime_t start, utime_t duration) {
    if (timer != NULL)
        memset(timer, '\0', sizeof(timer_t));
    else {
        timer = calloc(1, sizeof(timer_t));
        timer->alloc = TIMER_ALLOC;
    }
    timer->t_started = start ? start : timer_getmillitime();
    timer->t_ending = duration ? timer->t_started + duration : NEVER;
    return timer;
}

timer_t *
timer_reset(timer_t *timer, utime_t duration) {
    return timer_init(timer, RIGHT_NOW, duration);
}

timer_t *
timer_since(timer_t *timer, utime_t start) {
    return timer_init(timer, start, NEVER);
}

void
timer_free(timer_t *timer) {
    if (timer->alloc == TIMER_ALLOC) {
        memset(timer, '\0', sizeof(timer_t));
        free(timer);
    }
}

utime_t
timer_elapsed(timer_t *timer) {
    return timer->t_started ? timer_getmillitime() - timer->t_started : 0;
}

utime_t
timer_remaining(timer_t *timer) {
    if (timer->t_ending != NEVER) {
        long remaining = timer_calc_difference(timer->t_ending, timer_getmillitime());
        if (remaining > 0)
            return (utime_t)remaining;
    }
    return 0;
}

int
timer_expired(timer_t *timer) {
    return timer->t_ending == NEVER ? 1 : timer_calc_difference(timer->t_ending, timer_getmillitime()) <= 0;
}

int
timer_trigger(timer_t *timer, void *arg, void f(timer_t*, void*)) {
    if (timer_expired(timer)) {
        f(timer, arg);
        timer_free(timer);
        return 1;
    }
    return 0;
}

