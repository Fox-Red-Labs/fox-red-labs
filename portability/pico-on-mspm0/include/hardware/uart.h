/* Pico SDK on MSPM0: hardware/uart.h. uart0 is the board's console UART (on the LP-MSPM0G3519, UART0 on PA10/PA11, the probe's
 * serial port), set to 115200 by SysConfig before main(). */
#ifndef PICO_MSPM0_UART_H
#define PICO_MSPM0_UART_H
#include "pico/types.h"
typedef struct uart_inst uart_inst_t;
extern uart_inst_t *const uart0;
#define uart_default uart0
void uart_putc_raw(uart_inst_t *uart, char c);
#endif
