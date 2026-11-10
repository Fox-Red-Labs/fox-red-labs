/*
 * Post-quantum known-answer tests and timings on the LP-AM243 (AM2434, Cortex-R5F R5FSS0-0, 800 MHz, 2 MB on-chip RAM,
 * TI Arm Clang, MCU+ SDK nortos). Same core (portability/pqc-join/core), same pqc_hal_t.
 *  - Timer: the R5F's PMU cycle counter (CCNT), enabled here; the 32-bit counter is extended to 64 bit on every read.
 *  - Output: UART0 through DebugP_log (the XDS110 console, 115200). Every line is also kept in a RAM log repeated every 10 s.
 *  - Random: a fixed-seed generator. Keygen and encaps time does not depend on the bytes. The join (pqc_join) needs a Mac and is not called.
 *  - Mode: PQC_NO_DSA is ML-KEM-768 only; default is both algorithms in one image (ML-KEM-768 and ML-DSA-44 keygen, sign, verify).
 *  - Stack: the linker symbol __STACK_END and PQC_STACK_BYTES (set by build.sh) give the region that is painted.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <kernel/dpl/DebugP.h>
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
#ifndef PQC_STACK_BYTES
#define PQC_STACK_BYTES 16384
#endif
#define RLOG_BYTES 3072

static char rlog[RLOG_BYTES];
static unsigned rlen;
static uint32_t cpu_hz = 800000000u;

static inline uint32_t ccnt(void) { uint32_t v; __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(v)); return v; }
static uint64_t now(void)
{
    static uint32_t last; static uint32_t hi;
    uint32_t v = ccnt();
    if (v < last) hi++;
    last = v;
    return ((uint64_t)hi << 32) | v;
}
static void out(const char *s) { DebugP_log("%s", s); }
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

extern uint32_t __STACK_END;
static uint32_t *stack_lo(void) { return (uint32_t *)((uintptr_t)&__STACK_END - PQC_STACK_BYTES); }
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
    snprintf(b, sizeof b, "R,pqc,%s,%lu,%s,%s", test, (unsigned long)cycles, ok ? "ok" : "FAIL", note);
    out(b); out("\r\n"); keep(b);
}
static void info(const char *key, const char *val)
{
    char b[160];
    snprintf(b, sizeof b, "I,%s,%s", key, val);
    out(b); out("\r\n"); keep(b);
}

int main(void)
{
    char b[120];
    int kat = 1, dkat = 1;
    System_init(); Board_init(); Drivers_open(); Board_driversOpen();
    { uint32_t pmcr = 0x5; __asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(pmcr)); uint32_t en = 0x80000000u; __asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(en)); }
    (void)now();
    out("\r\nBENCH START pqc-am243x\r\nI,pass,1\r\n");
    info("build", PQC_BUILD_ID);
    info("optimization", PQC_OPT);
    snprintf(b, sizeof b, "TI Arm Clang %d.%d.%d", __ti_major__, __ti_minor__, __ti_patchlevel__); info("compiler", b);
    snprintf(b, sizeof b, "%lu,nominal 799.98 MHz by the README fit", (unsigned long)cpu_hz); info("cpu_hz", b);
    info("timer", "R5F PMU CCNT, 1 tick = 1 CPU cycle, extended to 64 bit");
    info("rand", "fixed-seed generator");
    snprintf(b, sizeof b, "%lu", (unsigned long)PQC_STACK_BYTES); info("stack_size_bytes", b);
    {
        pqc_hal_t h = {now, cpu_hz, rand_bytes, out, getline_, stack_paint, stack_used, result};
        kat = pqc_kat(&h);
        if (kat) pqc_timing(&h); else out("timing skipped: a known-answer test failed\r\n");
#ifndef PQC_NO_DSA
        dkat = pqc_sign_kat(&h);
        if (dkat) pqc_sign_timing(&h); else out("ML-DSA-44 timing skipped: a known-answer test failed\r\n");
#else
        out("ML-DSA-44 not in this image\r\n");
#endif
        info("results", (kat && dkat) ? "kat pass" : "kat FAIL");
    }
    out("BENCH DONE\r\n");
    for (unsigned pass = 2;; pass++) {
        char p[40];
        uint64_t t = now() + (uint64_t)cpu_hz * 10U;
        snprintf(p, sizeof p, "\r\nBENCH START pqc-am243x\r\nI,pass,%u\r\n", pass);
        out(p); out(rlog); out("BENCH DONE\r\n");
        while (now() < t) {}
    }
}
