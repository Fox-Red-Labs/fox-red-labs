/* Host check that the codec did real work: signal-to-noise ratio of decoded against input, after removing the codec delay. */
#include <stdio.h>
#include <math.h>
#include "lc3.h"
#include "lc3run.h"
extern int16_t keep_in[48000], keep_out[48000];
int main(void)
{
    static const int cfg[3][2] = { {16000, 32}, {24000, 48}, {48000, 96} };
    for (int i = 0; i < 3; i++) {
        static lc3res_t r; static lc3detail_t d; int hz = cfg[i][0]; lc3run(hz, cfg[i][1], &r, &d, 0);
        int dl = lc3_delay_samples(10000, hz), n = hz; double best = -1e9; int bd = 0;
        for (int t = dl - 8; t <= dl + 8; t++) {           /* find the alignment with the best SNR near the nominal delay */
            double s = 0, e = 0; for (int k = hz / 10; k < n - t; k++) { double a = keep_in[k], b = keep_out[k + t]; s += a * a; e += (a - b) * (a - b); }
            double snr = 10 * log10(s / (e + 1e-9)); if (snr > best) { best = snr; bd = t; }
        }
        for (int seg = 0; seg < 2; seg++) { double s2 = 0, e2 = 0; int lo = seg ? hz / 2 : hz / 10, hi = seg ? n - bd : hz / 2;
            for (int k = lo; k < hi; k++) { double a = keep_in[k], b = keep_out[k + bd]; s2 += a * a; e2 += (a - b) * (a - b); }
            printf("   %s part: SNR %.1f dB\n", seg ? "speech-like" : "sweep", 10 * log10(s2 / (e2 + 1e-9))); }
        printf("%u Hz %u kbps: nominal delay %d samples, best alignment %d, SNR %.1f dB\n", r.hz, r.kbps, dl, bd, best);
    }
    return 0;
}
