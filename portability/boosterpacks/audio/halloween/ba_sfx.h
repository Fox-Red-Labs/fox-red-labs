/* Halloween sound effects, synthesized at the sample rate from noise, pulse, triangle and a sine table; integer only.
 * One effect plays at a time (a new one replaces the old); the wind can also run as a quiet bed under the music. */
#ifndef BA_SFX_H
#define BA_SFX_H
#include <stdint.h>

enum { SFX_NONE, SFX_THUNDER, SFX_WIND, SFX_DOOR, SFX_HEART, SFX_CACKLE, SFX_SCREAM, SFX_WAIL, SFX_ZAP, SFX_BLIP, SFX_COUNT };

typedef struct {
    uint8_t type;
    uint32_t t, len;            /* samples since start, length */
    uint32_t rate;
    uint32_t rng;
    int32_t lp1, lp2, lp3;      /* one-pole filter states */
    uint32_t ph, ph2;           /* phase accumulators, 32-bit */
    int32_t flutter;
} ba_sfx_t;

void ba_sfx_init(ba_sfx_t *x, uint32_t rate, uint32_t seed);
void ba_sfx_start(ba_sfx_t *x, uint8_t type);
void ba_sfx_blip(ba_sfx_t *x, uint16_t hz);  /* a 70 ms beep, for button feedback */
int16_t ba_sfx_next(ba_sfx_t *x);     /* 0 when idle; full scale about +/-24000 */
const char *ba_sfx_name(uint8_t type);

/* the wind bed: its own state so it can sound under music and under an effect */
typedef struct { uint32_t rng, t; int32_t lp1, lp2; uint32_t rate; } ba_wind_t;
void ba_wind_init(ba_wind_t *w, uint32_t rate, uint32_t seed);
int16_t ba_wind_next(ba_wind_t *w);   /* about +/-9000 */

uint32_t ba_rand(uint32_t *s);        /* xorshift32 */
int16_t ba_sin8(uint32_t phase32);    /* sine from the top 8 bits of a 32-bit phase */

#endif
