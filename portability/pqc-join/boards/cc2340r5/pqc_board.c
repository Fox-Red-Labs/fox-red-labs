/*
 * Post-quantum known-answer tests and timings on the LP-EM-CC2340R5 (CC2340R52, Cortex-M0+, 48 MHz, 36 kB RAM,
 * TI Arm Clang, SimpleLink Low Power F3 SDK, NoRTOS). Same core (portability/pqc-join/core), same pqc_hal_t.
 *  - Timer: SysTick on the CPU clock with a wrap counter (the M0+ has no DWT); 1 tick = 1 CPU cycle.
 *  - Output: the XDS110 backchannel UART at 115200; every line is also kept in a small RAM log repeated every 10 s.
 *  - Random: a fixed-seed generator. The CC23x0 has no general-purpose TRNG (TI's RNG driver there draws on radio noise and is not in the
 *    driver library as shipped), so the "rand" info line says "fixed". Keygen and encaps time does not depend on the bytes.
 *    The join (pqc_join) needs a Mac and 8 kB of buffers and is not called: this image checks and times only.
 *  - Mode, set by compiler flags in build.sh: PQC_NO_DSA is ML-KEM-768 only; default is ML-DSA-44 (keygen, sign, verify);
 *    PQC_DSA_NOSIGN is ML-DSA-44 keygen and verify only.
 * main() is the bench's main_nortos.c (cc2340r5/bench/board), unchanged.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC23X0.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>
#include "ti_drivers_config.h"
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
#ifndef RLOG_BYTES
#define RLOG_BYTES 3072
#endif

volatile uint32_t bench_wraps;
static UART2_Handle uart;
static uint32_t cpu_hz;
static char rlog[RLOG_BYTES];
static unsigned rlen;

static void systick_isr(void) { bench_wraps++; }
static uint64_t now(void) { return bench_cycles(); }

static void out(const char *s)
{
    size_t w;
    UART2_write(uart, s, strlen(s), &w);
}
static void keep(const char *b)
{
    size_t n = strlen(b);
    if (rlen + n + 3 < sizeof rlog) { memcpy(rlog + rlen, b, n); rlen += (unsigned)n; rlog[rlen++] = '\r'; rlog[rlen++] = '\n'; rlog[rlen] = 0; }
}

static void rand_bytes(uint8_t *o, size_t n)
{
    static uint32_t s = 0x1234567u;
    while (n--) { s = s * 1664525u + 1013904223u; *o++ = (uint8_t)(s >> 24); }
}
static int getline_(char *b, size_t m, uint32_t ms) { (void)b; (void)m; (void)ms; return -1; }

extern uint32_t __stack, __STACK_END;
static void stack_paint(void)
{
    uint32_t *p = &__stack, sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    while ((uint32_t)p < sp - 64U) *p++ = 0xA5A5A5A5u;
}
static uint32_t stack_used(void)
{
    uint32_t *p = &__stack;
    while (p < &__STACK_END && *p == 0xA5A5A5A5u) p++;
    return (uint32_t)((uintptr_t)&__STACK_END - (uintptr_t)p);
}
static void result(const char *test, uint64_t cycles, int ok, const char *note)
{
    char b[200];
    snprintf(b, sizeof b, "R,pqc,%s,%lu,%s,%s", test, (unsigned long)cycles, ok ? "ok" : "FAIL", note);
    out(b); out("\r\n"); keep(b);
}
static void info(const char *key, const char *val)
{
    char b[160];
    snprintf(b, sizeof b, "I,%s,%s", key, val);
    out(b); out("\r\n"); keep(b);
}

void *mainThread(void *arg0)
{
    UART2_Params up;
    ClockP_FreqHz f;
    char b[120];
    int kat = 1, dkat = 1;
    (void)arg0;

    Power_setConstraint(PowerLPF3_DISALLOW_STANDBY);
    Power_setConstraint(PowerLPF3_DISALLOW_IDLE);
    UART2_Params_init(&up);
    up.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &up);
    ClockP_getCpuFreq(&f);
    cpu_hz = f.lo;
    HwiP_plug(INT_SYSTICK, (void *)systick_isr);
    bench_timer_start();

    out("\r\nBENCH START pqc-cc2340r5\r\nI,pass,1\r\n");
    info("build", PQC_BUILD_ID);
    info("optimization", PQC_OPT);
    snprintf(b, sizeof b, "TI Arm Clang %d.%d.%d", __ti_major__, __ti_minor__, __ti_patchlevel__); info("compiler", b);
    snprintf(b, sizeof b, "%lu,ClockP_getCpuFreq", (unsigned long)cpu_hz); info("cpu_hz", b);
    info("timer", "SysTick on the CPU clock with a wrap counter; 1 tick = 1 CPU cycle");
    info("rand", "fixed-seed generator (no general-purpose TRNG on the CC23x0)");
    snprintf(b, sizeof b, "%lu", (unsigned long)((uintptr_t)&__STACK_END - (uintptr_t)&__stack)); info("stack_size_bytes", b);
    {
        pqc_hal_t h = {now, cpu_hz, rand_bytes, out, getline_, stack_paint, stack_used, result};
#if defined(PQC_NO_DSA)
        kat = pqc_kat(&h);
        if (kat) pqc_timing(&h); else out("timing skipped: a known-answer test failed\r\n");
        out("ML-DSA-44 not in this image\r\n");
#elif defined(PQC_DSA_NOSIGN)
        out("ML-KEM not run in this image (ML-DSA-44 keygen and verify build)\r\n");
        dkat = pqc_dsa_nosign(&h);
#else
        out("ML-KEM not run in this image (ML-DSA-44 build)\r\n");
        dkat = pqc_sign_kat(&h);
        if (dkat) pqc_sign_timing(&h); else out("ML-DSA-44 timing skipped: a known-answer test failed\r\n");
#endif
        info("results", (kat && dkat) ? "kat pass" : "kat FAIL");
    }
    out("BENCH DONE\r\n");
    for (unsigned pass = 2;; pass++) {                       /* repeat the report so a late listener gets it */
        char p[40];
        uint64_t t = now() + (uint64_t)cpu_hz * 10U;
        snprintf(p, sizeof p, "\r\nBENCH START pqc-cc2340r5\r\nI,pass,%u\r\n", pass);
        out(p); out(rlog); out("BENCH DONE\r\n");
        while (now() < t) {}
    }
}
