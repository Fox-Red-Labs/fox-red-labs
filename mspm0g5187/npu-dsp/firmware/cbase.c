/*
 * Plain C reference on the Cortex-M0+ of the LP-MSPM0G5187: the same fixed int8 filters written by hand,
 * no NNC, no NPU. Straight loops (the compiler does the rest at -O3). Same integer arithmetic as
 * host/make_models.py, checked against the same expected outputs. Cases: one FIR channel (fc 0.1),
 * four FIR channels, eight bank channels, and the 64-point DFT as a 64x64 matrix-vector product.
 * Result block `cb` is read from RAM with the debugger.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "../../../mspm0g3507/mcu-bench/sdk-common/bench_timer.h"
#include "vectors.h"

#define NRUN 20
volatile struct {
    uint32_t magic, cpuclk_hz;
    uint32_t mism[4];       /* fir1, fir4, bank8, (dft not checked here) */
    uint32_t cyc_med[4];    /* median of NRUN calls: 1-channel FIR, 4-channel FIR, 8-channel bank */
    uint32_t cyc_min[4];
    uint32_t cyc_max[4];
} cb;

static int8_t outb[8 * DSP_BLOCK];

/* y[c][n] = clip(floor(sum_k taps[c][k] * x[n+63-k] / 2^m[c]))  -- the convolution the ONNX graph computes */
static void fir_bank(const int8_t (*taps)[DSP_TAPS], const uint8_t *m, int nch, const int8_t *x)
{
    for (int c = 0; c < nch; c++) {
        const int8_t *h = taps[c];
        for (int n = 0; n < DSP_BLOCK; n++) {
            int32_t acc = 0;
            const int8_t *xp = &x[n + DSP_TAPS - 1];
            for (int k = 0; k < DSP_TAPS; k++) acc += (int32_t)h[k] * xp[-k];
            acc >>= m[c];
            if (acc > 127) acc = 127;
            if (acc < -128) acc = -128;
            outb[c * DSP_BLOCK + n] = (int8_t)acc;
        }
    }
}

static uint32_t check(const int8_t *exp, int nch, int first)
{
    uint32_t bad = 0;
    for (int c = 0; c < nch; c++)
        for (int n = 0; n < DSP_BLOCK; n++)
            if (outb[c * DSP_BLOCK + n] != exp[(c + first) * DSP_BLOCK + n]) bad++;
    return bad;
}

static void sort(uint32_t *s, int n)
{
    for (int i = 1; i < n; i++) { uint32_t k = s[i]; int j = i - 1; while (j >= 0 && s[j] > k) { s[j + 1] = s[j]; j--; } s[j + 1] = k; }
}

static void timed(int idx, const int8_t (*taps)[DSP_TAPS], const uint8_t *m, int nch, const int8_t *x, const int8_t *exp)
{
    uint32_t s[NRUN];
    fir_bank(taps, m, nch, x);
    cb.mism[idx] = check(exp, nch, 0);
    for (int t = 0; t < NRUN; t++) {
        uint64_t t0 = bench_cycles();
        fir_bank(taps, m, nch, x);
        s[t] = (uint32_t)(bench_cycles() - t0);
    }
    sort(s, NRUN);
    cb.cyc_min[idx] = s[0]; cb.cyc_med[idx] = s[NRUN / 2]; cb.cyc_max[idx] = s[NRUN - 1];
}

int main(void)
{
    SYSCFG_DL_init();
    bench_timer_start();
    cb.cpuclk_hz = CPUCLK_FREQ;
    timed(0, taps_fir64, taps_fir64_m, 1, dsp_input_a, exp_fir64);   /* channel 0 only */
    timed(1, taps_fir64, taps_fir64_m, 4, dsp_input_a, exp_fir64);
    timed(2, taps_bank8, taps_bank8_m, 8, dsp_input_b, exp_bank8);
    cb.magic = 0xD5B000C1u;
    while (1) { __WFI(); }
}
