#include "common-defines.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/memorymap.h>

#include "core/system.h"
#include "core/simple-timer.h"
#include "core/uart.h"
#include "core/crc8.h"
#include "comms.h"
#include "bl-flash.h"

#define BOOTLOADER_SIZE   (0x6000)
#define APP_START_ADDRESS (FLASH_BASE + BOOTLOADER_SIZE)

#define LED_PORT (GPIOC) 
#define LED_PIN  (GPIO13) 

#define USART_PORT   (GPIOA)
#define USART_TX_PIN (GPIO_USART2_TX)
#define USART_RX_PIN (GPIO_USART2_RX)

#define DEVICE_ID    (0x52)

#define SYNC_SEQ_B0  (0xAA)
#define SYNC_SEQ_B1  (0xBB)
#define SYNC_SEQ_B2  (0xCC)
#define SYNC_SEQ_B3  (0xDD)

#define DEFAULT_TIMEOUT (5000)

typedef enum {
    BL_State_Sync,
    BL_State_SendUpdateReq,
    BL_State_WaitForUpdateRes,
    BL_State_DeviceIDReq,
    BL_State_DeviceIDRes,
    BL_State_FwLengthReq,
    BL_State_FwLengthRes,
    BL_State_EraseApplication,
    BL_State_RecieveFirmware,
    BL_State_UpdateSuccess,
} bl_state_t;

static volatile bl_state_t bl_state = BL_State_Sync;
static volatile uint32_t fw_length = 0x00;
static volatile uint32_t cur_address = APP_START_ADDRESS;
static volatile uint32_t bytes_written = 0x00;
static volatile uint8_t sync_bytes[4] = {0U};

comms_packet_t packet;
simple_timer_t simple_timer;


static void gpio_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOC);   // For GPIO
    rcc_periph_clock_enable(RCC_GPIOA);   // For UART
    rcc_periph_clock_enable(RCC_AFIO);    // For UART

    gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);   // PC13 -> GPIO

    gpio_set_mode(USART_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, USART_TX_PIN);  // PA2 -> Tx
    gpio_set_mode(USART_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, USART_RX_PIN);                    // PA3 -> Rx
}


static void jump_to_app(void) {
    typedef void (*void_fn)(void);

    uint32_t *reset_vector_entry = (uint32_t *)(APP_START_ADDRESS + 4U);
    uint32_t *reset_vector =  (uint32_t *)(*reset_vector_entry);

    void_fn jump_func = (void_fn) reset_vector;
    jump_func();
}

static void bootloading_process_failed(void) {
    comms_create_single_byte_packet(&packet, BL_PACKET_FW_UPDATE_FAILED_DATA0);
    comms_write(&packet);
    jump_to_app();
}


