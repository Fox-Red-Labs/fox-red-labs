/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (MSPM0G3519 port of ferenc-nemeth/stm32-bootloader)
  ******************************************************************************
  */
#include "ti_msp_dl_config.h"
#include "uart.h"
#include "xmodem.h"
#include "flash.h"

int main(void)
{
  /* Clocks, pins and the UART (PA10 TX, PA11 RX) from the SysConfig file. */
  SYSCFG_DL_init();
  uart_init();

  /* Send welcome message on startup. */
  uart_transmit_str((uint8_t*)"\n\r================================\n\r");
  uart_transmit_str((uint8_t*)"UART Bootloader\n\r");
  uart_transmit_str((uint8_t*)"https://github.com/ferenc-nemeth\n\r");
  uart_transmit_str((uint8_t*)"================================\n\r\n\r");

  /* No button here: if the first word of the application area is programmed
   * (not erased), then jump to the user application, otherwise stay in the bootloader. */
  if (0xFFFFFFFFu != *(volatile uint32_t*)FLASH_APP_START_ADDRESS)
  {
    uart_transmit_str((uint8_t*)"Jumping to user application...\n\r");
    flash_jump_to_app();
  }

  /* Infinite loop */
  while (1)
  {
    /* Ask for new data and start the Xmodem protocol. */
    uart_transmit_str((uint8_t*)"Please send a new binary file with Xmodem protocol to update the firmware.\n\r");
    xmodem_receive();
    /* We only exit the xmodem protocol, if there are any errors.
     * In that case, notify the user and start over. */
    uart_transmit_str((uint8_t*)"\n\rFailed... Please try again.\n\r");
  }
}
