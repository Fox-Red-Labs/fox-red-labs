/* Replaces the upstream uart.h (STM32 HAL) with DriverLib; same interface. */
#ifndef UART_H_
#define UART_H_
#include <stdint.h>
#include "ti_msp_dl_config.h"
#define UART_TIMEOUT ((uint16_t)1000u)   /* ms, as upstream */
typedef enum { UART_OK = 0x00u, UART_ERROR = 0xFFu } uart_status;
uart_status uart_receive(uint8_t *data, uint16_t length);
uart_status uart_transmit_str(uint8_t *data);
uart_status uart_transmit_ch(uint8_t data);
#endif
