/* Integer helpers for the audio algorithms: square roots and log2 in Q16. Plain C99, no floating point, no library. */
#ifndef BA_AMATH_H
#define BA_AMATH_H
#include <stdint.h>

static inline uint32_t ba_isqrt64(uint64_t x)
{
    uint64_t r = 0, b = 1ull << 62;
    while (b > x) b >>= 2;
    while (b) { if (x >= r + b) { x -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return (uint32_t)r;
}

/* log2(x) in Q16 (65536 = one octave) for x >= 1; 0 for x == 0. Error under 0.0002 octave (0.3 cent). */
static inline int32_t ba_log2_q16(uint32_t x)
{
    if (!x) return 0;
    int n = 0; uint32_t t = x;
    while (t >>= 1) n++;
    uint64_t m = (uint64_t)x << (31 - n); /* mantissa in [1,2) as Q31 */
    int32_t frac = 0;
    for (int i = 0; i < 16; i++) {
        m = (m * m) >> 31;
        frac <<= 1;
        if (m >= (1ull << 32)) { m >>= 1; frac |= 1; }
    }
    return (int32_t)(n << 16) + frac;
}

#endif
