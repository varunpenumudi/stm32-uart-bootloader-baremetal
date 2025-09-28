#include "common-defines.h"
#include "core/system.h"
#include "core/uart.h"
#include "timer.h"


#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>

#define BOOTLOADER_SIZE (0x6000)

#define LED_PORT (GPIOC) 
#define LED_PIN  (GPIO13) 

#define PWM_OUT_PORT (GPIOA)
#define PWM_OUT_PIN  (GPIO1)

#define USART_PORT   (GPIOA)
#define USART_TX_PIN (GPIO_USART2_TX)
#define USART_RX_PIN (GPIO_USART2_RX)

static void vector_setup(void) {
    SCB_VTOR = BOOTLOADER_SIZE;
}

static void gpio_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOC);   // For GPIO
    rcc_periph_clock_enable(RCC_GPIOA);   // For UART, PWM
    rcc_periph_clock_enable(RCC_AFIO);    // For UART
    
    gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);               // PC13 -> GPIO
    gpio_set_mode(PWM_OUT_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, PWM_OUT_PIN); // PA1 -> PWM
    gpio_set_mode(USART_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, USART_TX_PIN);  // PA2 -> Tx
    gpio_set_mode(USART_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, USART_RX_PIN);                    // PA3 -> Rx
}


int main(void) {
    vector_setup();
    system_setup();
    gpio_setup();
    timer_setup();
    uart_setup();

    float duty_cycle = 0.0;
    timer_set_pwm_duty_cycle(duty_cycle);

    while (1) {
        /* GPIO Task */
        gpio_toggle(LED_PORT, LED_PIN);

        /* PWM Task */
        duty_cycle = (duty_cycle < 100.0) ?(duty_cycle + 10.0) :0;
        timer_set_pwm_duty_cycle(duty_cycle);

        /* UART Task */
        while (uart_data_available()) {
            uart_write_byte(uart_read_byte());
        }

        /* Delay */
        system_delay_ms(1000);
    }

    return 0;
}