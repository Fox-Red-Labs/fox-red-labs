/* Spectrum analyzer: 128-point radix-2 fixed-point FFT (Q15, scaled by 1/2 per stage so it cannot overflow),
 * Hann window, 64 magnitude bins, 16 log-spaced bars of 0 to 127 pixels for a 128x128 screen.
 * At 8 kHz the bin width is 62.5 Hz and the top bin is 4 kHz. Work: 448 butterflies (about 1,800 multiplies). */
#ifndef BA_FFT_H
#define BA_FFT_H
#include <stdint.h>

#define BA_FFT_N 128
#define BA_BARS 16

/* In place on re[]/im[]. Output is the true DFT divided by BA_FFT_N. Input in natural order. */
void ba_fft128(int16_t *re, int16_t *im);
/* x: 128 samples, signed 16-bit (DC removed). mag: 64 bins in Q4 (16 = 1.0), one-sided amplitude with the Hann window loss included (a sine of amplitude A reads A/2). The input is shifted up to near 14 bits before the FFT (block floating point) so quiet signals keep their precision. */
void ba_spectrum_mag(const int16_t *x, uint32_t *mag /* BA_FFT_N/2, Q4 */);
/* Bars: peak magnitude in each band mapped to 8 pixels per octave (6 dB), 0 to 127. */
void ba_spectrum_bars(const uint32_t *mag, uint8_t *bar /* BA_BARS */);
/* The band edges in bins: bar i covers bins ba_bar_edge[i] to ba_bar_edge[i+1]-1. */
extern const uint8_t ba_bar_edge[BA_BARS + 1];

#endif
