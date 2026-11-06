/*
 * Post-quantum known-answer tests and timings on the NUCLEO-U083RC (STM32U083RC, Cortex-M0+, 48 MHz, no FPU), ST's GCC 14.3.1, STM32CubeU0 HAL, bare metal.
 * The protocol-free part of cc1354p10/pqc-join/pqc_board.c, the same core (portability/pqc-join/core) and the same hal as
 * portability/boards/mspm0g3519/pqc_board.c; only the glue differs:
 *  - Timer: SysTick on the CPU clock with a wrap counter (nucleo-u083rc/bench/common/bench_timer_u0.h); 1 tick = 1 CPU cycle.
 *  - Random: the RNG peripheral through ST's HAL, clocked from MSI as in ST's RNG_Config example. If it reports an error the bytes
 *    become a fixed pattern and pq.rng_err says so (timing does not depend on them; a key made that way is not secret).
 *  - Output: every line goes into a RAM log (pqlog) that the debugger reads once pq.magic == 0x50510001, the same R,pqc,... lines as
 *    the UART text of the other boards. pqc_join() needs a host on a serial port and is not called.
 *  - SHA-256 over 1 kB with pqc_sym.c, median of 3, as portability/boards/mspm0g3519/sec_board.c (feeds sha256_cpb).
 *  - Stack: the top of the 40 kB RAM; its size is the linker script's _Min_Stack_Size, passed to this file as PQ_STACK_BYTES by build.sh.
 * Images (build.sh): kem = ML-KEM-768 only (-DPQC_NO_DSA), dsa* = ML-DSA-44 only (-DPQC_NO_KEM), lowram/default = both. One algorithm per image keeps the
 * other's static test buffers (4.7 to 10 kB) out of the 40 kB RAM, which is what the stack is made from.
 * NOT RUN: built, never flashed. Entry is pqc_main(), called by nucleo-g071rb/bench/st-common/bench_main.c after ST's HAL_Init and SystemClock_Config.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32u0xx_hal.h"
#include "bench_timer_u0.h"
#include "pqc_join.h"
#include "pqc_sym.h"
#ifndef PQC_NO_DSA
#include "pqc_sign.h"
#endif

#ifndef PQC_BUILD_ID
#define PQC_BUILD_ID "unknown"
#endif
#ifndef PQC_OPT
#define PQC_OPT "unknown"
#endif
#ifndef PQ_STACK_BYTES
#error "PQ_STACK_BYTES must equal _Min_Stack_Size of the linker script (build.sh sets both)"
#endif
#ifndef PQLOG_BYTES
#define PQLOG_BYTES 4096
#endif

volatile struct { uint32_t magic, len, cpu_hz, stack_bytes, kat, dkat, rng_err, sha_ok; } pq;
char pqlog[PQLOG_BYTES];
static RNG_HandleTypeDef hrng;

void HAL_RNG_MspInit(RNG_HandleTypeDef *h)   /* as ST's RNG_Config example: RNG clock from MSI */
{
    RCC_PeriphCLKInitTypeDef p = {0};
    (void)h;
    p.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    p.RngClockSelection = RCC_RNGCLKSOURCE_MSI;
    if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK) pq.rng_err |= 2u;
    __HAL_RCC_RNG_CLK_ENABLE();
}

static void out(const char *s)
{
    size_t n = strlen(s);
    if (pq.len + n + 1 < sizeof pqlog) { memcpy(pqlog + pq.len, s, n); pq.len += (uint32_t)n; pqlog[pq.len] = 0; }
}
static uint64_t now(void) { return bench_cycles(); }
static void rand_bytes(uint8_t *o, size_t n)
{
    while (n) {
        uint32_t w;
        if (!pq.rng_err && HAL_RNG_GenerateRandomNumber(&hrng, &w) == HAL_OK) {
            for (int i = 0; i < 4 && n; i++, n--) { *o++ = (uint8_t)w; w >>= 8; }
        } else {
            pq.rng_err |= 1u;
            while (n) { *o++ = (uint8_t)(0xA5u ^ n); n--; }
        }
    }
}
static int getline_(char *b, size_t m, uint32_t ms) { (void)b; (void)m; (void)ms; return -1; }

