/* Sine tone from a 256-entry table and a 16-bit phase accumulator. At the 8 kHz sample rate a step of 8192
 * is exactly 1 kHz (8 samples per cycle, no jitter). */
#ifndef BA_TONE_H
#define BA_TONE_H
#include <stdint.h>

typedef struct { uint16_t phase, step; } ba_tone_t;

static inline uint16_t ba_tone_step(unsigned freq_hz, unsigned rate_hz) { return (uint16_t)(((uint32_t)freq_hz << 16) / rate_hz); }
void ba_tone_init(ba_tone_t *t, unsigned freq_hz, unsigned rate_hz);
int16_t ba_tone_next(ba_tone_t *t);

#endif
