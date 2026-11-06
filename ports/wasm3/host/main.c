// Mac host build: the same runner and app, with stdout as the console and a microsecond clock.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "../common/frl_hal.h"
#include "../common/frl_wasm.h"
void hal_init(void) {}
void hal_putc(int c) { putchar(c); fflush(stdout); }
void hal_led(int on) { printf("[LED %s]\n", on ? "ON" : "off"); }
void hal_delay_ms(uint32_t ms) { usleep(ms * 1000u); }
uint32_t hal_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000u); }
uint32_t hal_cpu_hz(void) { return 0; }
int main(void) { hal_init(); return frl_wasm_main(); }
