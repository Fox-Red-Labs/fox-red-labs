/* Flash layer for the MSPM0G3519: 1 KB sector erase, 64-bit (8-byte aligned) program with hardware ECC, jump with VTOR moved. */
#include "flash.h"
#define SECTOR 1024u
flash_status flash_erase(uint32_t address) {
    for (uint32_t a = address; a < FLASH_APP_END_ADDRESS; a += SECTOR) {
        DL_FlashCTL_unprotectSector(FLASHCTL, a, DL_FLASHCTL_REGION_SELECT_MAIN);
        if (DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, a, DL_FLASHCTL_COMMAND_SIZE_SECTOR) == DL_FLASHCTL_COMMAND_STATUS_FAILED) return FLASH_ERROR;
    }
    return FLASH_OK;
}
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length) {   /* length in 32-bit words, even */
    flash_status status = FLASH_OK;
    for (uint32_t i = 0; i + 1 < length + 1 && i < length && status == FLASH_OK; i += 2) {
        if (address + 8 > FLASH_APP_END_ADDRESS) { status |= FLASH_ERROR_SIZE; break; }
        DL_FlashCTL_unprotectSector(FLASHCTL, address, DL_FLASHCTL_REGION_SELECT_MAIN);
        if (DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, address, &data[i]) == DL_FLASHCTL_COMMAND_STATUS_FAILED) status |= FLASH_ERROR_WRITE;
        if (data[i] != *(volatile uint32_t *)address || data[i + 1] != *(volatile uint32_t *)(address + 4)) status |= FLASH_ERROR_READBACK;
        address += 8;
    }
    return status;
}
void flash_jump_to_app(void) {
    uint32_t sp = *(volatile uint32_t *)FLASH_APP_START_ADDRESS, pc = *(volatile uint32_t *)(FLASH_APP_START_ADDRESS + 4u);
    SysTick->CTRL = 0u;
    __disable_irq();
    NVIC->ICER[0] = 0xFFFFFFFFu; NVIC->ICPR[0] = 0xFFFFFFFFu;
    SCB->VTOR = FLASH_APP_START_ADDRESS;          /* the app's vector table, or its first interrupt lands in this bootloader */
    __set_MSP(sp);
    __enable_irq();                               /* the application starts as after a reset: interrupts enabled */
    ((void (*)(void))pc)();
}
