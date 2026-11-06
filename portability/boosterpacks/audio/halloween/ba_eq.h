/* Speaker EQ for the BOOSTXL-AUDIO's small speaker, fitted from a measured response (tools/fit_eq.py, coefficients in
 * ba_eq_coeffs.h, designed for 16 kHz): a 300 Hz high-pass that keeps the amplifier's headroom for what the speaker
 * can play, four peaking filters that flatten 500 Hz to 4 kHz, and a bass enhancer that turns the band below 300 Hz
 * into its harmonics (300 to 1,200 Hz), which the ear hears as the missing bass. Integer only, Q14 biquads. */
#ifndef BA_EQ_H
#define BA_EQ_H
#include <stdint.h>

#define BA_EQ_MAX 8
typedef struct { int32_t x1, x2, y1, y2; } ba_bq_t;
typedef struct { ba_bq_t eq[BA_EQ_MAX], lp[2], hb[2]; int32_t pre_q8, bass_q8; } ba_eq_t;

void ba_eq_init(ba_eq_t *e);
int32_t ba_eq_next(ba_eq_t *e, int32_t x);   /* one sample in, one out (may exceed 16 bits: the limiter follows) */

#endif
