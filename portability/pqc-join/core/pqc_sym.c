#include <string.h>
#include "pqc_sym.h"
#include "pqc_tables.h"

/* ---------- SHA-256 ---------- */
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
typedef struct { uint32_t h[8]; uint8_t buf[64]; size_t n; uint64_t total; } sha_t;

static void sha_block(sha_t *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) | ((uint32_t)p[4 * i + 2] << 8) | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha256_k[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(sha_t *s)
{
    memcpy(s->h, sha256_h0, sizeof s->h);
    s->n = 0; s->total = 0;
}

static void sha_update(sha_t *s, const uint8_t *m, size_t n)
{
    s->total += n;
    while (n) {
        size_t k = 64 - s->n;
        if (k > n) k = n;
        memcpy(s->buf + s->n, m, k);
        s->n += k; m += k; n -= k;
        if (s->n == 64) { sha_block(s, s->buf); s->n = 0; }
    }
}

static void sha_final(sha_t *s, uint8_t out[32])
{
    uint64_t bits = s->total * 8;
    uint8_t pad = 0x80, z = 0, len[8];
    sha_update(s, &pad, 1);
    while (s->n != 56) sha_update(s, &z, 1);
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_update(s, len, 8);
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(s->h[i] >> 24); out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8); out[4 * i + 3] = (uint8_t)s->h[i];
    }
}

void pqc_sha256(const uint8_t *m, size_t n, uint8_t out[32])
{
    sha_t s;
    sha_init(&s); sha_update(&s, m, n); sha_final(&s, out);
}

void pqc_hmac_sha256(const uint8_t *key, size_t kn, const uint8_t *m, size_t n, uint8_t out[32])
{
    uint8_t k[64] = {0}, ip[64], op[64], inner[32];
    sha_t s;
    if (kn > 64) pqc_sha256(key, kn, k); else if (kn) memcpy(k, key, kn);
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    sha_init(&s); sha_update(&s, ip, 64); sha_update(&s, m, n); sha_final(&s, inner);
    sha_init(&s); sha_update(&s, op, 64); sha_update(&s, inner, 32); sha_final(&s, out);
}

void pqc_hkdf_sha256(const uint8_t *ikm, size_t ikm_n, const uint8_t *info, size_t info_n, uint8_t *okm, size_t okm_n)
{
    uint8_t zero[32] = {0}, prk[32], msg[128], t[32];
    pqc_hmac_sha256(zero, 32, ikm, ikm_n, prk);          /* extract, salt = 32 zero bytes (RFC 5869: empty salt) */
    if (info_n > sizeof msg - 1) info_n = sizeof msg - 1;
    memcpy(msg, info, info_n);
    msg[info_n] = 1;                                       /* expand, first block only */
    pqc_hmac_sha256(prk, 32, msg, info_n + 1, t);
    memcpy(okm, t, okm_n > 32 ? 32 : okm_n);
}

uint32_t pqc_crc32(const uint8_t *m, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *m++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* ---------- AES-128 (encrypt only) ---------- */
static uint8_t xt(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }

static void aes_expand(const uint8_t key[16], uint8_t rk[176])
{
    uint8_t rc = 1;
    memcpy(rk, key, 16);
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
        if (i % 16 == 0) {
            uint8_t u = t[0];
            t[0] = (uint8_t)(aes_sbox[t[1]] ^ rc); t[1] = aes_sbox[t[2]]; t[2] = aes_sbox[t[3]]; t[3] = aes_sbox[u];
            rc = xt(rc);
        }
        for (int j = 0; j < 4; j++) rk[i + j] = (uint8_t)(rk[i - 16 + j] ^ t[j]);
    }
}

