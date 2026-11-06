/* Mac check that the codec worked: signal-to-noise ratio of the decoded second against the input, at the delay that fits best.
 * Opus delays the signal (encoder look-ahead, decoder filters), so the best lag in 0 to 1200 samples is searched. Build with -DOP_KEEP (tests/quality.sh). */
#include <stdio.h>
#include <math.h>
#include "opusrun.h"
#include "opuscfg.h"
extern int16_t op_keep_in[48000], op_keep_out[48000];
int main(void)
{
    for (int i = 0; i < OP_N; i++) {
        const int *c = op_cfg[i / OP_NCPLX]; static oprun_t r; oprun(c[0], c[1], c[2], c[3], op_cplx[i % OP_NCPLX], &r, 0, 0, 0);
        int n = c[0], best = 0; double bsnr = -99;
        for (int lag = 0; lag <= 1200; lag++) {
            double s = 0, e = 0;
            for (int k = n / 4; k < n - 1200; k++) { double a = op_keep_in[k], b = op_keep_out[k + lag]; s += a * a; e += (a - b) * (a - b); }
            double snr = 10 * log10(s / (e + 1e-9)); if (snr > bsnr) { bsnr = snr; best = lag; }
        }
        printf("cfg %d %5d Hz %4.1f ms %3d kbps c%-2d: best lag %4d samples, SNR %5.1f dB\n", i, c[0], c[1] / 1000.0, c[2], op_cplx[i % OP_NCPLX], best, bsnr);
    }
    return 0;
}
