/* Pico SDK on MSPM0: stdio on the default UART (hardware/uart.h and stdio_init_all). printf, puts and putchar are replaced so
 * the C library's own console path (the debugger's CIO) is never linked. "\n" goes out as "\r\n", the Pico SDK's default. */
#include <stdio.h>
#include <stdarg.h>
#include "ti_msp_dl_config.h"
#include "hardware/uart.h"
struct uart_inst { UART_Regs *hw; };
static struct uart_inst s_uart0 = { PICO_UART0_INST };
uart_inst_t *const uart0 = &s_uart0;
void uart_putc_raw(uart_inst_t *u, char c) { DL_UART_Main_transmitDataBlocking(u->hw, (uint8_t) c); }
bool stdio_init_all(void) { return true; }                /* UART0 is set to 115200 by SysConfig before main() */
int putchar(int c) { if (c == '\n') { uart_putc_raw(uart0, '\r'); } uart_putc_raw(uart0, (char) c); return c; }
int fputs(const char *s, FILE *f) { (void) f; while (*s) { putchar(*s++); } return 1; }   /* the C library's fputs.c also holds puts */
int puts(const char *s) { fputs(s, stdout); putchar('\n'); return 1; }
int vprintf(const char *fmt, va_list ap)
{
    static char b[256]; int n = vsnprintf(b, sizeof b, fmt, ap);
    for (int i = 0; i < n && i < (int) sizeof b - 1; i++) { putchar(b[i]); }
    return n;
}
int printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int n = vprintf(fmt, ap); va_end(ap); return n; }
