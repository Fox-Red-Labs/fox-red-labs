/* What an example needs beyond bs_port.h: bring-up and the console. Implemented by boards/mspm0_common/bs_port_mspm0.c. */
#ifndef BS_BOARD_H
#define BS_BOARD_H
#include <stdint.h>
void bs_board_init(void);            /* clocks, pins, I2C bus recovery, ADC, UART at 115200, microsecond tick */
void bs_console_putc(char c);        /* blocking, backchannel UART */
#endif
