#include <stdio.h>
#include <string.h>
#include "pqc_sign.h"
#include "pqc_mldsa_kat_data.h"
#include "mldsa_native.h"

#define DS(x) PQCP_MLDSA_NATIVE_MLDSA44_##x

static uint8_t g_dpk[PQC_DSA_PK_BYTES], g_dsk[PQC_DSA_SK_BYTES], g_sig[PQC_DSA_SIG_BYTES];
static uint8_t g_pre[MLD_DOMAIN_SEPARATION_MAX_BYTES];

static int cmp(const uint8_t *a, const uint8_t *b, size_t n) { return memcmp(a, b, n) == 0; }

/* FIPS 204 ML-DSA.Sign (external, pure): Sign_internal over M' = 0 || |ctx| || ctx || M with the given rnd. */
static int sign_ext(uint8_t *sig, const uint8_t *m, size_t mlen, const uint8_t *ctx, size_t ctxlen,
                    const uint8_t rnd[32], const uint8_t *sk)
{
    size_t prelen = DS(prepare_domain_separation_prefix)(g_pre, NULL, 0, ctx, ctxlen, MLD_PREHASH_NONE);
    if (prelen == 0) return -1;
    return DS(signature_internal)(sig, m, mlen, g_pre, prelen, rnd, sk, 0);
}

/* ---------- known-answer tests ---------- */
int pqc_sign_kat(const pqc_hal_t *h)
{
    static const uint8_t zero[32];
    int ok_kg, ok_sh, ok_sd, ok_vy, ok_vn, ok_rt;

    ok_kg = DS(keypair_internal)(g_dpk, g_dsk, kd_keygen_seed) == 0 &&
            cmp(g_dpk, kd_keygen_pk, sizeof g_dpk) && cmp(g_dsk, kd_keygen_sk, sizeof g_dsk);
    ok_sh = sign_ext(g_sig, kd_sign_hedged_message, KD_SIGN_HEDGED_MESSAGE_LEN, kd_sign_hedged_context,
                     KD_SIGN_HEDGED_CONTEXT_LEN, kd_sign_hedged_rnd, kd_sign_hedged_sk) == 0 &&
            cmp(g_sig, kd_sign_hedged_signature, sizeof g_sig);
    ok_sd = sign_ext(g_sig, kd_sign_deterministic_message, KD_SIGN_DETERMINISTIC_MESSAGE_LEN,
                     kd_sign_deterministic_context, KD_SIGN_DETERMINISTIC_CONTEXT_LEN, zero, kd_sign_deterministic_sk) == 0 &&
            cmp(g_sig, kd_sign_deterministic_signature, sizeof g_sig);
    ok_vy = DS(verify)(kd_verify_valid_signature, kd_verify_valid_message, KD_VERIFY_VALID_MESSAGE_LEN,
                       kd_verify_valid_context, KD_VERIFY_VALID_CONTEXT_LEN, kd_verify_valid_pk) == 0;
    ok_vn = DS(verify)(kd_verify_invalid_signature, kd_verify_invalid_message, KD_VERIFY_INVALID_MESSAGE_LEN,
                       kd_verify_invalid_context, KD_VERIFY_INVALID_CONTEXT_LEN, kd_verify_invalid_pk) != 0;
    /* round trip with the NIST key pair: our signature verifies, and one flipped message bit does not */
    ok_rt = 0;
    {
        uint8_t m[32]; memset(m, 0x5a, sizeof m);
        if (sign_ext(g_sig, m, sizeof m, NULL, 0, zero, kd_keygen_sk) == 0 &&
            DS(verify)(g_sig, m, sizeof m, NULL, 0, kd_keygen_pk) == 0) {
            m[0] ^= 1;
            ok_rt = DS(verify)(g_sig, m, sizeof m, NULL, 0, kd_keygen_pk) != 0;
        }
    }
    h->result("kat_mldsa44_keygen_nist_acvp_1", 0, ok_kg, "pk and sk match");
    h->result("kat_mldsa44_sign_hedged_nist_acvp", 0, ok_sh, "signature matches");
    h->result("kat_mldsa44_sign_deterministic_nist_acvp", 0, ok_sd, "signature matches");
    h->result("kat_mldsa44_verify_valid_nist_acvp", 0, ok_vy, "accepted");
    h->result("kat_mldsa44_verify_invalid_nist_acvp", 0, ok_vn, "rejected");
    h->result("kat_mldsa44_round_trip", 0, ok_rt, "own signature verifies, flipped message bit rejected");
    { char t[140]; snprintf(t, sizeof t, "ML-DSA-44 KAT keygen %s, sign hedged %s, sign deterministic %s, verify valid %s, invalid %s, round trip %s\r\n",
        ok_kg ? "PASS" : "FAIL", ok_sh ? "PASS" : "FAIL", ok_sd ? "PASS" : "FAIL", ok_vy ? "PASS" : "FAIL", ok_vn ? "PASS" : "FAIL", ok_rt ? "PASS" : "FAIL"); h->puts_(t); }
    {
        int all = ok_kg && ok_sh && ok_sd && ok_vy && ok_vn && ok_rt;
        h->puts_(all ? "ML-DSA-44 KAT PASS\r\n" : "ML-DSA-44 KAT FAIL\r\n");
        return all;
    }
}

