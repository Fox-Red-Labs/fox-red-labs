/* Replaces the upstream flash.h: same interface; application at 0x00010000. */
#ifndef FLASH_H_
#define FLASH_H_
#include <stdint.h>
#include "ti_msp_dl_config.h"
#define FLASH_APP_START_ADDRESS ((uint32_t)0x00010000u)
#define FLASH_APP_END_ADDRESS   ((uint32_t)0x00080000u)
typedef enum { FLASH_OK = 0x00u, FLASH_ERROR_SIZE = 0x01u, FLASH_ERROR_WRITE = 0x02u, FLASH_ERROR_READBACK = 0x04u, FLASH_ERROR = 0xFFu } flash_status;
flash_status flash_erase(uint32_t address);
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length);
void flash_jump_to_app(void);
#endif
