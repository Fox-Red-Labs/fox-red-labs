#include <stdio.h>
#include <string.h>
#include "pqc_join.h"
#include "pqc_sym.h"
#include "pqc_kat_data.h"
#include "mlkem_native.h"

#define NS(x) PQCP_MLKEM_NATIVE_MLKEM768_##x
#define INFO_STR "foxredlabs pqc-join v1 aes-128-gcm"
#define AAD_STR "pqc-join-v1"
#define LINE_MAX_BYTES 2600

static uint8_t g_pk[PQC_PK_BYTES], g_sk[PQC_SK_BYTES], g_ct[PQC_CT_BYTES], g_ss[PQC_SS_BYTES];
static char g_line[LINE_MAX_BYTES];

void pqc_hex(char *dst, const uint8_t *src, size_t n)
{
    static const char x[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { dst[2 * i] = x[src[i] >> 4]; dst[2 * i + 1] = x[src[i] & 15]; }
    dst[2 * n] = 0;
}

static int unhex(const char *s, size_t nchars, uint8_t *out)
{
    for (size_t i = 0; i < nchars; i++) {
        char c = s[i]; int v;
        if (c >= '0' && c <= '9') v = c - '0'; else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10; else return -1;
        if (i & 1) out[i / 2] = (uint8_t)(out[i / 2] | v); else out[i / 2] = (uint8_t)(v << 4);
    }
    return 0;
}

static int cmp(const uint8_t *a, const uint8_t *b, size_t n) { return memcmp(a, b, n) == 0; }

/* ---------- frames ---------- */
static void send_frame(const pqc_hal_t *h, const char *tag, const uint8_t *d, size_t n)
{
    char head[32], tail[16];
    snprintf(head, sizeof head, "@%s %u ", tag, (unsigned)n);
    h->puts_(head);
    for (size_t off = 0; off < n; off += 64) {      /* hex in 128-character pieces: the line buffer stays small */
        char piece[129]; size_t k = n - off > 64 ? 64 : n - off;
        pqc_hex(piece, d + off, k);
        h->puts_(piece);
    }
    snprintf(tail, sizeof tail, " *%08lx\r\n", (unsigned long)pqc_crc32(d, n));
    h->puts_(tail);
}

/* Parse g_line as "@TAG n hex *crc" into out (capacity cap). Returns n, or -1 when it is not that frame. */
static int parse_frame(const char *tag, uint8_t *out, size_t cap)
{
    char *p = g_line; size_t tl = strlen(tag);
    unsigned long n = 0, crc = 0;
    if (p[0] != '@' || strncmp(p + 1, tag, tl) != 0 || p[1 + tl] != ' ') return -1;
    p += 2 + tl;
    while (*p >= '0' && *p <= '9') n = n * 10 + (unsigned long)(*p++ - '0');
    if (*p++ != ' ' || n == 0 || n > cap) return -1;
    char *hex = p;
    if (strlen(hex) < 2 * n + 10 || hex[2 * n] != ' ' || hex[2 * n + 1] != '*') return -1;
    if (unhex(hex, 2 * n, out) != 0) return -1;
    for (char *q = hex + 2 * n + 2; *q && *q != '\r' && *q != '\n'; q++) {
        char c = *q; int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) return -1;
        crc = (crc << 4) | (unsigned long)v;
    }
    if ((uint32_t)crc != pqc_crc32(out, n)) return -1;
    return (int)n;
}

