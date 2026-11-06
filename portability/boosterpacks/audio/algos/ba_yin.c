#include "ba_yin.h"
#include "ba_amath.h"

void ba_yin(const int16_t *x, uint32_t rate_hz, uint16_t thr_q15, ba_yin_result_t *r)
{
    static uint32_t d[BA_YIN_TAUMAX];   /* difference function (shifted to fit 32 bits) */
    static uint32_t dn[BA_YIN_TAUMAX];  /* cumulative-mean-normalised, Q15 */
    uint64_t cum = 0;
    /* 1. difference function d(tau) = sum_j (x[j] - x[j+tau])^2, tau = 1 .. TAUMAX-1. 64-bit sums, then one common right shift. */
    static uint64_t d64[BA_YIN_TAUMAX];
    uint64_t total = 0;
    for (int tau = 1; tau < BA_YIN_TAUMAX; tau++) {
        uint64_t s = 0;
        for (int j = 0; j < BA_YIN_W; j++) { int32_t e = (int32_t)x[j] - (int32_t)x[j + tau]; s += (uint32_t)(e * e); }
        d64[tau] = s; total += s;
    }
    int sh = 0;
    while ((total >> sh) >= (1ull << 31)) sh++;
    for (int tau = 1; tau < BA_YIN_TAUMAX; tau++) d[tau] = (uint32_t)(d64[tau] >> sh);
    /* 2. cumulative mean normalised difference d'(tau) = d(tau) * tau / sum_{k<=tau} d(k) */
    dn[0] = 32768u;
    for (int tau = 1; tau < BA_YIN_TAUMAX; tau++) {
        cum += d[tau];
        dn[tau] = cum ? (uint32_t)((((uint64_t)d[tau] * (uint32_t)tau) << 15) / cum) : 32768u;
    }
    /* 3. absolute threshold: first dip under thr, followed down to its bottom. tau below rate/1200 is not searched. */
    int tmin = (int)(rate_hz / 1200u); if (tmin < 2) tmin = 2;
    int best = 0, voiced = 0;
    for (int tau = tmin; tau < BA_YIN_TAUMAX - 1; tau++) {
        if (dn[tau] < thr_q15) {
            while (tau + 1 < BA_YIN_TAUMAX - 1 && dn[tau + 1] < dn[tau]) tau++;
            best = tau; voiced = 1; break;
        }
    }
    if (!voiced) {
        uint32_t m = 0xFFFFFFFFu;
        for (int tau = tmin; tau < BA_YIN_TAUMAX - 1; tau++) if (dn[tau] < m) { m = dn[tau]; best = tau; }
    }
    /* 4. parabolic interpolation on the normalised difference around the minimum */
    int64_t a = d[best - 1], b = d[best], c = d[best + 1];
    int64_t den = 2 * (a - 2 * b + c), delta = 0;
    if (den > 0) { delta = ((a - c) * 256) / den; if (delta > 128) delta = 128; if (delta < -128) delta = -128; }
    r->tau_q8 = (uint32_t)(best * 256 + delta);
    r->cmnd_q15 = (uint16_t)(dn[best] > 65535 ? 65535 : dn[best]);
    r->voiced = (uint8_t)voiced;
    r->freq_x256 = voiced ? (uint32_t)(((uint64_t)rate_hz << 16) / r->tau_q8) : 0;
}

int ba_pitch_note(uint32_t freq_x256, int *midi, int *cents_x10)
{
    if (!freq_x256) return 0;
    /* cents from A4 = 1200 * (log2(f256) - log2(440 * 256)), in tenths: 12000 * dlog2 */
    int64_t dl = (int64_t)ba_log2_q16(freq_x256) - (int64_t)ba_log2_q16(112640u); /* Q16 octaves; 440*256 = 112640 */
    int64_t c10 = (dl * 12000) / 65536;
    int n = (int)((c10 + (c10 >= 0 ? 500 : -500)) / 1000);
    *midi = 69 + n; *cents_x10 = (int)(c10 - (int64_t)n * 1000);
    return 1;
}
