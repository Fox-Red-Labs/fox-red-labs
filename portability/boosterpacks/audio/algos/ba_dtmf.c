#include "ba_dtmf.h"
#include "ba_algo_tables.h"

static const char keys[4][4] = { {'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'} };
static uint64_t min_energy = (uint64_t)BA_DTMF_N * 400u; /* sum of squares: RMS of about 20 codes in the signed 16-bit input */

void ba_dtmf_init(ba_dtmf_t *d)
{
    for (int i = 0; i < 8; i++) d->s1[i] = d->s2[i] = 0;
    d->n = 0; d->energy = 0; d->last = 0; d->armed = 1; d->blk = 0; d->share16 = 8;
}

static int64_t power(const ba_dtmf_t *d, int i)
{
    int64_t a = d->s1[i], b = d->s2[i];
    return a * a + b * b - ((int64_t)ba_dtmf_coef[i] * a >> 14) * b;
}

int ba_dtmf_push(ba_dtmf_t *d, int16_t x)
{
    for (int i = 0; i < 8; i++) {
        int32_t s = (int32_t)x + (int32_t)(((int64_t)ba_dtmf_coef[i] * d->s1[i]) >> 14) - d->s2[i];
        d->s2[i] = d->s1[i]; d->s1[i] = s;
    }
    d->energy += (uint64_t)((int32_t)x * x);
    if (++d->n < BA_DTMF_N) return 0;
    /* decide */
    int64_t p[8];
    for (int i = 0; i < 8; i++) { p[i] = power(d, i); if (p[i] < 0) p[i] = 0; }
    int r = 0, c = 4;
    for (int i = 1; i < 4; i++) if (p[i] > p[r]) r = i;
    for (int i = 5; i < 8; i++) if (p[i] > p[c]) c = i;
    int64_t r2 = 0, c2 = 0;
    for (int i = 0; i < 4; i++) if (i != r && p[i] > r2) r2 = p[i];
    for (int i = 4; i < 8; i++) if (i != c && p[i] > c2) c2 = p[i];
    uint64_t total = d->energy * (BA_DTMF_N / 2u); /* a pure tone of energy E puts E * N / 2 in its bin */
    char out = 0;
    if (d->energy >= min_energy
        && (uint64_t)(p[r] + p[c]) * 16u >= total * d->share16 /* share16/16 of the energy in the two tones (each tone's Goertzel power is E*N/2 for a pure tone) */
        && p[r] >= 4 * r2 && p[c] >= 4 * c2                 /* 6 dB over the runner-up in each group */
        && p[r] <= 6 * p[c] && p[c] <= 6 * p[r])            /* twist within 8 dB */
        out = keys[r][c - 4];
    d->blk = out;
    for (int i = 0; i < 8; i++) d->s1[i] = d->s2[i] = 0;
    d->n = 0; d->energy = 0;
    return 1;
}

char ba_dtmf_block_digit(const ba_dtmf_t *d) { return d->blk; }

char ba_dtmf_stream(ba_dtmf_t *d)
{
    char b = d->blk;
    if (!b) { d->last = 0; d->armed = 1; return 0; }
    if (b == d->last) { if (d->armed) { d->armed = 0; return b; } return 0; }
    d->last = b; d->armed = 1;
    return 0;
}
