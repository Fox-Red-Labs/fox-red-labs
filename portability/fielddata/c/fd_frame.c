/* Field data: the generic frame. See fd_frame.h. No heap, no libc beyond memset. */
#include "fd_frame.h"
#include <string.h>

static int hexv(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int num(const char *s, size_t n, int32_t *out)
{
    int64_t v = 0; size_t i = 0; int neg = 0;
    if (!n) return -1;
    if (s[0] == '-') { neg = 1; i = 1; if (n == 1) return -1; }
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
        if (v > 2147483648LL) return -1;
    }
    if (!neg && v > 2147483647LL) return -1;
    *out = (int32_t)(neg ? -v : v);
    return 0;
}

static void put(fd_fields_t *f, uint8_t id, int32_t v)
{
    uint8_t i;
    for (i = 0; i < f->n; i++) if (f->id[i] == id) { f->v[i] = v; return; }
    if (f->n < FD_MAX_FIELDS) { f->id[f->n] = id; f->v[f->n] = v; f->n++; }
}

int fd_parse_line(const char *s, size_t len, fd_fields_t *out, int32_t *seqn)
{
    size_t star, i;
    uint8_t x = 0;
    int h1, h2;
    while (len && (s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
    if (len < 7 || s[0] != '$' || s[len - 3] != '*') return -1;
    star = len - 3;
    h1 = hexv(s[star + 1]); h2 = hexv(s[star + 2]);
    if (h1 < 0 || h2 < 0) return -1;
    for (i = 1; i < star; i++) x ^= (uint8_t)s[i];
    if (x != (uint8_t)(h1 * 16 + h2)) return -2;
    if (s[1] != 'F' || s[2] != 'D' || (star > 3 && s[3] != ',')) return -3;
    memset(out, 0, sizeof(*out));
    *seqn = -1;
    i = 4;
    while (i < star) {
        size_t ks = i, ke, vs;
        int32_t k = 0, v = 0;
        while (i < star && s[i] != '=' && s[i] != ',') i++;
        ke = i;
        if (i >= star || s[i] != '=') return -1;
        vs = ++i;
        while (i < star && s[i] != ',') i++;
        if (num(s + vs, i - vs, &v) == 0) {
            if (ke - ks == 1 && s[ks] == 'n') *seqn = v & 0xFFFF;
            else if (ke - ks == 3 && memcmp(s + ks, "app", 3) == 0) out->app = (uint8_t)v;
            else if (num(s + ks, ke - ks, &k) == 0 && k > 0 && k < 256) put(out, (uint8_t)k, v);
        }
        if (i < star) i++;
    }
    return out->app ? 0 : -1;
}

static uint8_t width(int32_t v) { return (v >= -128 && v <= 127) ? 1 : (v >= -32768 && v <= 32767) ? 2 : 4; }

size_t fd_build_ad(uint8_t *out, size_t cap, uint8_t seq, const fd_fields_t *f)
{
    size_t n = 8, i, p;                 /* the AD length counts its type byte (0xFF) and the 7-byte header */
    for (i = 0; i < f->n; i++) n += 2 + width(f->v[i]);
    if (n + 1 > cap || n > 255) return 0;
    p = 0;
    out[p++] = (uint8_t)n; out[p++] = 0xFF; out[p++] = 0xFF; out[p++] = 0xFF;
    out[p++] = 'F'; out[p++] = 'R'; out[p++] = FD_VT; out[p++] = f->app; out[p++] = seq;
    for (i = 0; i < f->n; i++) {
        uint8_t w = width(f->v[i]), b;
        uint32_t u = (uint32_t)f->v[i];
        out[p++] = f->id[i]; out[p++] = w;
        for (b = 0; b < w; b++) out[p++] = (uint8_t)(u >> (8 * b));
    }
    return p;
}

int fd_decode_msd(const uint8_t *m, size_t len, fd_fields_t *out, uint8_t *seq)
{
    size_t i = 7;
    if (len < 7 || m[0] != 0xFF || m[1] != 0xFF || m[2] != 'F' || m[3] != 'R' || m[4] != FD_VT) return -1;
    memset(out, 0, sizeof(*out));
    out->app = m[5]; *seq = m[6];
    while (i + 2 <= len) {
        uint8_t id = m[i], w = m[i + 1], b;
        uint32_t u = 0;
        if ((w != 1 && w != 2 && w != 4) || i + 2 + w > len) return -1;
        for (b = 0; b < w; b++) u |= (uint32_t)m[i + 2 + b] << (8 * b);
        if (w == 1) u = (uint32_t)(int32_t)(int8_t)u;
        else if (w == 2) u = (uint32_t)(int32_t)(int16_t)u;
        put(out, id, (int32_t)u);
        i += 2 + w;
    }
    return 0;
}