extern uint32_t _estack;
static uint32_t *stack_lo(void) { return (uint32_t *)((uintptr_t)&_estack - PQ_STACK_BYTES); }
static void stack_paint(void)
{
    uint32_t *p = stack_lo(), sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    while ((uint32_t)p < sp - 64U) *p++ = 0xA5A5A5A5u;
}
static uint32_t stack_used(void)
{
    uint32_t *p = stack_lo();
    while (p < &_estack && *p == 0xA5A5A5A5u) p++;
    return (uint32_t)((uintptr_t)&_estack - (uintptr_t)p);
}
static void result(const char *test, uint64_t cycles, int ok, const char *note)
{
    char b[200];
    snprintf(b, sizeof b, "R,pqc,%s,%lu,%s,%s\r\n", test, (unsigned long)cycles, ok ? "ok" : "FAIL", note);
    out(b);
}
static void info(const char *k, const char *v) { char b[160]; snprintf(b, sizeof b, "I,%s,%s\r\n", k, v); out(b); }

static uint64_t med3(uint64_t a, uint64_t b, uint64_t c)
{
    if (a > b) { uint64_t t = a; a = b; b = t; }
    if (b > c) { uint64_t t = b; b = c; c = t; }
    return a > b ? a : b;
}

/* SHA-256 of "abc" against FIPS 180-4, then 1,024 bytes three times; the row is cycles for 1 kB (cycles per byte = / 1024). */
static void sha_timing(void)
{
    static const uint8_t abc[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
                                    0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    static uint8_t buf[1024];
    uint8_t o[32];
    uint64_t t[3];
    char b[64];
    for (unsigned i = 0; i < sizeof buf; i++) buf[i] = (uint8_t)i;
    pqc_sha256((const uint8_t *)"abc", 3, o);
    pq.sha_ok = memcmp(o, abc, 32) == 0;
    for (int i = 0; i < 3; i++) { uint64_t t0 = now(); pqc_sha256(buf, sizeof buf, o); t[i] = now() - t0; }
    snprintf(b, sizeof b, "1024 bytes, pqc_sym.c");
    result("sha256_1k_pqc_sym_c", med3(t[0], t[1], t[2]), (int)pq.sha_ok, b);
}

int pqc_main(void)
{
    char b[120];
    pq.cpu_hz = SystemCoreClock;
    pq.stack_bytes = PQ_STACK_BYTES;
    hrng.Instance = RNG;
    hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
    if (HAL_RNG_Init(&hrng) != HAL_OK) pq.rng_err |= 4u;
    bench_timer_start();
    out("BENCH START pqc-u083\r\n");
    info("build", PQC_BUILD_ID);
    info("optimization", PQC_OPT);
    snprintf(b, sizeof b, "arm-none-eabi-gcc %s", __VERSION__); info("compiler", b);
    snprintf(b, sizeof b, "%lu", (unsigned long)pq.cpu_hz); info("cpu_hz", b);
    info("timer", "SysTick on the CPU clock with a wrap counter; 1 tick = 1 CPU cycle");
    snprintf(b, sizeof b, "%lu", (unsigned long)pq.stack_bytes); info("stack_size_bytes", b);
    {
        pqc_hal_t h = {now, pq.cpu_hz, rand_bytes, out, getline_, stack_paint, stack_used, result};
#ifndef PQC_NO_KEM
        int kat = pqc_kat(&h);
        pq.kat = (uint32_t)kat;
        if (kat) pqc_timing(&h); else out("timing skipped: a known-answer test failed\r\n");
#else
        out("ML-KEM-768 not built in this image\r\n");
#endif
#ifndef PQC_NO_DSA
        pq.dkat = (uint32_t)pqc_sign_kat(&h);
        if (pq.dkat) pqc_sign_timing(&h); else out("ML-DSA-44 timing skipped: a known-answer test failed\r\n");
#else
        out("ML-DSA-44 not built in this image\r\n");
#endif
        sha_timing();
    }
    out("BENCH DONE\r\n");
    pq.magic = 0x50510001u;
    return 0;
}
