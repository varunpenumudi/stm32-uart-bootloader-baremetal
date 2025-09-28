#ifndef INC_BL_FLASH_H
#define INC_BL_FLASH_H

#include "common-defines.h"

void bl_flash_erase_main_application(void);
void bl_flash_write(uint32_t address, uint8_t* data, uint32_t length);

#endif /* INC_BL_FLASH_H */
