/*
 * ECDSA P-256 sign and verify and ECDH P-256 with micro-ecc (kmackay/micro-ecc, unmodified except the one sed of ".syntax divided" for the
 * asm build, see build_sec.sh) on a Cortex-M0+ MSPM0 LaunchPad at 80 MHz. Timer, log and stack painting as in sec_board.c.
 * Random k and the ECDH scalars come from a fixed-seed xorshift generator (repeatable; benchmark only).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "bench_timer.h"
#include "uECC.h"
#include "pqc_sym.h"

#ifndef SEC_BUILD_ID
#define SEC_BUILD_ID "unknown"
#endif
volatile struct { uint32_t magic, len, cpu_hz, stack_bytes; } pq;
char pqlog[8192];
static void out(const char *s) { size_t n = strlen(s); if (pq.len + n + 1 < sizeof pqlog) { memcpy(pqlog + pq.len, s, n); pq.len += (uint32_t)n; pqlog[pq.len] = 0; } }
extern uint32_t __STACK_END;
extern uint32_t __stack_start_marker __asm("__stack");
static uint32_t *stack_lo(void) { return (uint32_t *)((uintptr_t)&__STACK_END - pq.stack_bytes); }
static void paint(void) { uint32_t *p = stack_lo(), sp; __asm volatile("mov %0, sp" : "=r"(sp)); while ((uint32_t)p < sp - 64U) *p++ = 0xA5A5A5A5u; }
static uint32_t used(void) { uint32_t *p = stack_lo(); while (p < &__STACK_END && *p == 0xA5A5A5A5u) p++; return (uint32_t)((uintptr_t)&__STACK_END - (uintptr_t)p); }
static void row(const char *t, uint64_t c, int ok, const char *n) { char b[240]; snprintf(b, sizeof b, "R,sec,%s,%lu,%s,%s\r\n", t, (unsigned long)c, ok ? "ok" : "FAIL", n); out(b); }
static uint64_t med3(uint64_t a, uint64_t b, uint64_t c) { if (a > b) { uint64_t t = a; a = b; b = t; } if (b > c) { b = c; } return a > b ? a : b; }
static void hexout(const char *k, const uint8_t *p, int n) { char b[200]; int o = snprintf(b, sizeof b, "I,%s,", k); for (int i = 0; i < n; i++) o += snprintf(b + o, sizeof b - o, "%02x", p[i]); snprintf(b + o, sizeof b - o, "\r\n"); out(b); }

static uint32_t rs = 0x1234ABCDu;
static int det_rng(uint8_t *d, unsigned n) { while (n--) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; *d++ = (uint8_t)(rs >> 8); } return 1; }

int main(void)
{
    uint8_t dg[32], pa[64], sa[32], pb[64], sb[32], sig[64], s1[32], s2[32];
    uint64_t t0, t1, t[3]; uint32_t st = 0; int i, ok = 1, rej;
    uECC_Curve cv = uECC_secp256r1();
    char b[200];
    SYSCFG_DL_init();
    bench_timer_start();
    pq.cpu_hz = CPUCLK_FREQ;
    pq.stack_bytes = (uint32_t)(uintptr_t)&__STACK_END - (uint32_t)(uintptr_t)&__stack_start_marker;
    out("BENCH START sec-uecc\r\n");
    snprintf(b, sizeof b, "I,build,%s\r\n", SEC_BUILD_ID); out(b);
    uECC_set_rng(det_rng);
    pqc_sha256((const uint8_t *)"micro-ecc bench digest", 22, dg);
    ok = uECC_make_key(pa, sa, cv) && uECC_make_key(pb, sb, cv);
    for (i = 0; i < 3; i++) { paint(); t0 = bench_cycles(); ok &= uECC_sign(sa, dg, 32, sig, cv); t1 = bench_cycles(); t[i] = t1 - t0; if (used() > st) st = used(); }
    snprintf(b, sizeof b, "random k from a fixed-seed xorshift; stack %lu B", (unsigned long)st);
    row("ecdsa_p256_sign", med3(t[0], t[1], t[2]), ok, b);
    st = 0; ok = 1;
    for (i = 0; i < 3; i++) { paint(); t0 = bench_cycles(); ok &= uECC_verify(pa, dg, 32, sig, cv); t1 = bench_cycles(); t[i] = t1 - t0; if (used() > st) st = used(); }
    dg[5] ^= 0x10; rej = !uECC_verify(pa, dg, 32, sig, cv); dg[5] ^= 0x10;
    snprintf(b, sizeof b, "own signature accepted: %s; one-bit-flipped digest rejected: %s; stack %lu B; digest not included", ok ? "yes" : "NO", rej ? "yes" : "NO", (unsigned long)st);
    row("ecdsa_p256_verify", med3(t[0], t[1], t[2]), ok && rej, b);
    st = 0; ok = 1;
    for (i = 0; i < 3; i++) { paint(); t0 = bench_cycles(); ok &= uECC_shared_secret(pb, sa, s1, cv); t1 = bench_cycles(); t[i] = t1 - t0; if (used() > st) st = used(); }
    ok &= uECC_shared_secret(pa, sb, s2, cv) && memcmp(s1, s2, 32) == 0;
    snprintf(b, sizeof b, "both secrets equal: %s; stack %lu B", memcmp(s1, s2, 32) == 0 ? "yes" : "NO", (unsigned long)st);
    row("ecdh_p256_shared", med3(t[0], t[1], t[2]), ok, b);
    hexout("sa", sa, 32); hexout("pb", pb, 64); hexout("pa", pa, 64); hexout("secret", s1, 32);
    out("BENCH DONE\r\n");
    pq.magic = 0x50510001u;
    for (;;) {}
}
