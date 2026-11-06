/*
 * Classical crypto timings on a Cortex-M0+ MSPM0 LaunchPad (LP-MSPM0G3519, 80 MHz, TI Arm Clang, bare metal): SHA-256 of 1 kB,
 * ECDSA P-256 sign and verify, ECDH P-256 shared secret. Engine: TI's MSP Crypto Library 0.33 (prebuilt cryptolib.a from the MSPM0 SDK,
 * software on this chip) and, for SHA-256, our portable C (portability/pqc-join/core/pqc_sym.c) beside it.
 * Timer: SysTick on the CPU clock (no DWT on the M0+). Output: RAM log read with the debugger (see pqc_board.c).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "bench_timer.h"
#include "ti/crypto/include/msp-crypto-lib.h"
#include "pqc_sym.h"
#include "sec_vectors.h"

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
static void row(const char *t, uint64_t c, int ok, const char *n) { char b[200]; snprintf(b, sizeof b, "R,sec,%s,%lu,%s,%s\r\n", t, (unsigned long)c, ok ? "ok" : "FAIL", n); out(b); }
static uint64_t med3(uint64_t a, uint64_t b, uint64_t c) { if (a > b) { uint64_t t = a; a = b; b = t; } if (b > c) { b = c; } return a > b ? a : b; }

static uint8_t buf[1024];
static uint32_t md32[8];
static uint8_t sr[32], ss[32], md[32];
static uint8_t ska[32], pkax[32], pkay[32], skb[32], pkbx[32], pkby[32];
static uint32_t k1[8], k2[8], k3[8], k4[8];

int main(void)
{
    uint64_t t0, t1, t[3]; uint32_t st[3]; int ok, i;
    char b[160];
    SYSCFG_DL_init();
    bench_timer_start();
    pq.cpu_hz = CPUCLK_FREQ;
    pq.stack_bytes = (uint32_t)(uintptr_t)&__STACK_END - (uint32_t)(uintptr_t)&__stack_start_marker;
    out("BENCH START sec-m0p\r\n");
    snprintf(b, sizeof b, "I,build,%s\r\n", SEC_BUILD_ID); out(b);
    snprintf(b, sizeof b, "I,cpu_hz,%lu\r\n", (unsigned long)pq.cpu_hz); out(b);

    /* SHA-256: FIPS 180-4 'abc' first, then 1,024 bytes in RAM */
    for (i = 0; i < 1024; i++) buf[i] = (uint8_t)i;
    ok = Crypto_SHA256("abc", 3, md32) == CRYPTO_HASH_STAT_OK && memcmp(md32, sha_abc, 32) == 0;
    for (i = 0; i < 3; i++) { paint(); t0 = bench_cycles(); Crypto_SHA256(buf, 1024, md32); t1 = bench_cycles(); t[i] = t1 - t0; st[i] = used(); }
    snprintf(b, sizeof b, "abc vector %s; stack %lu B; %lu.%02lu cycles per byte", ok ? "pass" : "FAIL", (unsigned long)st[2], (unsigned long)(med3(t[0], t[1], t[2]) / 1024), (unsigned long)((med3(t[0], t[1], t[2]) * 100 / 1024) % 100));
    row("sha256_1k_ti_cryptolib", med3(t[0], t[1], t[2]), ok, b);
    { uint8_t o[32]; pqc_sha256((const uint8_t *)"abc", 3, o); ok = memcmp(o, sha_abc, 32) == 0;
      for (i = 0; i < 3; i++) { paint(); t0 = bench_cycles(); pqc_sha256(buf, 1024, o); t1 = bench_cycles(); t[i] = t1 - t0; st[i] = used(); }
      snprintf(b, sizeof b, "abc vector %s; stack %lu B", ok ? "pass" : "FAIL", (unsigned long)st[2]);
      row("sha256_1k_pqc_sym_c", med3(t[0], t[1], t[2]), ok, b); }

    /* ECDSA P-256 sign: TI's CAVP vector, fixed k, must reproduce r and s */
    for (i = 0; i < 3; i++) {
        paint(); t0 = bench_cycles();
        crypto_ec_stat_t s = Crypto_ECDSA_P256_sign(sk_sign, md_sign, k_sign, sr, ss);
        t1 = bench_cycles(); t[i] = t1 - t0; st[i] = used();
        ok = s == CRYPTO_EC_STAT_ECDSA_SIGN_OK && memcmp(sr, r_sign, 32) == 0 && memcmp(ss, s_sign, 32) == 0;
        if (!ok) break;
    }
    snprintf(b, sizeof b, "CAVP vector %s; stack %lu B", ok ? "pass" : "FAIL", (unsigned long)st[2]);
    row("ecdsa_p256_sign", med3(t[0], t[1], t[2]), ok, b);

    /* verify: CAVP id 174 (message hashed first, as in TI's example); time verify alone, then reject a flipped digest */
    Crypto_SHA256(msg_v, sizeof msg_v, md32);
    for (i = 0; i < 3; i++) {
        paint(); t0 = bench_cycles();
        crypto_ec_stat_t s = Crypto_ECDSA_P256_verify(pkx_v, pky_v, (uint8_t *)md32, r_v, s_v);
        t1 = bench_cycles(); t[i] = t1 - t0; st[i] = used();
        ok = s == CRYPTO_EC_STAT_ECDSA_VALID_SIGNATURE; if (!ok) break;
    }
    md32[0] ^= 1u;
    { crypto_ec_stat_t s = Crypto_ECDSA_P256_verify(pkx_v, pky_v, (uint8_t *)md32, r_v, s_v);
      int rej = s != CRYPTO_EC_STAT_ECDSA_VALID_SIGNATURE; md32[0] ^= 1u;
      snprintf(b, sizeof b, "CAVP id 174 valid accepted: %s; one-bit-flipped digest rejected: %s; stack %lu B; digest not included", ok ? "yes" : "NO", rej ? "yes" : "NO", (unsigned long)st[2]);
      row("ecdsa_p256_verify", med3(t[0], t[1], t[2]), ok && rej, b); }

    /* ECDH: two key pairs from fixed 32-byte scalars, each side's shared secret from its key and the other's public key; they must match.
     * Byte order of the uint32_t arguments is not documented: variant 0 passes the big-endian bytes as they come out of keyGen, variant 1
     * passes them byte-reversed (little-endian words, least significant first, the order of the curve tables); the log says which one returned OK. */
    for (i = 0; i < 32; i++) { ska[i] = (uint8_t)(0x11 + i); skb[i] = (uint8_t)(0xA0 - i); }
    {
        uint8_t ra[32], rb[32]; memcpy(ra, ska, 32); memcpy(rb, skb, 32);
        crypto_ec_stat_t e1 = Crypto_EC_keyGen(&Crypto_EC_Curve_NIST256, ra, ska, pkax, pkay);
        crypto_ec_stat_t e2 = Crypto_EC_keyGen(&Crypto_EC_Curve_NIST256, rb, skb, pkbx, pkby);
        int v;
        for (v = 0; v < 4; v++) {
            crypto_ec_curve_t cv = Crypto_EC_Curve_NIST256; int rev = v & 1;
            if (v >= 2) cv.bufLen = (uint8_t)(cv.bufLen / 4);
            if (v == 0) { snprintf(b, sizeof b, "I,curve_bufLen,%d\r\n", (int)cv.bufLen); out(b); }
            uint32_t sk1[8], px[8], py[8], o1x[8], o1y[8], sk2[8], qx[8], qy[8], o2x[8], o2y[8];
            uint8_t *p;
            crypto_ec_stat_t d1 = 0, d2 = 0;
            for (i = 0; i < 3; i++) {
                if (!rev) { memcpy(sk1, ska, 32); memcpy(px, pkbx, 32); memcpy(py, pkby, 32); }
                else { for (int j = 0; j < 32; j++) { ((uint8_t *)sk1)[j] = ska[31 - j]; ((uint8_t *)px)[j] = pkbx[31 - j]; ((uint8_t *)py)[j] = pkby[31 - j]; } }
                memset(o1x, 0, 32); memset(o1y, 0, 32);
                paint(); t0 = bench_cycles();
                d1 = Crypto_ECDH_getSharedSecret(&cv, sk1, px, py, o1x, o1y);
                t1 = bench_cycles(); t[i] = t1 - t0; st[i] = used();
            }
            if (!rev) { memcpy(sk2, skb, 32); memcpy(qx, pkax, 32); memcpy(qy, pkay, 32); }
            else { for (int j = 0; j < 32; j++) { ((uint8_t *)sk2)[j] = skb[31 - j]; ((uint8_t *)qx)[j] = pkax[31 - j]; ((uint8_t *)qy)[j] = pkay[31 - j]; } }
            memset(o2x, 0, 32); memset(o2y, 0, 32);
            d2 = Crypto_ECDH_getSharedSecret(&cv, sk2, qx, qy, o2x, o2y);
            ok = e1 == CRYPTO_EC_STAT_KEYGEN_OK && e2 == CRYPTO_EC_STAT_KEYGEN_OK && d1 == CRYPTO_EC_STAT_SHARED_KEY_OK && d2 == CRYPTO_EC_STAT_SHARED_KEY_OK && memcmp(o1x, o2x, 32) == 0;
            snprintf(b, sizeof b, "variant %d (%s, bufLen %d); keygen %d,%d ecdh %d,%d; both secrets equal: %s; stack %lu B", v, rev ? "byte-reversed" : "as keyGen bytes", (int)cv.bufLen, (int)e1, (int)e2, (int)d1, (int)d2, memcmp(o1x, o2x, 32) == 0 ? "yes" : "NO", (unsigned long)st[2]);
            row(v==0?"ecdh_p256_shared_v0":v==1?"ecdh_p256_shared_v1":v==2?"ecdh_p256_shared_v2":"ecdh_p256_shared_v3", med3(t[0], t[1], t[2]), ok, b);
            p = (uint8_t *)o1x;
            { char h[200]; int n = 0; for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", p[i]);
              snprintf(b, sizeof b, "I,ecdh_v%d_secret_x_bytes_in_memory,%s\r\n", v, h); out(b); }
        }
        { char h[200]; int n = 0;
          for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", ska[i]); snprintf(b, sizeof b, "I,ska,%s\r\n", h); out(b);
          n = 0; for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", skb[i]); snprintf(b, sizeof b, "I,skb,%s\r\n", h); out(b);
          n = 0; for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", pkbx[i]); snprintf(b, sizeof b, "I,pkbx,%s\r\n", h); out(b);
          n = 0; for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", pkby[i]); snprintf(b, sizeof b, "I,pkby,%s\r\n", h); out(b);
          n = 0; for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", pkax[i]); snprintf(b, sizeof b, "I,pkax,%s\r\n", h); out(b);
          n = 0; for (i = 0; i < 32; i++) n += snprintf(h + n, sizeof h - n, "%02x", pkay[i]); snprintf(b, sizeof b, "I,pkay,%s\r\n", h); out(b); }
    }
    out("BENCH DONE\r\n");
    pq.magic = 0x50510001u;
    for (;;) {}
}
