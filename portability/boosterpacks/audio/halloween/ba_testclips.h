/* Synthetic test sounds for the listener, made the same way on the Mac and on the board so both can run the same
 * clips through the same code: a clap (a 25 ms noise burst), a knock (two damped 180 Hz thumps), a whistle (a
 * gliding 1.5 to 1.8 kHz tone, 600 ms), a shout (a buzzy 220 Hz voice-like tone with noise, 500 ms), quiet room noise,
 * and a ticking clock (soft clicks that must stay quiet). Each clip is 1.2 s of 12-bit microphone codes around 2048,
 * with background noise, scaled by gain_q8 (256 = the level a clap at about 1 m makes, a guess). */
#ifndef BA_TESTCLIPS_H
#define BA_TESTCLIPS_H
#include <stdint.h>
#include "ba_sfx.h"

enum { TC_CLAP, TC_KNOCK, TC_WHISTLE, TC_SHOUT, TC_QUIET, TC_TICK, TC_COUNT };
#define TC_LEN 9600u

static inline uint16_t ba_testclip(uint8_t kind, uint32_t i, uint32_t *rng, uint16_t gain_q8)
{
    int32_t n = ((int32_t)(ba_rand(rng) >> 16) - 32768) / 4096;          /* +/-8 codes of room noise */
    int32_t s = 0;
    uint32_t t0 = 1600u;                                                  /* the sound starts at 200 ms */
    if (i >= t0) {
        uint32_t t = i - t0;
        switch (kind) {
        case TC_CLAP: if (t < 200u) s = ((int32_t)(ba_rand(rng) >> 16) - 32768) * (int32_t)(200u - t) / 200 / 24; break;
        case TC_KNOCK: {
            uint32_t u = t < 1600u ? t : t - 1600u;                      /* two knocks 200 ms apart */
            if (u < 600u && (t < 600u || (t >= 1600u && t < 2200u)))
                s = (int32_t)ba_sin8(u * (uint32_t)(((uint64_t)180u << 32) / 8000u)) * (int32_t)(600u - u) / 600 / 40;
            break; }
        case TC_WHISTLE:
            if (t < 4800u) {
                uint32_t f = 1500u + 300u * t / 4800u;
                static uint32_t ph; if (t == 0) ph = 0;
                ph += (uint32_t)(((uint64_t)f << 32) / 8000u);
                s = ba_sin8(ph) / 60;
            }
            break;
        case TC_SHOUT:
            if (t < 4000u) {
                static uint32_t ph2; if (t == 0) ph2 = 0;
                ph2 += (uint32_t)(((uint64_t)220u << 32) / 8000u);
                int32_t saw = (int32_t)(ph2 >> 16) - 32768;
                s = (saw / 60) + ((int32_t)(ba_rand(rng) >> 16) - 32768) / 300;
            }
            break;
        case TC_TICK: if ((t % 4000u) < 8u) s = 40; break;
        default: break;
        }
    }
    int32_t v = 2048 + n + s * (int32_t)gain_q8 / 256;
    if (v < 0) v = 0; if (v > 4095) v = 4095;
    return (uint16_t)v;
}

#endif
