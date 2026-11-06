/* Over-the-air model packages: see tt_mdl.h. */
#include <string.h>
#include "tt_mdl.h"

uint16_t tt_mdl_crc16(const uint8_t *p, size_t n, uint16_t crc)
{
    size_t i;
    int b;
    for (i = 0; i < n; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t tt_mdl_crc(const tt_mdl_t *m)
{
    uint8_t h[4] = { m->kind, (uint8_t)m->ver, (uint8_t)(m->ver >> 8), m->n };
    return tt_mdl_crc16(m->node, (size_t)m->n * TT_MDL_NODE_BYTES, tt_mdl_crc16(h, 4, 0xFFFF));
}

static int8_t nf(const tt_mdl_t *m, int i) { return (int8_t)m->node[i * TT_MDL_NODE_BYTES]; }
static uint16_t nthr(const tt_mdl_t *m, int i) { const uint8_t *q = &m->node[i * TT_MDL_NODE_BYTES + 1]; return (uint16_t)(q[0] | q[1] << 8); }
static uint8_t nl(const tt_mdl_t *m, int i) { return m->node[i * TT_MDL_NODE_BYTES + 3]; }
static uint8_t nr(const tt_mdl_t *m, int i) { return m->node[i * TT_MDL_NODE_BYTES + 4]; }

int tt_mdl_valid(const tt_mdl_t *m)
{
    int i;
    if (m->n == 0 || m->n > TT_MDL_MAX_NODES || m->kind > 1) return 0;
    for (i = 0; i < m->n; i++) {
        if (nf(m, i) < 0) {
            if (nthr(m, i) > 3) return 0;                       /* four classes */
        } else {
            /* children point forward (the trainer writes a node before its children), so every walk ends */
            if (nf(m, i) > 6 || nl(m, i) <= i || nr(m, i) <= i || nl(m, i) >= m->n || nr(m, i) >= m->n) return 0;
        }
    }
    return 1;
}

int tt_mdl_write(tt_mdl_store_t *s, const uint8_t *w, uint16_t len)
{
    int r = TT_MDL_OK;
    if (len < 1) r = TT_MDL_ERR_LEN;
    else if (w[0] == 1) {
        if (len != 7 || w[1] > 1 || w[4] == 0 || w[4] > TT_MDL_MAX_NODES) r = TT_MDL_ERR_LEN;
        else {
            tt_mdl_t *m = &s->slot[w[1]];
            s->live[w[1]] = 0;                                  /* the built-in model covers this kind meanwhile */
            s->gen[w[1]]++;
            memset(m, 0, sizeof(*m));
            m->kind = w[1]; m->ver = (uint16_t)(w[2] | w[3] << 8); m->n = w[4]; m->crc = (uint16_t)(w[5] | w[6] << 8);
            s->loading = (uint8_t)(w[1] + 1); s->got = 0;
        }
    } else if (w[0] == 2) {
        uint16_t off;
        if (!s->loading) r = TT_MDL_ERR_STATE;
        else if (len < 4 || len > 3 + TT_MDL_CHUNK) r = TT_MDL_ERR_LEN;
        else {
            tt_mdl_t *m = &s->slot[s->loading - 1];
            off = (uint16_t)(w[1] | w[2] << 8);
            if (off != s->got || off + (len - 3) > m->n * TT_MDL_NODE_BYTES) r = TT_MDL_ERR_LEN;   /* in order, no gaps */
            else { memcpy(&m->node[off], &w[3], (size_t)(len - 3)); s->got = (uint16_t)(off + len - 3); }
        }
    } else if (w[0] == 3) {
        if (!s->loading || len != 17) r = TT_MDL_ERR_STATE;
        else {
            uint8_t k = (uint8_t)(s->loading - 1);
            tt_mdl_t *m = &s->slot[k];
            s->loading = 0;
            if (s->got != m->n * TT_MDL_NODE_BYTES) r = TT_MDL_ERR_LEN;
            else if (tt_mdl_crc(m) != m->crc) r = TT_MDL_ERR_CRC;
            else if (!tt_mdl_valid(m)) r = TT_MDL_ERR_TREE;
            else { memcpy(m->mac, &w[1], 16); s->result = TT_MDL_PENDING; return 1; }
        }
    } else r = TT_MDL_ERR_OP;
    s->result = (uint8_t)r;
    return r ? 10 + r : 0;
}

static uint16_t feat(const tt_pull_t *p, int f)
{
    switch (f) {
    case 0: return p->dur10;  case 1: return p->ravg;  case 2: return p->rmax;  case 3: return p->iavg;
    case 4: return p->ipk;    case 5: return p->vmin;  default: return (uint16_t)(p->tc + 128);
    }
}

int tt_mdl_classify(const tt_mdl_store_t *s, const tt_pull_t *p)
{
    int k = (p->iavg == 0 && p->ipk == 0) ? 1 : 0, i = 0, steps;
    const tt_mdl_t *m = &s->slot[k];
    if (!s->live[k]) return -1;
    for (steps = 0; steps < TT_MDL_MAX_NODES; steps++) {
        if (nf(m, i) < 0) return nthr(m, i);
        i = (feat(p, nf(m, i)) <= nthr(m, i)) ? nl(m, i) : nr(m, i);
    }
    return -1;
}

uint16_t tt_mdl_version(const tt_mdl_store_t *s) { return s->live[0] ? s->slot[0].ver : 0; }

void tt_mdl_status(const tt_mdl_store_t *s, uint8_t out[8])
{
    uint16_t v0 = s->live[0] ? s->slot[0].ver : 0, v1 = s->live[1] ? s->slot[1].ver : 0;
    out[0] = (uint8_t)v0; out[1] = (uint8_t)(v0 >> 8); out[2] = (uint8_t)v1; out[3] = (uint8_t)(v1 >> 8);
    out[4] = (uint8_t)(s->live[0] | s->live[1] << 1); out[5] = s->result;
    out[6] = (uint8_t)s->got; out[7] = (uint8_t)(s->got >> 8);
}

/* ---------- SHA-256 and HMAC, small and slow: a model install checks one 200-byte package ---------- */
typedef struct { uint32_t h[8]; uint8_t b[64]; uint32_t nb; uint64_t len; } sha_t;

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(sha_t *c)
{
    uint32_t w[16], a[8], t1, t2;
    int i;
    for (i = 0; i < 16; i++) w[i] = (uint32_t)c->b[4 * i] << 24 | (uint32_t)c->b[4 * i + 1] << 16 | (uint32_t)c->b[4 * i + 2] << 8 | c->b[4 * i + 3];
    for (i = 0; i < 8; i++) a[i] = c->h[i];
    for (i = 0; i < 64; i++) {
        uint32_t wi;
        if (i < 16) wi = w[i];
        else {
            uint32_t w15 = w[(i + 1) & 15], w2 = w[(i + 14) & 15];
            wi = w[i & 15] += (ROR(w15, 7) ^ ROR(w15, 18) ^ (w15 >> 3)) + w[(i + 9) & 15] + (ROR(w2, 17) ^ ROR(w2, 19) ^ (w2 >> 10));
        }
        t1 = a[7] + (ROR(a[4], 6) ^ ROR(a[4], 11) ^ ROR(a[4], 25)) + ((a[4] & a[5]) ^ (~a[4] & a[6])) + K[i] + wi;
        t2 = (ROR(a[0], 2) ^ ROR(a[0], 13) ^ ROR(a[0], 22)) + ((a[0] & a[1]) ^ (a[0] & a[2]) ^ (a[1] & a[2]));
        a[7] = a[6]; a[6] = a[5]; a[5] = a[4]; a[4] = a[3] + t1; a[3] = a[2]; a[2] = a[1]; a[1] = a[0]; a[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) c->h[i] += a[i];
    c->nb = 0;
}

static void sha_init(sha_t *c)
{
    static const uint32_t h0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(c->h, h0, sizeof(h0)); c->nb = 0; c->len = 0;
}

static void sha_byte(sha_t *c, uint8_t x) { c->b[c->nb++] = x; c->len += 8; if (c->nb == 64) sha_block(c); }
static void sha_put(sha_t *c, const uint8_t *p, size_t n) { while (n--) sha_byte(c, *p++); }

static void sha_done(sha_t *c, uint8_t out[32])
{
    uint64_t len = c->len;
    int i;
    sha_byte(c, 0x80);
    while (c->nb != 56) sha_byte(c, 0);
    for (i = 7; i >= 0; i--) { c->b[c->nb++] = (uint8_t)(len >> (8 * i)); }
    sha_block(c);
    for (i = 0; i < 32; i++) out[i] = (uint8_t)(c->h[i / 4] >> (24 - 8 * (i % 4)));
}

void tt_hmac16(const uint8_t key[TT_MDL_KEY_LEN], const uint8_t *a, size_t na, const uint8_t *b, size_t nb, uint8_t out[16])
{
    sha_t c;
    uint8_t d[32];
    int i;
    sha_init(&c);
    for (i = 0; i < 64; i++) sha_byte(&c, (uint8_t)((i < TT_MDL_KEY_LEN ? key[i] : 0) ^ 0x36));
    sha_put(&c, a, na);
    sha_put(&c, b, nb);
    sha_done(&c, d);
    sha_init(&c);
    for (i = 0; i < 64; i++) sha_byte(&c, (uint8_t)((i < TT_MDL_KEY_LEN ? key[i] : 0) ^ 0x5c));
    sha_put(&c, d, 32);
    sha_done(&c, d);
    memcpy(out, d, 16);
}

void tt_mdl_mac(const tt_mdl_t *m, const uint8_t key[TT_MDL_KEY_LEN], uint8_t out[16])
{
    uint8_t h[4] = { m->kind, (uint8_t)m->ver, (uint8_t)(m->ver >> 8), m->n };
    tt_hmac16(key, h, 4, m->node, (size_t)m->n * TT_MDL_NODE_BYTES, out);
}

int tt_cmd_check(const uint8_t key[TT_MDL_KEY_LEN], const uint8_t addr[6], const uint8_t w[TT_CMD_LEN], uint32_t last_counter,
                 uint8_t *cmd, uint8_t *duty, uint32_t *counter)
{
    uint8_t pre[7], mac[16], diff = 0;
    uint32_t ctr = (uint32_t)w[2] | (uint32_t)w[3] << 8 | (uint32_t)w[4] << 16 | (uint32_t)w[5] << 24;
    int i;
    pre[0] = 0xC0;                                   /* commands never share a MAC with a model package */
    memcpy(&pre[1], addr, 6);
    tt_hmac16(key, pre, 7, w, 6, mac);
    for (i = 0; i < 12; i++) diff |= (uint8_t)(mac[i] ^ w[6 + i]);
    if (diff) return TT_CMD_ERR_MAC;
    if (ctr <= last_counter) return TT_CMD_ERR_OLD;  /* a replay, or an older command arriving late */
    if (w[0] < 1 || w[0] > 5 || (w[0] == 4 && w[1] > 40) || (w[0] < 4 && w[1])) return TT_CMD_ERR_BAD;
    *cmd = w[0]; *duty = w[1]; *counter = ctr;
    return TT_CMD_OK;
}

static int mac_ok(const tt_mdl_t *m, const uint8_t key[TT_MDL_KEY_LEN])
{
    uint8_t want[16], diff = 0;
    int i;
    tt_mdl_mac(m, key, want);
    for (i = 0; i < 16; i++) diff |= (uint8_t)(want[i] ^ m->mac[i]);      /* same time whatever the first wrong byte */
    return diff == 0;
}

int tt_mdl_trusted(const tt_mdl_t *m, uint8_t k, const uint8_t key[TT_MDL_KEY_LEN])
{
    return m->kind == k && tt_mdl_crc(m) == m->crc && tt_mdl_valid(m) && mac_ok(m, key);
}

int tt_mdl_finish(tt_mdl_store_t *s, uint8_t k, const uint8_t key[TT_MDL_KEY_LEN])
{
    if (k > 1 || !tt_mdl_trusted(&s->slot[k], k, key)) { s->result = TT_MDL_ERR_AUTH; return TT_MDL_ERR_AUTH; }
    s->live[k] = 1;
    s->result = TT_MDL_OK;
    return TT_MDL_OK;
}
