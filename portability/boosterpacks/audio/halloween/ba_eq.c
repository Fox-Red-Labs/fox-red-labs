#include "ba_eq.h"
#include "ba_eq_coeffs.h"

static int32_t bq(ba_bq_t *s, const int32_t c[5], int32_t x)
{
    int64_t acc = (int64_t)c[0] * x + (int64_t)c[1] * s->x1 + (int64_t)c[2] * s->x2 - (int64_t)c[3] * s->y1 - (int64_t)c[4] * s->y2;
    int32_t y = (int32_t)(acc >> 14);
    s->x2 = s->x1; s->x1 = x; s->y2 = s->y1; s->y1 = y;
    return y;
}

void ba_eq_init(ba_eq_t *e)
{
    uint8_t *p = (uint8_t *)e; for (unsigned i = 0; i < sizeof *e; i++) p[i] = 0;
    e->pre_q8 = 256;    /* 0 dB: the 300 Hz high-pass frees about as much headroom as the boosts take; the limiter does the rest */
    e->bass_q8 = 400;   /* how much of the generated harmonics is added */
}

int32_t ba_eq_next(ba_eq_t *e, int32_t x)
{
    x = (x * e->pre_q8) >> 8;
    /* bass enhancer: the band below 300 Hz, full-wave rectified (even harmonics), then band-limited to 350-1,200 Hz */
    int32_t lo = bq(&e->lp[1], bass_lp_q14[1], bq(&e->lp[0], bass_lp_q14[0], x));
    int32_t h = lo < 0 ? -lo : lo;
    h = bq(&e->hb[1], harm_bp_q14[1], bq(&e->hb[0], harm_bp_q14[0], h));
    int32_t y = x;
    for (unsigned i = 0; i < sizeof eq_q14 / sizeof eq_q14[0] && i < BA_EQ_MAX; i++) y = bq(&e->eq[i], eq_q14[i], y);
    return y + ((h * e->bass_q8) >> 8);
}
