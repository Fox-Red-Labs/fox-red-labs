/*
 * LP-MSPM0G3507 glue for the post-quantum join core. BUILD-ONLY portability check: this image is never flashed
 * (the board is borrowed and holds someone else's demo). UART and timer follow the G3507 benchmark harness
 * (mspm0g3507/third-party-bench), SysConfig output from its syscfg/ folder. The G3507 has no TRNG, so rand
 * is a labelled stand-in that returns a fixed pattern: this build proves the core compiles, links and what it costs,
 * not that a key made here is secret.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include <ti/driverlib/dl_uart_main.h>
#include "../../../mspm0g3507/mcu-bench/sdk-common/bench_timer.h"
#include "pqc_join.h"

static uint64_t now(void) { return bench_cycles(); }
static void puts_(const char *s) { while (*s) DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)*s++); }
static void rand_standin(uint8_t *o, size_t n) { for (size_t i = 0; i < n; i++) o[i] = (uint8_t)(0xA5 ^ i); }
static int getline_(char *buf, size_t max, uint32_t ms)
{
    size_t n = 0;
    uint64_t end = now() + (uint64_t)ms * (CPUCLK_FREQ / 1000);
    while (now() < end) {
        if (DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) continue;
        char c = (char)DL_UART_Main_receiveData(UART_0_INST);
        if (c == '\n') { buf[n] = 0; return (int)n; }
        if (c != '\r' && n + 1 < max) buf[n++] = c;
    }
    return -1;
}
extern uint32_t __stack, __STACK_END;
static void stack_paint(void) { uint32_t *p = &__stack, sp; __asm volatile("mov %0, sp" : "=r"(sp)); while ((uint32_t)p < sp - 64U) *p++ = 0xA5A5A5A5u; }
static uint32_t stack_used(void) { uint32_t *p = &__stack; while (p < &__STACK_END && *p == 0xA5A5A5A5u) p++; return (uint32_t)((uintptr_t)&__STACK_END - (uintptr_t)p); }
static void result(const char *t, uint64_t c, int ok, const char *note)
{
    char b[160];
    snprintf(b, sizeof b, "R,pqc,%s,%lu,%s,%s\r\n", t, (unsigned long)c, ok ? "ok" : "FAIL", note);
    puts_(b);
}

int main(void)
{
    pqc_hal_t h = {now, CPUCLK_FREQ, rand_standin, puts_, getline_, stack_paint, stack_used, result};
    SYSCFG_DL_init();
    bench_timer_start();
    if (pqc_kat(&h)) { pqc_timing(&h); (void)pqc_join(&h); }
    for (;;) {}
}