static void aes_enc(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16])
{
    uint8_t s[16], t[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
    for (int r = 1; r <= 10; r++) {
        for (int i = 0; i < 16; i++) t[i] = aes_sbox[s[i]];
        /* shift rows */
        s[0] = t[0]; s[1] = t[5]; s[2] = t[10]; s[3] = t[15];
        s[4] = t[4]; s[5] = t[9]; s[6] = t[14]; s[7] = t[3];
        s[8] = t[8]; s[9] = t[13]; s[10] = t[2]; s[11] = t[7];
        s[12] = t[12]; s[13] = t[1]; s[14] = t[6]; s[15] = t[11];
        if (r < 10) {
            for (int c = 0; c < 16; c += 4) {
                uint8_t a0 = s[c], a1 = s[c + 1], a2 = s[c + 2], a3 = s[c + 3], x = (uint8_t)(a0 ^ a1 ^ a2 ^ a3);
                t[c] = (uint8_t)(a0 ^ x ^ xt((uint8_t)(a0 ^ a1)));
                t[c + 1] = (uint8_t)(a1 ^ x ^ xt((uint8_t)(a1 ^ a2)));
                t[c + 2] = (uint8_t)(a2 ^ x ^ xt((uint8_t)(a2 ^ a3)));
                t[c + 3] = (uint8_t)(a3 ^ x ^ xt((uint8_t)(a3 ^ a0)));
            }
            memcpy(s, t, 16);
        }
        for (int i = 0; i < 16; i++) s[i] ^= rk[16 * r + i];
    }
    memcpy(out, s, 16);
}

/* ---------- GCM ---------- */
static void gmul(uint8_t x[16], const uint8_t h[16])
{
    uint8_t z[16] = {0}, v[16];
    memcpy(v, h, 16);
    for (int i = 0; i < 128; i++) {
        if ((x[i / 8] >> (7 - i % 8)) & 1) for (int j = 0; j < 16; j++) z[j] ^= v[j];
        int lsb = v[15] & 1;
        for (int j = 15; j > 0; j--) v[j] = (uint8_t)((v[j] >> 1) | (v[j - 1] << 7));
        v[0] >>= 1;
        if (lsb) v[0] ^= 0xe1;
    }
    memcpy(x, z, 16);
}

static void ghash_upd(uint8_t y[16], const uint8_t h[16], const uint8_t *d, size_t n)
{
    while (n) {
        size_t k = n > 16 ? 16 : n;
        for (size_t i = 0; i < k; i++) y[i] ^= d[i];
        gmul(y, h);
        d += k; n -= k;
    }
}

static void ctr_inc(uint8_t c[16])
{
    for (int i = 15; i >= 12; i--) if (++c[i]) break;
}

static void gcm_core(const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad, size_t aad_n,
                     const uint8_t *in, size_t n, uint8_t *out, uint8_t tag[16], int decrypt)
{
    uint8_t rk[176], h[16] = {0}, y[16] = {0}, j0[16], c[16], ks[16], lens[16], ek[16];
    aes_expand(key, rk);
    aes_enc(rk, h, h);
    memcpy(j0, nonce, 12); j0[12] = j0[13] = j0[14] = 0; j0[15] = 1;
    ghash_upd(y, h, aad, aad_n);
    if (decrypt) ghash_upd(y, h, in, n);
    memcpy(c, j0, 16);
    for (size_t off = 0; off < n; off += 16) {
        size_t k = n - off > 16 ? 16 : n - off;
        ctr_inc(c);
        aes_enc(rk, c, ks);
        for (size_t i = 0; i < k; i++) out[off + i] = (uint8_t)(in[off + i] ^ ks[i]);
    }
    if (!decrypt) ghash_upd(y, h, out, n);
    memset(lens, 0, 16);
    for (int i = 0; i < 8; i++) { lens[7 - i] = (uint8_t)(((uint64_t)aad_n * 8) >> (8 * i)); lens[15 - i] = (uint8_t)(((uint64_t)n * 8) >> (8 * i)); }
    ghash_upd(y, h, lens, 16);
    aes_enc(rk, j0, ek);
    for (int i = 0; i < 16; i++) tag[i] = (uint8_t)(y[i] ^ ek[i]);
}

void pqc_gcm_encrypt(const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad, size_t aad_n,
                     const uint8_t *in, size_t n, uint8_t *out, uint8_t tag[16])
{
    gcm_core(key, nonce, aad, aad_n, in, n, out, tag, 0);
}

int pqc_gcm_decrypt(const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad, size_t aad_n,
                    const uint8_t *in, size_t n, const uint8_t tag[16], uint8_t *out)
{
    uint8_t t[16], diff = 0;
    gcm_core(key, nonce, aad, aad_n, in, n, out, t, 1);
    for (int i = 0; i < 16; i++) diff |= (uint8_t)(t[i] ^ tag[i]);
    if (diff) { memset(out, 0, n); return -1; }
    return 0;
}
