/* Microphone block statistics (running on the device and on the host test). Samples are unsigned ADC codes. */
#ifndef BA_MIC_H
#define BA_MIC_H
#include <stdint.h>

typedef struct { uint32_t n; int32_t mean_x16; uint32_t rms_x16; uint16_t min, max; } ba_mic_stats_t;

static inline void ba_mic_stats(const uint16_t *s, uint32_t n, ba_mic_stats_t *o)
{
    uint64_t sum = 0, sq = 0;
    uint16_t lo = 0xFFFF, hi = 0;
    for (uint32_t i = 0; i < n; i++) { sum += s[i]; if (s[i] < lo) lo = s[i]; if (s[i] > hi) hi = s[i]; }
    int64_t mean16 = (int64_t)((sum * 16u) / (n ? n : 1u));
    for (uint32_t i = 0; i < n; i++) { int64_t d = (int64_t)s[i] * 16 - mean16; sq += (uint64_t)(d * d); }
    uint64_t var = sq / (n ? n : 1u); /* in 1/256 code^2 */
    uint32_t r = 0; while ((uint64_t)(r + 1) * (r + 1) <= var) r++; /* integer sqrt, in 1/16 code */
    o->n = n; o->mean_x16 = (int32_t)mean16; o->rms_x16 = r; o->min = lo; o->max = hi;
}

#endif
