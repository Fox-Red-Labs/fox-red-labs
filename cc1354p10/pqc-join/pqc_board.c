/*
 * Post-quantum secure join on the LP-CC1354P10-1 (Cortex-M33, 48 MHz, TI Arm Clang, SimpleLink CC13xx/CC26xx SDK 8.33, NoRTOS).
 * Runs the NIST known-answer tests, times ML-KEM-768 keygen, encaps and decaps and ML-DSA-44 keygen, sign and verify, then waits on the XDS110 backchannel
 * UART (115200 8N1) for the Mac side (tools/pqc_join/mac_side.py). The protocol and the crypto are in
 * portability/pqc-join/core; this file is the board glue only: UART, timer, TRNG, stack painting.
 * main() is the SDK's main_nortos.c (cc1354p10/bench/board), unchanged.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/TRNG.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>
#include "ti_drivers_config.h"
#include "bench_timer.h"
#include "pqc_join.h"
#include "pqc_sign.h"

#ifndef PQC_BUILD_ID
#define PQC_BUILD_ID "unknown"
#endif
#ifndef PQC_OPT
#define PQC_OPT "unknown"
#endif

volatile uint32_t bench_wraps;
static UART2_Handle uart;
static TRNG_Handle trng;
static uint32_t cpu_hz, tick_us;
static char rlog[6144];
static unsigned rlen;

/* The power policy named in pqc.syscfg: do nothing, so an idle CPU spins and the debugger can always attach. */
void tbBusyPolicy(void) {}

static void systick_isr(void) { bench_wraps++; }
static uint64_t now(void) { return bench_cycles(); }

static void out(const char *s)
{
    size_t w;
    UART2_write(uart, s, strlen(s), &w);
}

static void rand_bytes(uint8_t *o, size_t n)
{
    if (TRNG_getRandomBytes(trng, o, n) != TRNG_STATUS_SUCCESS) {
        out("TRNG failed\r\n");
        for (;;) {}
    }
}

static int getline_(char *buf, size_t max, uint32_t ms)
{
    size_t n = 0;
    uint32_t timeout = (ms * 1000U) / tick_us;
    for (;;) {
        char c; size_t got = 0;
        int_fast16_t st = UART2_readTimeout(uart, &c, 1, &got, n ? (1000U * 1000U) / tick_us : timeout);
        if (st != UART2_STATUS_SUCCESS || got != 1) { return n ? (int)(buf[n] = 0, n) : -1; }
        if (c == '\n') { buf[n] = 0; return (int)n; }
        if (c != '\r' && n + 1 < max) buf[n++] = c;
    }
}

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
    out(b); out("\r\n");
    if (rlen + strlen(b) + 3 < sizeof rlog) { strcpy(rlog + rlen, b); rlen += strlen(b); rlog[rlen++] = '\r'; rlog[rlen++] = '\n'; rlog[rlen] = 0; }
}

static void info(const char *key, const char *val)
{
    char b[160];
    snprintf(b, sizeof b, "I,%s,%s", key, val);
    out(b); out("\r\n");
    if (rlen + strlen(b) + 3 < sizeof rlog) { strcpy(rlog + rlen, b); rlen += strlen(b); rlog[rlen++] = '\r'; rlog[rlen++] = '\n'; rlog[rlen] = 0; }
}

void *mainThread(void *arg0)
{
    UART2_Params up;
    ClockP_FreqHz f;
    char b[120];
    (void)arg0;

    Power_setConstraint(PowerCC26XX_DISALLOW_STANDBY);
    Power_setConstraint(PowerCC26XX_DISALLOW_IDLE);
    UART2_Params_init(&up);
    up.baudRate = 115200;
    up.readReturnMode = UART2_ReadReturnMode_FULL;
    uart = UART2_open(CONFIG_UART2_0, &up);
    {   /* Board_init() initializes only Power and GPIO; every other driver's _init() is ours to call. Without
         * TRNG_init(), TRNG_open() returns NULL and the first TRNG call faults. */
        TRNG_Params tp;
        TRNG_init();
        TRNG_Params_init(&tp);
        tp.returnBehavior = TRNG_RETURN_BEHAVIOR_POLLING;   /* NoRTOS: no blocking wait on an interrupt */
        trng = TRNG_open(CONFIG_TRNG_0, &tp);
        if (trng == NULL) { out("TRNG_open failed\r\n"); for (;;) {} }
    }
    ClockP_getCpuFreq(&f);
    cpu_hz = f.lo;
    tick_us = ClockP_getSystemTickPeriod();
    HwiP_plug(INT_SYSTICK, (void *)systick_isr);
    bench_timer_start();

    out("\r\nBENCH START pqc-join\r\n");
    info("image", "pqc-join");
    info("build", PQC_BUILD_ID);
    info("board", "lp_cc1354p10_1");
    info("optimization", PQC_OPT);
    snprintf(b, sizeof b, "TI Arm Clang %d.%d.%d", __ti_major__, __ti_minor__, __ti_patchlevel__); info("compiler", b);
    info("sdk", "simplelink_cc13xx_cc26xx_sdk_8_33_00_16 nortos");
    snprintf(b, sizeof b, "%lu,ClockP_getCpuFreq", (unsigned long)cpu_hz); info("cpu_hz", b);
    info("timer", "SysTick on the CPU clock with a wrap counter; 1 tick = 1 CPU cycle");
    {   /* DWT cycle counter cross-check: does it count, and does it agree with SysTick? */
        uint64_t s0, s1; uint32_t d0, d1;
        DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        s0 = now(); d0 = DWT->CYCCNT;
        for (volatile int i = 0; i < 20000; i++) {}
        s1 = now(); d1 = DWT->CYCCNT;
        snprintf(b, sizeof b, "systick %lu dwt %lu", (unsigned long)(s1 - s0), (unsigned long)(d1 - d0)); info("dwt_check", b);
    }
    snprintf(b, sizeof b, "%lu", (unsigned long)((uintptr_t)&__STACK_END - (uintptr_t)&__stack)); info("stack_size_bytes", b);

    {
        pqc_hal_t h = {now, cpu_hz, rand_bytes, out, getline_, stack_paint, stack_used, result};
        int kat = pqc_kat(&h);
        int dkat;
        if (kat) pqc_timing(&h);
        else out("timing skipped: a known-answer test failed\r\n");
        dkat = pqc_sign_kat(&h);
        if (dkat) pqc_sign_timing(&h);
        else out("ML-DSA-44 timing skipped: a known-answer test failed\r\n");
        info("results", kat ? (dkat ? "kat pass" : "mlkem kat pass, mldsa kat FAIL") : "kat FAIL");
        if (kat) {
            int ok = pqc_join(&h);
            info("join", ok ? "ok" : "failed");
        }
    }
    for (;;) {                                               /* repeat the report so a late listener gets it */
        uint64_t t = now() + (uint64_t)cpu_hz * 10U;
        out("\r\nBENCH REPEAT pqc-join\r\n"); out(rlog); out("BENCH DONE\r\n");
        while (now() < t) {}
    }
}