/* ---------- timing ---------- */
static uint64_t med3(uint64_t a, uint64_t b, uint64_t c)
{
    if (a > b) { uint64_t t = a; a = b; b = t; }
    if (b > c) { b = c; }
    return a > b ? a : b;
}

void pqc_sign_timing(const pqc_hal_t *h)
{
    static const uint8_t zero[32];
    uint8_t m[32];
    uint64_t t[3][3]; uint32_t st[3][3], base;
    int ok = 1;
    memcpy(m, "fox red labs ml-dsa-44 message.", 32);
    for (int r = 0; r < 3; r++) {
        uint64_t t0, t1;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok &= DS(keypair_internal)(g_dpk, g_dsk, kd_keygen_seed) == 0; t1 = h->now(); t[0][r] = t1 - t0; st[0][r] = h->stack_used() - base;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok &= sign_ext(g_sig, m, sizeof m, NULL, 0, zero, g_dsk) == 0; t1 = h->now(); t[1][r] = t1 - t0; st[1][r] = h->stack_used() - base;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok &= DS(verify)(g_sig, m, sizeof m, NULL, 0, g_dpk) == 0; t1 = h->now(); t[2][r] = t1 - t0; st[2][r] = h->stack_used() - base;
    }
    static const char *nm[3] = {"mldsa44_keygen", "mldsa44_sign", "mldsa44_verify"};
    static const char *sn[3] = {"stack_mldsa44_keygen", "stack_mldsa44_sign", "stack_mldsa44_verify"};
    for (int i = 0; i < 3; i++) {
        char note[96], line[170];
        uint32_t smax = st[i][0] > st[i][1] ? st[i][0] : st[i][1]; if (st[i][2] > smax) smax = st[i][2];
        uint64_t md = med3(t[i][0], t[i][1], t[i][2]);
        snprintf(note, sizeof note, "runs %lu/%lu/%lu", (unsigned long)t[i][0], (unsigned long)t[i][1], (unsigned long)t[i][2]);
        h->result(nm[i], md, ok, note);
        h->result(sn[i], smax, ok, "net of caller frames");
        snprintf(line, sizeof line, "%s cycles %lu %lu %lu median %lu (%lu us at %lu MHz) stack %lu B\r\n", nm[i],
                 (unsigned long)t[i][0], (unsigned long)t[i][1], (unsigned long)t[i][2], (unsigned long)md,
                 (unsigned long)(md * 1000000ull / h->cpu_hz), (unsigned long)(h->cpu_hz / 1000000u), (unsigned long)smax);
        h->puts_(line);
    }
    {   /* signing time depends on the message (rejection sampling): sign 16 different messages, deterministic rnd */
        uint64_t s[16];
        int ok16 = 1;
        for (int i = 0; i < 16; i++) {
            uint64_t t0, t1;
            m[31] = (uint8_t)i;
            t0 = h->now(); ok16 &= sign_ext(g_sig, m, sizeof m, NULL, 0, zero, g_dsk) == 0; t1 = h->now(); s[i] = t1 - t0;
            ok16 &= DS(verify)(g_sig, m, sizeof m, NULL, 0, g_dpk) == 0;
        }
        for (int i = 1; i < 16; i++) for (int j = i; j > 0 && s[j - 1] > s[j]; j--) { uint64_t x = s[j]; s[j] = s[j - 1]; s[j - 1] = x; }
        {
            uint64_t sum = 0, md = (s[7] + s[8]) / 2;
            char note[110], line[170];
            for (int i = 0; i < 16; i++) sum += s[i];
            snprintf(note, sizeof note, "16 messages: min %lu max %lu mean %lu", (unsigned long)s[0], (unsigned long)s[15], (unsigned long)(sum / 16));
            h->result("mldsa44_sign16", md, ok16, note);
            snprintf(line, sizeof line, "mldsa44_sign over 16 messages: min %lu median %lu mean %lu max %lu cycles\r\n",
                     (unsigned long)s[0], (unsigned long)md, (unsigned long)(sum / 16), (unsigned long)s[15]);
            h->puts_(line);
        }
    }
}

