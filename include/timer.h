//
// Created by David Nugent on 11/1/17.
// Simple non-blocking egg timer

#ifndef GENERIC_TIMER_H
#define GENERIC_TIMER_H

#include <time.h>

typedef unsigned long utime_t;
typedef struct timer_s timer_t;

struct timer_s {
    unsigned int alloc;   // non-zero = heap allocated
    utime_t
        t_started,
        t_ending;
};

extern timer_t *timer_init(timer_t *timer, utime_t start, utime_t duration);
extern timer_t *timer_reset(timer_t *timer, utime_t duration);
extern timer_t *timer_since(timer_t *timer, utime_t start);
extern void timer_free(timer_t *timer);

extern utime_t timer_elapsed(timer_t *timer);
extern utime_t timer_remaining(timer_t *timer);

extern int timer_expired(timer_t *timer);

// retrieve the current time in milliseconds
extern utime_t timer_getmillitime();
// returns the raw difference between two millisecond time stamps
// if now < len then result may be negative
extern long timer_calc_difference(utime_t now, utime_t then);


#endif //GENERIC_TIMER_H
