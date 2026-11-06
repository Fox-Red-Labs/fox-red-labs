/* Pico SDK on MSPM0: pico/stdlib.h. Pulls in the board's pin table and the headers the Pico's stdlib.h pulls in.
 * pico/error.h is the real SDK header (it comes after this folder on the include path). */
#ifndef PICO_MSPM0_STDLIB_H
#define PICO_MSPM0_STDLIB_H
#include "pico/types.h"
#include "pico/error.h"
#include "pico_board.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/time.h"
bool stdio_init_all(void);                         /* stdout on the board's default UART, "\n" sent as "\r\n" as the Pico does */
static inline void tight_loop_contents(void) { }
#endif
