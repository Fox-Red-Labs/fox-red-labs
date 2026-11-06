// The five things a board must provide. The runner (frl_wasm.c) calls nothing else.
#pragma once
#include <stdint.h>
void hal_init(void);            // clocks, console, LED pin, microsecond timer
void hal_putc(int c);           // console character (UART on the boards, stdout on the host)
void hal_led(int on);           // 1 = LED on
void hal_delay_ms(uint32_t ms);
uint32_t hal_us(void);          // free-running microseconds (wraps at 2^32)
uint32_t hal_cpu_hz(void);
