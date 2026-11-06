#include "ba_tone.h"
#include "ba_sine_table.h"

void ba_tone_init(ba_tone_t *t, unsigned freq_hz, unsigned rate_hz)
{
    t->phase = 0;
    t->step = ba_tone_step(freq_hz, rate_hz);
}

int16_t ba_tone_next(ba_tone_t *t)
{
    int16_t s = ba_sine_tab[t->phase >> 8];
    t->phase = (uint16_t)(t->phase + t->step);
    return s;
}
