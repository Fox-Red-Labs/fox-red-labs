#include "ba_adpcm.h"

static const int16_t step_tab[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767 };
static const int8_t idx_tab[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

int16_t ba_adpcm_decode(int32_t *pred, int8_t *idx, uint8_t nib)
{
    int32_t st = step_tab[*idx], d = st >> 3;
    if (nib & 4) d += st;
    if (nib & 2) d += st >> 1;
    if (nib & 1) d += st >> 2;
    *pred += (nib & 8) ? -d : d;
    if (*pred > 32767) *pred = 32767;
    if (*pred < -32768) *pred = -32768;
    int32_t i = *idx + idx_tab[nib & 15];
    *idx = (int8_t)(i < 0 ? 0 : i > 88 ? 88 : i);
    return (int16_t)*pred;
}

static int16_t take(ba_adpcm_t *p)
{
    if (p->pos >= p->t->nsamples) { p->done = 1; return 0; }
    uint8_t b = p->t->data[p->pos >> 1];
    uint8_t nib = (p->pos & 1u) ? (uint8_t)(b >> 4) : (uint8_t)(b & 15u);
    p->pos++;
    return ba_adpcm_decode(&p->pred, &p->idx, nib);
}

void ba_adpcm_start(ba_adpcm_t *p, const ba_track_t *t, uint32_t out_rate)
{
    p->t = t; p->pos = 0; p->pred = 0; p->idx = 0; p->done = 0;
    p->step = (uint32_t)(((uint64_t)t->rate << 16) / out_rate);
    p->frac = 0;
    p->s0 = take(p); p->s1 = take(p);
}

int16_t ba_adpcm_next(ba_adpcm_t *p)
{
    if (p->done) return 0;
    int32_t y = p->s0 + (int32_t)(((int64_t)(p->s1 - p->s0) * (int32_t)p->frac) >> 16);
    p->frac += p->step;
    while (p->frac >= 65536u) { p->frac -= 65536u; p->s0 = p->s1; p->s1 = take(p); }
    return (int16_t)y;
}
