/**
 * @file    uart.c
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module is a layer between the MSPM0 UART driver and my Xmodem protocol.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#include "uart.h"

/* SysTick is polled (no interrupt): one COUNTFLAG per millisecond. */
#define SYSTICK_PER_MS (CPUCLK_FREQ / 1000u)

/**
 * @brief   Starts the free running 1 ms SysTick used for the receive timeout, and the UART baud rate.
 */
void uart_init(void)
{
  SysTick->CTRL = 0u;
  SysTick->LOAD = SYSTICK_PER_MS - 1u;
  SysTick->VAL = 0u;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

  DL_UART_Main_disable(UART_0_INST);
  DL_UART_Main_configBaudRate(UART_0_INST, UART_0_INST_FREQUENCY, 115200u);
  DL_UART_Main_enable(UART_0_INST);
}

/**
 * @brief   Stops the SysTick and the UART, so that the user application starts from a clean state.
 */
void uart_deinit(void)
{
  while (DL_UART_Main_isBusy(UART_0_INST))
  {
  }
  SysTick->CTRL = 0u;
  DL_UART_Main_reset(UART_0_INST);
  DL_UART_Main_disablePower(UART_0_INST);
}

/**
 * @brief   Receives data from UART.
 * @param   *data: Array to save the received data.
 * @param   length:  Size of the data.
 * @return  status: Report about the success of the receiving.
 */
uart_status uart_receive(uint8_t *data, uint16_t length)
{
  for (uint16_t i = 0u; i < length; i++)
  {
    uint32_t waited = 0u;
    (void)SysTick->CTRL; /* Clear COUNTFLAG. */
    while (DL_UART_Main_isRXFIFOEmpty(UART_0_INST))
    {
      if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
      {
        waited++;
        if (waited >= UART_TIMEOUT)
        {
          return UART_ERROR;
        }
      }
    }
    data[i] = DL_UART_Main_receiveData(UART_0_INST);
    /* Clear overrun/framing/noise flags so they do not stay latched. */
    DL_UART_Main_clearInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_OVERRUN_ERROR | DL_UART_MAIN_INTERRUPT_FRAMING_ERROR |
                                                   DL_UART_MAIN_INTERRUPT_NOISE_ERROR | DL_UART_MAIN_INTERRUPT_BREAK_ERROR |
                                                   DL_UART_MAIN_INTERRUPT_PARITY_ERROR);
  }
  return UART_OK;
}

/**
 * @brief   Transmits a string to UART.
 * @param   *data: Array of the data.
 * @return  status: Report about the success of the transmission.
 */
uart_status uart_transmit_str(uint8_t *data)
{
  while ('\0' != *data)
  {
    DL_UART_Main_transmitDataBlocking(UART_0_INST, *data++);
  }
  return UART_OK;
}

/**
 * @brief   Transmits a single char to UART.
 * @param   *data: The char.
 * @return  status: Report about the success of the transmission.
 */
uart_status uart_transmit_ch(uint8_t data)
{
  DL_UART_Main_transmitDataBlocking(UART_0_INST, data);
  return UART_OK;
}