/* ---------- keygen and verify only (a part whose stack cannot hold signing) ---------- */
int pqc_dsa_nosign(const pqc_hal_t *h)
{
    int ok_kg, ok_vy, ok_vn;
    uint64_t t[2][3]; uint32_t st[2][3], base;
    ok_kg = DS(keypair_internal)(g_dpk, g_dsk, kd_keygen_seed) == 0 &&
            cmp(g_dpk, kd_keygen_pk, sizeof g_dpk) && cmp(g_dsk, kd_keygen_sk, sizeof g_dsk);
    ok_vy = DS(verify)(kd_verify_valid_signature, kd_verify_valid_message, KD_VERIFY_VALID_MESSAGE_LEN,
                       kd_verify_valid_context, KD_VERIFY_VALID_CONTEXT_LEN, kd_verify_valid_pk) == 0;
    ok_vn = DS(verify)(kd_verify_invalid_signature, kd_verify_invalid_message, KD_VERIFY_INVALID_MESSAGE_LEN,
                       kd_verify_invalid_context, KD_VERIFY_INVALID_CONTEXT_LEN, kd_verify_invalid_pk) != 0;
    h->result("kat_mldsa44_keygen_nist_acvp_1", 0, ok_kg, "pk and sk match");
    h->result("kat_mldsa44_verify_valid_nist_acvp", 0, ok_vy, "accepted");
    h->result("kat_mldsa44_verify_invalid_nist_acvp", 0, ok_vn, "rejected");
    if (!(ok_kg && ok_vy && ok_vn)) { h->puts_("ML-DSA-44 nosign KAT FAIL\r\n"); return 0; }
    h->puts_("ML-DSA-44 nosign KAT PASS (keygen, verify valid, verify invalid; signing not run)\r\n");
    for (int r = 0; r < 3; r++) {
        uint64_t t0, t1; int ok;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok = DS(keypair_internal)(g_dpk, g_dsk, kd_keygen_seed) == 0; t1 = h->now(); t[0][r] = t1 - t0; st[0][r] = h->stack_used() - base;
        ok_kg &= ok;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok = DS(verify)(kd_verify_valid_signature, kd_verify_valid_message, KD_VERIFY_VALID_MESSAGE_LEN,
                                       kd_verify_valid_context, KD_VERIFY_VALID_CONTEXT_LEN, kd_verify_valid_pk) == 0; t1 = h->now();
        t[1][r] = t1 - t0; st[1][r] = h->stack_used() - base; ok_vy &= ok;
    }
    {
        static const char *nm[2] = {"mldsa44_keygen", "mldsa44_verify"};
        static const char *sn[2] = {"stack_mldsa44_keygen", "stack_mldsa44_verify"};
        for (int i = 0; i < 2; i++) {
            char note[96], line[170];
            uint32_t smax = st[i][0] > st[i][1] ? st[i][0] : st[i][1]; if (st[i][2] > smax) smax = st[i][2];
            uint64_t md = med3(t[i][0], t[i][1], t[i][2]);
            snprintf(note, sizeof note, "runs %lu/%lu/%lu", (unsigned long)t[i][0], (unsigned long)t[i][1], (unsigned long)t[i][2]);
            h->result(nm[i], md, ok_kg && ok_vy, note);
            h->result(sn[i], smax, ok_kg && ok_vy, "net of caller frames");
            snprintf(line, sizeof line, "%s cycles %lu %lu %lu median %lu (%lu us at %lu MHz) stack %lu B\r\n", nm[i],
                     (unsigned long)t[i][0], (unsigned long)t[i][1], (unsigned long)t[i][2], (unsigned long)md,
                     (unsigned long)(md * 1000000ull / h->cpu_hz), (unsigned long)(h->cpu_hz / 1000000u), (unsigned long)smax);
            h->puts_(line);
        }
    }
    return 1;
}
