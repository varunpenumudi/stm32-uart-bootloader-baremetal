#include <libopencm3/stm32/flash.h>
#include "bl-flash.h"


#define MAIN_APPLICATION_START_PAGE 24
#define MAIN_APPLICATION_END_PAGE 63

void bl_flash_erase_main_application(void) {
    flash_unlock();
    for(uint8_t page = MAIN_APPLICATION_START_PAGE; page <= MAIN_APPLICATION_END_PAGE; page++) {
        flash_erase_page(FLASH_BASE + (page*1024));
    }
    flash_lock();
}


void bl_flash_write(uint32_t address, uint8_t* data, uint32_t length) {
    flash_unlock();

    uint32_t cur_address = address;
    uint32_t i = 0;
    uint16_t half_word = 0;

    while (i < length) {
        half_word = data[i];         // MSB - Little Endian
        if (i + 1 < length) {        // LSB - Little Endian
            half_word |= (data[i + 1] << 8);  
        }
        else {
            half_word |= (0xFF << 8);
        }
        flash_program_half_word(cur_address, half_word);
        i += 2;
        cur_address += 2;
    }
    
    flash_lock();
}