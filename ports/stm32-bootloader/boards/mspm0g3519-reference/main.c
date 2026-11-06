/* Bootloader main for the LP-MSPM0G3519, from ferenc-nemeth/stm32-bootloader main.c (MIT). Differences: DriverLib init, no button
 * (the LaunchPad button is not used): update mode when the application area is blank, otherwise jump to the application. */
#include "ti_msp_dl_config.h"
#include "xmodem.h"
#include "flash.h"
int main(void) {
    SYSCFG_DL_init();
    uart_transmit_str((uint8_t *)"\n\r================================\n\r");
    uart_transmit_str((uint8_t *)"UART Bootloader\n\r");
    uart_transmit_str((uint8_t *)"https://github.com/ferenc-nemeth\n\r");
    uart_transmit_str((uint8_t *)"================================\n\r\n\r");
    if (*(volatile uint32_t *)FLASH_APP_START_ADDRESS != 0xFFFFFFFFu) {
        uart_transmit_str((uint8_t *)"Jumping to user application...\n\r");
        flash_jump_to_app();
    }
    while (1) {
        uart_transmit_str((uint8_t *)"Please send a new binary file with Xmodem protocol to update the firmware.\n\r");
        xmodem_receive();
        uart_transmit_str((uint8_t *)"\n\rFailed... Please try again.\n\r");
    }
}
