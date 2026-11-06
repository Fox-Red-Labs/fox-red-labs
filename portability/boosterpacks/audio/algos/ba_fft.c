#include "ba_fft.h"
#include "ba_amath.h"
#include "ba_algo_tables.h"

const uint8_t ba_bar_edge[BA_BARS + 1] = { 1, 2, 3, 4, 5, 6, 8, 10, 13, 16, 20, 25, 31, 38, 47, 56, 64 };

void ba_fft128(int16_t *re, int16_t *im)
{
    /* bit reversal */
    for (unsigned i = 0, j = 0; i < BA_FFT_N; i++) {
        if (i < j) { int16_t t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
        unsigned m = BA_FFT_N >> 1;
        while (m >= 1 && (j & m)) { j ^= m; m >>= 1; }
        j |= m;
    }
    for (unsigned len = 2; len <= BA_FFT_N; len <<= 1) {
        unsigned half = len >> 1, step = BA_FFT_N / len;
        for (unsigned i = 0; i < BA_FFT_N; i += len) {
            for (unsigned k = 0; k < half; k++) {
                int32_t wr = ba_fft_cos[k * step], wi = ba_fft_nsin[k * step];
                int32_t xr = re[i + k + half], xi = im[i + k + half];
                int32_t tr = (xr * wr - xi * wi + 16384) >> 15, ti = (xr * wi + xi * wr + 16384) >> 15;
                int32_t ur = re[i + k], ui = im[i + k];
                re[i + k] = (int16_t)((ur + tr) >> 1); im[i + k] = (int16_t)((ui + ti) >> 1);
                re[i + k + half] = (int16_t)((ur - tr) >> 1); im[i + k + half] = (int16_t)((ui - ti) >> 1);
            }
        }
    }
}

void ba_spectrum_mag(const int16_t *x, uint32_t *mag)
{
    int16_t re[BA_FFT_N], im[BA_FFT_N];
    /* block floating point: shift the input up so its peak is near 14 bits, which keeps the per-stage rounding noise
       far below the signal, then take the shift back out of the magnitudes */
    int32_t pk = 1;
    for (int i = 0; i < BA_FFT_N; i++) { int32_t a = x[i] < 0 ? -(int32_t)x[i] : x[i]; if (a > pk) pk = a; }
    int sh = 0;
    while (sh < 14 && (pk << (sh + 1)) < 16384) sh++;
    for (int i = 0; i < BA_FFT_N; i++) { int32_t v = (int32_t)x[i] << sh; re[i] = (int16_t)((v * ba_hann128[i] + 16384) >> 15); im[i] = 0; }
    ba_fft128(re, im);
    for (int k = 0; k < BA_FFT_N / 2; k++) {
        uint64_t p = (uint64_t)((int32_t)re[k] * re[k]) + (uint64_t)((int32_t)im[k] * im[k]);
        mag[k] = (ba_isqrt64(p << 8) * 2u) >> sh; /* Q4, one-sided */
    }
}

void ba_spectrum_bars(const uint32_t *mag, uint8_t *bar)
{
    for (int b = 0; b < BA_BARS; b++) {
        uint32_t pk = 0;
        for (int k = ba_bar_edge[b]; k < ba_bar_edge[b + 1]; k++) if (mag[k] > pk) pk = mag[k];
        int32_t h = ((ba_log2_q16(pk) * 8) >> 16) - 32; /* 8 px per octave; magnitude 1 is 0 px, 32768 is 120 px (the Q4 scale is taken out) */
        bar[b] = (uint8_t)(h < 0 ? 0 : h > 127 ? 127 : h);
    }
}
