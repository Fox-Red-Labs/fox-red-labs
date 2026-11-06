/*
 * APP_BENCH: the alarm model alone, for the NPU-against-CPU comparison. Built against the NNC NPU library
 * (model/nnc_g5187_npu) or the NNC CPU-only library (nnc_g3507_cpu, generic Cortex-M0+) with build.sh's NNC=.
 * Per held-out test window: class against onnxruntime's, first call and best of 5, timed with SysTick
 * (80,000 cycles per ms), one tvmgen_default_run call (input scaling included). Prints one line per window.
 */
#include "tvmgen_default.h"
#include "test_vectors.h"
#define FE_C 16
#define FE_T 128
static float nin[FE_C * FE_T];
static float nout[3];

static uint32_t now_cycles(void)
{
    uint32_t a, v;
    do { a = ms_ticks; v = SysTick->VAL; } while (a != ms_ticks);
    return a * 80000u + (79999u - v);
}

int main(void)
{
    SYSCFG_DL_init();
#ifdef TVMGEN_DEFAULT_TI_NPU
    DL_NPU_reset(NPU);
    DL_NPU_enablePower(NPU);
    while (!(DL_SYSCTL_getStatus() & DL_SYSCTL_STATUS_NPU_READY)) { }
    DL_NPU_clearInterruptStatus(NPU, DL_NPU_INTERRUPT_DONE);
    DL_NPU_enableInterrupt(NPU, DL_NPU_INTERRUPT_DONE);
    NVIC_EnableIRQ(NPU_INT_IRQn);
    tx_str("\r\nBENCH target=npu\r\n");
#else
    tx_str("\r\nBENCH target=cpu\r\n");
#endif
    struct tvmgen_default_inputs ti = { (void *)nin };
    struct tvmgen_default_outputs to = { (void *)nout };
    int ok = 0;
    for (int v = 0; v < TV_N; v++) {
        for (int k = 0; k < FE_C * FE_T; k++) nin[k] = tv_x[v][k] / TV_Q;
        uint32_t best = 0xFFFFFFFFu, first = 0;
        for (int r = 0; r < 6; r++) {
            uint32_t t0 = now_cycles();
#ifdef TVMGEN_DEFAULT_TI_NPU
            tvmgen_default_finished = 0;
            tvmgen_default_run(&ti, &to);
            while (!tvmgen_default_finished) { }
#else
            tvmgen_default_run(&ti, &to);
#endif
            uint32_t dt = now_cycles() - t0;
            if (r == 0) first = dt; else if (dt < best) best = dt;
        }
        int c = 0; for (int k = 1; k < 3; k++) if (nout[k] > nout[c]) c = k;
        ok += c == tv_class[v];
        tx_str("V "); tx_int(v); tx_str(" class="); tx_int(c); tx_str(" onnx="); tx_int(tv_class[v]);
        tx_str(" first_cyc="); tx_int((int32_t)first); tx_str(" best_cyc="); tx_int((int32_t)best); tx_str("\r\n");
    }
    tx_str("BENCH done class_match="); tx_int(ok); tx('/'); tx_int(TV_N); tx_str("\r\n");
    for (;;) { }
}
