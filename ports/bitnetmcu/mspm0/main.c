/*
 * BitNetMCU on MSPM0: classify the 10 test digits, time each one with the SysTick cycle counter,
 * and leave the answers and cycle counts in RAM for the debugger (struct bnm, magic 0xB17E0001).
 * GPL-3.0, like BitNetMCU (third-party/bitnetmcu/LICENSE; copy in ../LICENSE).
 *
 * -DRAMFUNC runs the three inner-loop functions from SRAM, as the upstream CH32V002 numbers do.
 */
#include "ti_msp_dl_config.h"
#include "bench_timer.h"

#ifdef RAMFUNC
#define RAM __attribute__((section(".TI.ramfunc"), noinline))  /* noinline: or -O3 folds them into flash code */
#include <stdint.h>
void processfclayer(int8_t *, const uint32_t *, int32_t, uint32_t, uint32_t, int32_t *) RAM;
int32_t *processmaxpool22(int32_t *activations, uint32_t xy_input, int32_t *output) RAM;
int32_t *processconv33ReLU(int32_t *activations, const int8_t *weights, uint32_t xy_input, uint32_t n_shift, int32_t *output) RAM;
#endif

#include "bitnet_run.c"

volatile struct {
    uint32_t magic;            /* 0xB17E0001 when done */
    uint32_t cpuclk_hz;
    uint32_t timer_overhead;   /* cycles for two back-to-back timer reads, subtracted below */
    uint32_t right;            /* answers that match the label */
    uint32_t pred[N_TEST];
    uint32_t cycles[N_TEST];
} bnm;

int main(void)
{
    SYSCFG_DL_init();
    bench_timer_start();
    bnm.cpuclk_hz = CPUCLK_FREQ;
    { uint64_t a = bench_cycles(); uint64_t b = bench_cycles(); bnm.timer_overhead = (uint32_t) (b - a); }
    for (int i = 0; i < N_TEST; i++) {
        uint64_t t0 = bench_cycles();
        uint32_t p = BitMnistInference(test_in[i]);
        uint64_t t1 = bench_cycles();
        bnm.pred[i] = p;
        bnm.cycles[i] = (uint32_t) (t1 - t0) - bnm.timer_overhead;
        bnm.right += (p == *test_label[i]);
    }
    bnm.magic = 0xB17E0001u;
    while (1) {
        __WFI();
    }
}
