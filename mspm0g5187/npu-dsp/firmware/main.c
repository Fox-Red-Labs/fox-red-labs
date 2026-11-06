/*
 * Fixed-weight DSP graphs on the LP-MSPM0G5187, bare metal. One image per model and target:
 *   NNC library on the TinyEngine NPU (-DUSE_NPU) or on the Cortex-M0+ (NNC "type=soft" build).
 * Model, input and expected output come from -DMODEL_<NAME> and vectors.h (host/make_models.py).
 * The libraries are compiled with skip_normalize=true and output_int=true, so the input is the int8
 * test signal and the output is int8 (uint8 for the trip layer), checked against the integer reference.
 * Result block `dsp` is read from RAM with the debugger. Clock: SYSPLL 80 MHz (SysConfig of TI's
 * motor_bearing_fault_detection_ai_npu example, NPU library on), same as the occupancy runs.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "../../../mspm0g3507/mcu-bench/sdk-common/bench_timer.h"
#include "tvmgen_default.h"
#include "vectors.h"

#if defined(MODEL_FIR64)
#define INPUT dsp_input_a
#define EXPECT exp_fir64
#define NOUT EXP_FIR64_N
#define OUT_T int8_t
#elif defined(MODEL_FIR64_DEC4)
#define INPUT dsp_input_a
#define EXPECT exp_fir64_dec4
#define NOUT EXP_FIR64_DEC4_N
#define OUT_T int8_t
#elif defined(MODEL_BANK8)
#define INPUT dsp_input_b
#define EXPECT exp_bank8
#define NOUT EXP_BANK8_N
#define OUT_T int8_t
#elif defined(MODEL_BANK8_TRIP)
#define INPUT dsp_input_b
#define EXPECT exp_bank8_trip
#define NOUT EXP_BANK8_TRIP_N
#define OUT_T uint8_t
#elif defined(MODEL_BANK8_TRIP2)
#define INPUT dsp_input_b
#define EXPECT exp_bank8_trip2
#define NOUT EXP_BANK8_TRIP2_N
#define OUT_T uint8_t
#elif defined(MODEL_BANK8_TRIP3)
#define INPUT dsp_input_b
#define EXPECT exp_bank8_trip3
#define NOUT EXP_BANK8_TRIP3_N
#define OUT_T uint8_t
#elif defined(MODEL_BANK8_TRIPS)
#define INPUT dsp_input_b
#define EXPECT exp_bank8_trips
#define NOUT EXP_BANK8_TRIPS_N
#define OUT_T int8_t
#elif defined(MODEL_DFT64)
#define INPUT dsp_input_c
#define EXPECT exp_dft64
#define NOUT EXP_DFT64_N
#define OUT_T int8_t
#else
#error "define MODEL_<NAME>"
#endif

#define NRUN 20

volatile struct {
    uint32_t magic;        /* 0xD5B00001 when done */
    uint32_t cpuclk_hz;
    uint32_t timer_overhead;
    uint32_t nout;
    uint32_t mismatches;   /* outputs that differ from the integer reference */
    uint32_t maxdiff;      /* largest absolute difference */
    uint32_t cyc_min, cyc_med, cyc_max;
    uint32_t cyc[NRUN];
    uint32_t spin_per_call; /* NPU builds: wait-loop passes in the last timed call (CPU cycles free while the NPU works, rough) */
} dsp;

static int8_t  in[DSP_NIN];
static OUT_T   out[NOUT];

static uint32_t spin;
static void run_once(struct tvmgen_default_inputs *ti, struct tvmgen_default_outputs *to)
{
#ifdef USE_NPU
    tvmgen_default_run(ti, to);
    spin = 0;
    while (!tvmgen_default_finished) { spin++; }
#else
    tvmgen_default_run(ti, to);
#endif
}

int main(void)
{
    SYSCFG_DL_init();
#ifdef USE_NPU
    DL_NPU_reset(NPU);
    DL_NPU_enablePower(NPU);
    while (!(DL_SYSCTL_getStatus() & DL_SYSCTL_STATUS_NPU_READY)) { }
    DL_NPU_clearInterruptStatus(NPU, DL_NPU_INTERRUPT_DONE);
    DL_NPU_enableInterrupt(NPU, DL_NPU_INTERRUPT_DONE);
    NVIC_EnableIRQ(NPU_INT_IRQn);
#endif
    bench_timer_start();
    dsp.cpuclk_hz = CPUCLK_FREQ;
    { uint64_t a = bench_cycles(); uint64_t b = bench_cycles(); dsp.timer_overhead = (uint32_t)(b - a); }
    for (int i = 0; i < DSP_NIN; i++) in[i] = INPUT[i];
    struct tvmgen_default_inputs ti = { (void *)in };
    struct tvmgen_default_outputs to = { (void *)out };

    run_once(&ti, &to);                       /* first call, checked against the reference */
    uint32_t bad = 0, md = 0;
    for (int i = 0; i < NOUT; i++) {
        int d = (int)out[i] - (int)EXPECT[i]; if (d < 0) d = -d;
        if (d) bad++;
        if ((uint32_t)d > md) md = d;
    }
    dsp.nout = NOUT; dsp.mismatches = bad; dsp.maxdiff = md;

    for (int t = 0; t < NRUN; t++) {
        uint64_t t0 = bench_cycles();
        run_once(&ti, &to);
        dsp.cyc[t] = (uint32_t)(bench_cycles() - t0);
    }
    dsp.spin_per_call = spin;
    uint32_t s[NRUN];
    for (int i = 0; i < NRUN; i++) s[i] = dsp.cyc[i];
    for (int i = 1; i < NRUN; i++) { uint32_t k = s[i]; int j = i - 1; while (j >= 0 && s[j] > k) { s[j + 1] = s[j]; j--; } s[j + 1] = k; }
    dsp.cyc_min = s[0]; dsp.cyc_med = s[NRUN / 2]; dsp.cyc_max = s[NRUN - 1];
    dsp.magic = 0xD5B00001u;
    while (1) { __WFI(); }
}
