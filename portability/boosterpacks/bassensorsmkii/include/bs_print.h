/* Tiny integer printing over bs_console_putc for the examples (no printf, no floating point). */
#ifndef BS_PRINT_H
#define BS_PRINT_H
#include <stdint.h>
#include "bs_board.h"
static inline void bs_puts(const char *s) { while (*s) bs_console_putc(*s++); }
static inline void bs_putu(uint32_t v) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); bs_puts(b + i); }
static inline void bs_puti(int32_t v) { if (v < 0) { bs_console_putc('-'); v = -v; } bs_putu((uint32_t)v); }
static inline void bs_puthex(uint32_t v, int digits) { for (int i = digits - 1; i >= 0; i--) bs_console_putc("0123456789abcdef"[(v >> (4 * i)) & 15]); }
/* value scaled by `scale` (10, 100 or 1000), printed with that many decimals; negative values below 1 keep their sign */
static inline void bs_putfix(int32_t v, uint32_t scale)
{
    if (v < 0) { bs_console_putc('-'); v = -v; }
    bs_putu((uint32_t)v / scale); bs_console_putc('.');
    uint32_t f = (uint32_t)v % scale;
    for (uint32_t s = scale / 10; s; s /= 10) bs_console_putc((char)('0' + (f / s) % 10));
}
#endif
