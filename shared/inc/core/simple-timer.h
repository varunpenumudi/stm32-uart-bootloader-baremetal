#ifndef INC_SIMPLE_TIMER_H
#define INC_SIMPLE_TIMER_H

#include "common-defines.h"

typedef struct {
    uint64_t wait_time;
    uint64_t target_time;
    bool auto_reset;
} simple_timer_t;

void simple_timer_setup(simple_timer_t *timer, uint64_t wait_time, bool auto_reset);
void simple_timer_reset(simple_timer_t *timer, uint64_t drift);
bool simple_timer_has_elapsed(simple_timer_t *timer);

#endif /* INC_SIMPLE_TIMER_H */