/* ---------- known-answer tests ---------- */
int pqc_kat(const pqc_hal_t *h)
{
    static uint8_t ek[PQC_PK_BYTES], dk[PQC_SK_BYTES], c[PQC_CT_BYTES], k[PQC_SS_BYTES], k2[PQC_SS_BYTES];
    uint8_t coins[64];
    int ok_kg, ok_enc, ok_dec, ok_rej, ok_pk;

    memcpy(coins, kat_keygen_d, 32); memcpy(coins + 32, kat_keygen_z, 32);
    ok_kg = NS(keypair_derand)(ek, dk, coins) == 0 && cmp(ek, kat_keygen_ek, sizeof ek) && cmp(dk, kat_keygen_dk, sizeof dk);
    ok_pk = NS(check_pk)(kat_keygen_ek) == 0 && NS(check_sk)(kat_keygen_dk) == 0;
    ok_enc = NS(enc_derand)(c, k, kat_encaps_ek, kat_encaps_m) == 0 && cmp(c, kat_encaps_c, sizeof c) && cmp(k, kat_encaps_k, sizeof k);
    ok_dec = NS(dec)(k, kat_decaps_c, kat_decaps_dk) == 0 && cmp(k, kat_decaps_k, sizeof k);
    /* implicit rejection: one flipped ciphertext bit must give a different secret, not an error */
    ok_rej = 0;
    {   /* the encaps test's dk is not in the data; use the keygen key pair with its own encapsulation */
        uint8_t m[32]; memcpy(m, kat_encaps_m, 32);
        if (NS(enc_derand)(c, k, kat_keygen_ek, m) == 0 && NS(dec)(k2, c, kat_keygen_dk) == 0 && cmp(k, k2, 32)) {
            c[0] ^= 1;
            ok_rej = NS(dec)(k2, c, kat_keygen_dk) == 0 && !cmp(k, k2, 32);
        }
    }
    h->result("kat_keygen_nist_acvp_26", 0, ok_kg, "ek and dk match");
    h->result("kat_encaps_nist_acvp", 0, ok_enc, "ciphertext and secret match");
    h->result("kat_decaps_nist_acvp", 0, ok_dec, "secret matches");
    h->result("kat_implicit_rejection", 0, ok_rej, "flipped ciphertext bit gives a different secret");
    h->result("kat_key_checks", 0, ok_pk, "check_pk and check_sk accept the NIST keys");
    { char t[100]; snprintf(t, sizeof t, "KAT keygen %s, encaps %s, decaps %s, rejection %s, key checks %s\r\n",
        ok_kg ? "PASS" : "FAIL", ok_enc ? "PASS" : "FAIL", ok_dec ? "PASS" : "FAIL", ok_rej ? "PASS" : "FAIL", ok_pk ? "PASS" : "FAIL"); h->puts_(t); }
    {
        int all = ok_kg && ok_enc && ok_dec && ok_rej && ok_pk;
        h->puts_(all ? "KAT PASS\r\n" : "KAT FAIL\r\n");
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

void pqc_timing(const pqc_hal_t *h)
{
    static uint8_t pk[PQC_PK_BYTES], sk[PQC_SK_BYTES], ct[PQC_CT_BYTES], ss[PQC_SS_BYTES], ss2[PQC_SS_BYTES];
    uint8_t coins[64], m[32];
    uint64_t t[3][3]; uint32_t st[3][3], base;
    int ok = 1;
    memcpy(coins, kat_keygen_d, 32); memcpy(coins + 32, kat_keygen_z, 32); memcpy(m, kat_encaps_m, 32);
    for (int r = 0; r < 3; r++) {
        uint64_t t0, t1;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok &= NS(keypair_derand)(pk, sk, coins) == 0; t1 = h->now(); t[0][r] = t1 - t0; st[0][r] = h->stack_used() - base;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok &= NS(enc_derand)(ct, ss, pk, m) == 0; t1 = h->now(); t[1][r] = t1 - t0; st[1][r] = h->stack_used() - base;
        h->stack_paint(); base = h->stack_used();
        t0 = h->now(); ok &= NS(dec)(ss2, ct, sk) == 0; t1 = h->now(); t[2][r] = t1 - t0; st[2][r] = h->stack_used() - base;
        ok &= cmp(ss, ss2, 32);
    }
    static const char *nm[3] = {"mlkem768_keygen", "mlkem768_encaps", "mlkem768_decaps"};
    static const char *sn[3] = {"stack_keygen", "stack_encaps", "stack_decaps"};
    for (int i = 0; i < 3; i++) {
        char note[96], line[160];
        uint32_t smax = st[i][0] > st[i][1] ? st[i][0] : st[i][1]; if (st[i][2] > smax) smax = st[i][2];
        snprintf(note, sizeof note, "runs %lu/%lu/%lu", (unsigned long)t[i][0], (unsigned long)t[i][1], (unsigned long)t[i][2]);
        h->result(nm[i], med3(t[i][0], t[i][1], t[i][2]), ok, note);
        snprintf(note, sizeof note, "net of caller frames");
        h->result(sn[i], smax, ok, note);
        snprintf(line, sizeof line, "%s cycles %lu %lu %lu median %lu (%lu us at %lu MHz) stack %lu B\r\n", nm[i],
                 (unsigned long)t[i][0], (unsigned long)t[i][1], (unsigned long)t[i][2], (unsigned long)med3(t[i][0], t[i][1], t[i][2]),
                 (unsigned long)(med3(t[i][0], t[i][1], t[i][2]) * 1000000ull / h->cpu_hz), (unsigned long)(h->cpu_hz / 1000000u), (unsigned long)smax);
        h->puts_(line);
    }
}

/* ---------- the join ---------- */
int pqc_join(const pqc_hal_t *h)
{
    uint8_t coins[64], key[16], chk[32], msg[300], pt[300];
    uint64_t t0, t1, t2, t3, t4, t5, t6;
    int n;

    h->puts_("join: making an ML-KEM-768 key pair\r\n");
    t0 = h->now();
    h->rand(coins, 64);
    t1 = h->now();
    if (NS(keypair_derand)(g_pk, g_sk, coins) != 0) { h->puts_("join: keygen failed\r\n"); return 0; }
    t2 = h->now();
    h->result("join_trng_64_bytes", t1 - t0, 1, "64 random bytes for the key pair");
    h->result("join_keygen", t2 - t1, 1, "ML-KEM-768 key pair from the random bytes");
    memset(coins, 0, sizeof coins);
    h->puts_("join: key pair ready, offering the public key\r\n");
    for (;;) {                                              /* offer the key until the Mac answers */
        send_frame(h, "PK", g_pk, PQC_PK_BYTES);
        int len = h->getline(g_line, sizeof g_line, 4000);
        if (len < 0) continue;
        n = parse_frame("CT", g_ct, sizeof g_ct);
        if (n == PQC_CT_BYTES) break;
        if (len > 0 && g_line[0] == '@') h->puts_("join: bad CT frame, waiting\r\n");
    }
    t3 = h->now();
    if (NS(dec)(g_ss, g_ct, g_sk) != 0) { h->puts_("join: decaps failed\r\n"); return 0; }
    pqc_hkdf_sha256(g_ss, 32, (const uint8_t *)INFO_STR, strlen(INFO_STR), key, 16);
    pqc_sha256(key, 16, chk);
    t4 = h->now();
    h->result("join_decaps_kdf", t4 - t3, 1, "decapsulate, HKDF-SHA256 to the AES key, key check");
    for (;;) {                                              /* show the key check until the message arrives */
        send_frame(h, "KEY", chk, 4);
        int len = h->getline(g_line, sizeof g_line, 4000);
        if (len < 0) continue;
        n = parse_frame("MSG", msg, sizeof msg);
        if (n > 28) break;
    }
    {
        int plen = n - 28, bad;
        t5 = h->now();
        bad = pqc_gcm_decrypt(key, msg, (const uint8_t *)AAD_STR, strlen(AAD_STR), msg + 12, (size_t)plen, msg + 12 + plen, pt);
        uint8_t done = bad ? 0 : 1;
        t6 = h->now();
        { char note[48]; snprintf(note, sizeof note, "AES-128-GCM, %d bytes", plen); h->result("join_decrypt", t6 - t5, !bad, note); }
        if (!bad) { pt[plen] = 0; for (int i = 0; i < plen; i++) if (pt[i] < 32 || pt[i] > 126) pt[i] = '?'; }
        if (!bad) { char t[400]; snprintf(t, sizeof t, "DECRYPTED (%d bytes, tag ok): %s\r\n", plen, (const char *)pt); h->puts_(t); }
        else h->puts_("DECRYPT FAILED: tag mismatch\r\n");
        send_frame(h, "DONE", &done, 1);
        memset(key, 0, sizeof key); memset(g_ss, 0, sizeof g_ss); memset(g_sk, 0, sizeof g_sk);
        return !bad;
    }
}
