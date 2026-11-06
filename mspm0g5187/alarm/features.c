/*
 * Board side of host/features.py (the specification): one feature frame from 1,024 samples.
 * Block scaling: the frame is shifted by g bits so its largest sample fits 15 bits, Hann-windowed,
 * and run through CMSIS-DSP arm_cfft_q15 (512 complex points with zero imaginary part, output scaled
 * down by 2^9; the complex FFT links only its own small tables, the real FFT's init links all sizes). Then
 *   python power = C power / 2^(16 + 2g),  so  dB = 3.0103 * (log2(C power) - 16 - 2g).
 */
#include <stdint.h>
#include <math.h>
#include "arm_math.h"
#include "arm_const_structs.h"
#include "features.h"
#include "fe_tables.h"

static q15_t buf[2 * FE_NFFT];
static uint32_t pacc[FE_NFFT / 2 + 1];

void fe_init(void) { }

static float db_of(uint64_t v, int g)
{
    if (!v) return -130.0f;
    return 3.0103f * (log2f((float)v) - 16.0f - 2.0f * (float)g);
}

void fe_frame(const int32_t *ring, uint32_t len, uint32_t start, float o[FE_NCH])
{
    int32_t mx = 0;
    for (uint32_t i = 0; i < FE_FRAME; i++) {
        int32_t a = ring[(start + i) % len];
        if (a < 0) a = -a;
        if (a > mx) mx = a;
    }
    int g = 0;
    if (mx) {
        int b = 31 - __builtin_clz((uint32_t)mx);      /* mx < 2^(b+1) */
        g = 14 - b;                                    /* mx * 2^g < 2^15 */
    }
    for (int h = 0; h < 2; h++) {
        for (int n = 0; n < FE_NFFT; n++) {
            int32_t x = ring[(start + (uint32_t)(h * FE_NFFT + n)) % len];
            int32_t xs = g >= 0 ? (x << g) : (x >> (-g));
            buf[2 * n] = (q15_t)((xs * (int32_t)fe_win_q15[n]) >> 15);
            buf[2 * n + 1] = 0;
        }
        arm_cfft_q15(&arm_cfft_sR_q15_len512, buf, 0, 1);
        for (int k = 0; k <= FE_NFFT / 2; k++) {
            int32_t re = buf[2 * k], im = buf[2 * k + 1];
            uint32_t p = (uint32_t)(re * re) + (uint32_t)(im * im);
            pacc[k] = h ? pacc[k] + (p >> 1) : (p >> 1);
        }
    }
    for (int b = 0; b < 9; b++) {
        uint64_t s = 0;
        for (int k = fe_band_bins[b]; k < fe_band_bins[b + 1]; k++) s += pacc[k];
        o[b] = db_of(s, g);
    }
    uint64_t tot = 0;
    for (int k = fe_band_bins[0]; k < fe_band_bins[9]; k++) tot += pacc[k];
    o[9] = db_of(tot, g);
    uint64_t sum = 0;
    uint32_t pk = 0;
    int kp = fe_peak_bins[0];
    for (int k = fe_peak_bins[0]; k < fe_peak_bins[1]; k++) {
        sum += pacc[k];
        if (pacc[k] > pk) { pk = pacc[k]; kp = k; }
    }
    o[10] = (float)kp * FE_DF / 4000.0f;
    if (pk && sum) {
        float c = 3.0103f * (log2f((float)pk) - log2f((float)sum) + log2f((float)(fe_peak_bins[1] - fe_peak_bins[0]))) / 10.0f;
        o[11] = c < 0 ? 0 : (c > 4 ? 4 : c);
    } else {
        o[11] = 0;
    }
    o[12] = db_of(pk, g);
}
