#include "core/simple-timer.h"
#include "core/system.h"

void simple_timer_setup(simple_timer_t *timer, uint64_t wait_time, bool auto_reset) {
    timer->wait_time = wait_time;
    timer->target_time = system_get_ticks() + wait_time;
    timer->auto_reset = auto_reset;
}

void simple_timer_reset(simple_timer_t *timer, uint64_t drift) {
    timer->target_time = system_get_ticks() + (timer->wait_time) - drift;
}

bool simple_timer_has_elapsed(simple_timer_t *timer) {
    uint64_t now = system_get_ticks();
    bool has_elapsed = (now >= timer->target_time);
    if (has_elapsed && timer->auto_reset) {
        uint64_t drift = system_get_ticks() - timer->target_time;
        simple_timer_reset(timer, drift);
    }

    return has_elapsed;
}
