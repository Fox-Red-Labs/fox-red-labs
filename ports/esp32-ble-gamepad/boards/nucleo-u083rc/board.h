#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
void board_init(void);
void board_putc(char c);
uint32_t board_us(void);   /* 1 MHz free-running, wraps at 2^32 */
uint32_t board_ms(void);
uint32_t board_free_ram(void); /* bytes between end of .bss and top of RAM (stack lives here too) */
#endif
