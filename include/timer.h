//
// Created by David Nugent on 11/1/17.
// Simple non-blocking egg timer

#ifndef GENERIC_TIMER_H
#define GENERIC_TIMER_H

#include <time.h>

typedef unsigned long utime_t;
typedef struct microtimer_s microtimer_t;

struct microtimer_s {
    unsigned int alloc;   // non-zero = heap allocated
    utime_t
        t_started,
        t_ending;
};

extern microtimer_t *timer_init(microtimer_t *timer, utime_t start, utime_t duration);
extern microtimer_t *timer_reset(microtimer_t *timer, utime_t duration);
extern microtimer_t *timer_since(microtimer_t *timer, utime_t start);
extern void timer_free(microtimer_t *timer);

extern utime_t timer_elapsed(microtimer_t *timer);
extern utime_t timer_remaining(microtimer_t *timer);

extern int timer_expired(microtimer_t *timer);

// retrieve the current time in milliseconds
extern utime_t timer_getmillitime();
// returns the raw difference between two millisecond time stamps
// if now < len then result may be negative
extern long timer_calc_difference(utime_t now, utime_t then);


#endif //GENERIC_TIMER_H
