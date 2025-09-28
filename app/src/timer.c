#include "timer.h"
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rcc.h>

//   24_000_000  Hz
//   final_freq = (system_freq / prescaler)  / arr

#define PRESCALER     (24)
#define ARR_VALUE     (1000)


void timer_setup(void) {
    rcc_periph_clock_enable(RCC_TIM2);

    // High level timer configuration
    timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

    // Set timer PWM mode
    timer_set_oc_mode(TIM2, TIM_OC2, TIM_OCM_PWM1);

    // Enable PWM
    timer_enable_counter(TIM2);
    timer_enable_oc_output(TIM2, TIM_OC2);


    timer_set_prescaler(TIM2, (PRESCALER-1));
    timer_set_period(TIM2, (ARR_VALUE-1));
}

void timer_set_pwm_duty_cycle(float duty_cycle) {
    // duty_cycle = (ccr/arr) * 100
    // ccr = (duty_cycle/100) * arr

    const float raw_value = (float)ARR_VALUE * (duty_cycle/100);
    timer_set_oc_value(TIM2, TIM_OC2, (uint32_t) raw_value);
}