int main(void) {
    system_setup();
    gpio_setup();
    uart_setup();
    comms_setup();
    simple_timer_setup(&simple_timer, 10000, false);

    simple_timer_reset(&simple_timer, 0);
    while (true) {
        switch (bl_state) {
        case BL_State_Sync: {
            if (simple_timer_has_elapsed(&simple_timer)) {
                bootloading_process_failed();
            }
            else if (uart_data_available()) {
                sync_bytes[0] = sync_bytes[1];
                sync_bytes[1] = sync_bytes[2];
                sync_bytes[2] = sync_bytes[3];
                sync_bytes[3] = uart_read_byte();

                if ((sync_bytes[0] == SYNC_SEQ_B0) && 
                    (sync_bytes[1] == SYNC_SEQ_B1) &&
                    (sync_bytes[2] == SYNC_SEQ_B2) &&
                    (sync_bytes[3] == SYNC_SEQ_B3)) 
                {
                    comms_create_single_byte_packet(&packet, BL_PACKET_SEQ_OBSERVED_DATA0);
                    comms_write(&packet);
                    bl_state = BL_State_SendUpdateReq;
                }
            }
        } break;
        
        case BL_State_SendUpdateReq: {
            comms_create_single_byte_packet(&packet, BL_PACKET_FW_UPDATE_REQ_DATA0);
            comms_write(&packet);
            bl_state = BL_State_WaitForUpdateRes;
            simple_timer_reset(&simple_timer, 0);
        } break;

        case BL_State_WaitForUpdateRes: {
            if (simple_timer_has_elapsed(&simple_timer)) {
                bootloading_process_failed();
            }
            else if (uart_data_available()) {
                comms_update();
                if (comms_packets_available()) {
                    comms_read(&packet);
                    if (comms_is_single_byte_packet(&packet, BL_PACKET_FW_UPDATE_RES_DATA0)) {
                        bl_state = BL_State_DeviceIDReq;
                    }
                }
            }
        } break;

        case BL_State_DeviceIDReq: {
            comms_create_single_byte_packet(&packet, BL_PACKET_DEVICE_ID_REQ_DATA0);
            comms_write(&packet);
            bl_state = BL_State_DeviceIDRes;
            simple_timer_reset(&simple_timer, 0);
        } break;

        case BL_State_DeviceIDRes: {
            if (simple_timer_has_elapsed(&simple_timer)) {
                bootloading_process_failed();
            }
            else if (uart_data_available()) {
                comms_update();
                if (comms_packets_available()) {
                    comms_read(&packet);
                    if ((packet.length == 2) && 
                        (packet.data[0] == BL_PACKET_DEVICE_ID_RES_DATA0) && 
                        (packet.data[1] == DEVICE_ID) &&
                        (packet.crc == crc8((uint8_t *) &packet, COMMS_PACKET_FULL_LEN-COMMS_PACKET_CRC_LEN))
                    ) {
                        bl_state = BL_State_FwLengthReq;
                    }
                }
            }
        } break;

        case BL_State_FwLengthReq: {
            comms_create_single_byte_packet(&packet, BL_PACKET_FW_LENGTH_REQ_DATA0);
            comms_write(&packet);
            bl_state = BL_State_FwLengthRes;
            simple_timer_reset(&simple_timer, 0);
        } break;

        case BL_State_FwLengthRes: {
            if (simple_timer_has_elapsed(&simple_timer)) {
                bootloading_process_failed();
            }
            else if (uart_data_available()) {
                comms_update();
                if (comms_packets_available()) {
                    comms_read(&packet);
                    if ((packet.length == 5) && (packet.data[0] == BL_PACKET_FW_LENGTH_RES_DATA0)) {
                        fw_length = (packet.data[1] << 24) | (packet.data[2] << 16) | (packet.data[3] << 8) | (packet.data[4]);
                        bl_state = BL_State_EraseApplication;
                    }
                }
            }
        } break;

        case BL_State_EraseApplication: {
            bl_flash_erase_main_application();
            bl_state = BL_State_RecieveFirmware;

            // Ready for Packets
            comms_create_single_byte_packet(&packet, BL_PACKET_READY_FOR_DATA_DATA0);
            comms_write(&packet);
            simple_timer_reset(&simple_timer, 0);
        } break;

        case BL_State_RecieveFirmware:  {
            if (simple_timer_has_elapsed(&simple_timer)) {
                bootloading_process_failed();
            }
            else if (uart_data_available()) {
                comms_update();
                if (comms_packets_available()) {
                    // Read Packet
                    comms_read(&packet);

                    // Write Packet Data
                    bl_flash_write(cur_address, packet.data, COMMS_PACKET_PAYLOAD_LEN);
                    cur_address += COMMS_PACKET_PAYLOAD_LEN;
                    bytes_written += COMMS_PACKET_PAYLOAD_LEN;
                }
            }
            if (bytes_written >= fw_length)  {
                bl_state = BL_State_UpdateSuccess;
            }
            else {
                // Ready for Next Packet
                comms_create_single_byte_packet(&packet, BL_PACKET_READY_FOR_DATA_DATA0);
                comms_write(&packet);
                simple_timer_reset(&simple_timer, 0);
            }
        } break;

        case BL_State_UpdateSuccess: {
            comms_create_single_byte_packet(&packet, BL_PACKET_FW_UPDATE_SUCCESS_DATA0);
            comms_write(&packet);

            for (uint8_t i = 0; i < 10; i++)  {
                gpio_toggle(LED_PORT, LED_PIN);
                system_delay_ms(500);
            }

            jump_to_app();
        } break;

        default: {
            bl_state = BL_State_Sync;
        } break;

        } 
    }

    jump_to_app();

    return 0;
}