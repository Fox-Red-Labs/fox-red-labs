#include "ba_gate.h"

static int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

void ba_gate_init(ba_gate_t *g, int32_t thr_open, int32_t thr_close, uint16_t hold, unsigned attack_ms, unsigned release_ms, unsigned rate_hz)
{
    g->env = 0; g->gain = 0; g->thr_open = thr_open; g->thr_close = thr_close; g->hold = hold; g->hold_cnt = 0; g->is_open = 0;
    g->atk_shift = 1; g->rel_shift = 8; /* envelope: rises by 1/2 of the gap per sample, falls by 1/256 */
    unsigned a = attack_ms * rate_hz / 1000u, r = release_ms * rate_hz / 1000u;
    g->up_step = (int32_t)(32768u / (a ? a : 1u)); g->down_step = (int32_t)(32768u / (r ? r : 1u));
}

int16_t ba_gate_process(ba_gate_t *g, int16_t x)
{
    int32_t a = x < 0 ? -(int32_t)x : x;
    if (a > g->env) g->env += (a - g->env) >> g->atk_shift; else g->env -= (g->env >> g->rel_shift) + (g->env > 0);
    if (g->env > g->thr_open) { g->is_open = 1; g->hold_cnt = g->hold; }
    else if (g->is_open && g->env < g->thr_close) { if (g->hold_cnt) g->hold_cnt--; else g->is_open = 0; }
    else if (g->is_open) g->hold_cnt = g->hold;
    if (g->is_open) { g->gain += g->up_step; if (g->gain > 32768) g->gain = 32768; }
    else { g->gain -= g->down_step; if (g->gain < 0) g->gain = 0; }
    return sat16(((int32_t)x * g->gain) >> 15);
}

void ba_echo_init(ba_echo_t *e, int16_t *buf, uint16_t delay_samples, int16_t fb_q15, int16_t wet_q15)
{
    e->buf = buf; e->len = delay_samples; e->pos = 0; e->fb_q15 = fb_q15; e->wet_q15 = wet_q15;
    for (uint16_t i = 0; i < delay_samples; i++) buf[i] = 0;
}

int16_t ba_echo_process(ba_echo_t *e, int16_t x)
{
    int32_t d = e->buf[e->pos];
    e->buf[e->pos] = sat16((int32_t)x + ((d * e->fb_q15) >> 15));
    if (++e->pos >= e->len) e->pos = 0;
    return sat16((int32_t)x + ((d * e->wet_q15) >> 15));
}
