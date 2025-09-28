#ifndef INC_SYSTEM_H
#define INC_SYSTEM_H

#include "common-defines.h"

#define CPU_FREQ        (24000000)
#define SYSTICK_FREQ    (1000)

void system_setup(void);
uint64_t system_get_ticks(void);
void system_delay_ms(uint64_t millis);

#endif // INC_SYSTEM_H