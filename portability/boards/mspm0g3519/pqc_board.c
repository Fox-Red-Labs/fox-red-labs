/*
 * Post-quantum known-answer tests and timings on a Cortex-M0+ MSPM0 LaunchPad (LP-MSPM0G3519, 80 MHz, TI Arm Clang, MSPM0 SDK, bare metal).
 * The protocol-free part of cc1354p10/pqc-join/pqc_board.c: same core (portability/pqc-join/core), same hal, different board glue.
 *  - Timer: SysTick on the CPU clock with a wrap counter (the M0+ has no DWT cycle counter); 1 tick = 1 CPU cycle.
 *  - Output: no serial path to the Mac on this board (J12/J13 may be on BP), so every line goes into a RAM log (pqlog) that
 *    the debugger reads once pq.magic == 0x50510001. Same lines as the CC1354P10 UART text, so the same parser works.
 *  - Random: the pack times only; pqc_join() (needs a host) is not called, so rand is a fixed-seed generator and says so.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "bench_timer.h"
#include "pqc_join.h"
#ifndef PQC_NO_DSA
#include "pqc_sign.h"
#endif

#ifndef PQC_BUILD_ID
#define PQC_BUILD_ID "unknown"
#endif
#ifndef PQC_OPT
#define PQC_OPT "unknown"
#endif

volatile struct { uint32_t magic, len, cpu_hz, stack_bytes, kat, dkat; } pq;
#ifndef PQLOG_BYTES
#define PQLOG_BYTES 12288
#endif
char pqlog[PQLOG_BYTES];

static void out(const char *s)
{
    size_t n = strlen(s);
    if (pq.len + n + 1 < sizeof pqlog) { memcpy(pqlog + pq.len, s, n); pq.len += (uint32_t)n; pqlog[pq.len] = 0; }
}
static uint64_t now(void) { return bench_cycles(); }
static void rand_bytes(uint8_t *o, size_t n)
{
    static uint32_t s = 0x1234567u;
    while (n--) { s = s * 1664525u + 1013904223u; *o++ = (uint8_t)(s >> 24); }
}
static int getline_(char *b, size_t m, uint32_t ms) { (void)b; (void)m; (void)ms; return -1; }

extern uint32_t __STACK_END;
extern uint32_t __stack_start_marker __asm("__stack");
static uint32_t *stack_lo(void) { return (uint32_t *)((uintptr_t)&__STACK_END - pq.stack_bytes); }
static void stack_paint(void)
{
    uint32_t *p = stack_lo(), sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    while ((uint32_t)p < sp - 64U) *p++ = 0xA5A5A5A5u;
}
static uint32_t stack_used(void)
{
    uint32_t *p = stack_lo();
    while (p < &__STACK_END && *p == 0xA5A5A5A5u) p++;
    return (uint32_t)((uintptr_t)&__STACK_END - (uintptr_t)p);
}
static void result(const char *test, uint64_t cycles, int ok, const char *note)
{
    char b[200];
    snprintf(b, sizeof b, "R,pqc,%s,%lu,%s,%s\r\n", test, (unsigned long)cycles, ok ? "ok" : "FAIL", note);
    out(b);
}
static void info(const char *k, const char *v) { char b[160]; snprintf(b, sizeof b, "I,%s,%s\r\n", k, v); out(b); }

int main(void)
{
    char b[120];
    SYSCFG_DL_init();
    bench_timer_start();
    pq.cpu_hz = CPUCLK_FREQ;
    pq.stack_bytes = (uint32_t)(uintptr_t)&__STACK_END - (uint32_t)(uintptr_t)&__stack_start_marker;
    out("BENCH START pqc-m0p\r\n");
    info("build", PQC_BUILD_ID);
    info("optimization", PQC_OPT);
    snprintf(b, sizeof b, "TI Arm Clang %d.%d.%d", __ti_major__, __ti_minor__, __ti_patchlevel__); info("compiler", b);
    snprintf(b, sizeof b, "%lu", (unsigned long)pq.cpu_hz); info("cpu_hz", b);
    info("timer", "SysTick on the CPU clock with a wrap counter; 1 tick = 1 CPU cycle");
    snprintf(b, sizeof b, "%lu", (unsigned long)pq.stack_bytes); info("stack_size_bytes", b);
    {
        pqc_hal_t h = {now, pq.cpu_hz, rand_bytes, out, getline_, stack_paint, stack_used, result};
#ifdef PQC_DSA_NOSIGN
        pq.kat = 1;   /* ML-KEM-768 is not run in this image: with a 13 kB stack its timing pass overruns the stack; it has its own image */
        out("ML-KEM not run in this image (keygen and verify only build)\r\n");
#else
        int kat = pqc_kat(&h);
        pq.kat = (uint32_t)kat;
        if (kat) pqc_timing(&h); else out("timing skipped: a known-answer test failed\r\n");
#endif
#ifdef PQC_DSA_NOSIGN
        pq.dkat = (uint32_t)pqc_dsa_nosign(&h);   /* keygen and verify only: signing overruns a 13 kB stack */
#elif !defined(PQC_NO_DSA)
        pq.dkat = (uint32_t)pqc_sign_kat(&h);
        if (pq.dkat) pqc_sign_timing(&h); else out("ML-DSA-44 timing skipped: a known-answer test failed\r\n");
#else
        out("ML-DSA-44 not built: its stack does not fit this RAM\r\n");
#endif
    }
    out("BENCH DONE\r\n");
    pq.magic = 0x50510001u;
    for (;;) {}
}
