/*
 * Count the people in a room: one of the lab's four 1D CNN occupancy models (UCI Room Occupancy Estimation data,
 * models in mspm0g3507/mcu-bench/edgeai/uci-room-occupancy/) on an MSPM0 LaunchPad, asked by the browser
 * bench through the debugger. The model is chosen at build time (M, for example occ_cnn_2k). The chip holds the
 * model's 20 held-out test windows (4 sensor channels of 32 samples: light, sound, CO2, motion). At start it runs
 * all 20 and leaves in oc how many match the reference answers (class and all four logits), how many match the
 * true head count, and the fastest and slowest run in CPU cycles. Then the page writes a window number to oc.idx
 * and bumps oc.seq_in; the chip runs that window and sets pred, logits, cycles and seq_out. LED2 red flashes once
 * per answer (parameter 0 of the shared block, frl_params.h). Replaces the main source of TI's gpio_toggle_output
 * example for the board; built by tools/webflash/build_occ.py. The timing covers the model's whole run call,
 * float input quantisation included, like the occupancy benchmark (mspm0l1306/bench/edgeai/main.c).
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"
#include "frl_params.h"
#include <stdint.h>
#define STR2(x) #x
#define STR(x) STR2(x)
#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
#include STR(M.h)
#include "test_vectors.h"

#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)
#define N_TEST 20u

volatile struct {
    uint32_t magic;       /* 0x0CC00002 once the 20 test windows are done and the chip waits for a window number */
    uint32_t cpuclk_hz;
    uint32_t n_test;
    uint32_t same_ref;    /* windows whose class and four logits match the reference answers */
    uint32_t right;       /* windows whose class is the true head count */
    uint32_t cyc_min;     /* fastest and slowest of the 20 runs, CPU cycles */
    uint32_t cyc_max;
    uint32_t seq_in;      /* the page writes idx, then sets seq_in to a new value */
    uint32_t seq_out;     /* equals seq_in once pred, logits and cycles belong to that window */
    uint32_t idx;
    uint32_t pred;
    uint32_t cycles;
    int32_t logits[4];
} oc;

static uint32_t timed(uint32_t v, int8_t *lg, uint32_t *cycles)
{
    SysTick->LOAD = 0xFFFFFFu;            /* 24 bits: 16.7 M cycles, far above the slowest model's run */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    uint32_t a = SysTick->VAL;
    int c = CAT(M, _run)(CAT(M, _test_x)[v], lg);
    uint32_t b = SysTick->VAL;
    *cycles = (a - b) & 0xFFFFFFu;
    return (uint32_t) c;
}

static void led(uint32_t on)
{
    if (on == PL_ACTIVE_HIGH) {
        DL_GPIO_setPins(PL_R_PORT, PL_R_PIN);
    } else {
        DL_GPIO_clearPins(PL_R_PORT, PL_R_PIN);
    }
}

int main(void)
{
    SYSCFG_DL_init();
    led(0);
    oc.cpuclk_hz = CPUCLK_FREQ;
    oc.n_test = N_TEST;
    uint32_t same = 0, right = 0, lo = 0xFFFFFFFFu, hi = 0, c;
    int8_t lg[4];
    for (uint32_t v = 0; v < N_TEST; v++) {
        uint32_t k = timed(v, lg, &c);
        uint32_t ok = (k == CAT(M, _test_expected_class)[v]);
        for (int i = 0; i < 4; i++) ok &= (lg[i] == CAT(M, _test_expected_logits)[v][i]);
        same += ok;
        right += (k == CAT(M, _test_label)[v]);
        if (c < lo) lo = c;
        if (c > hi) hi = c;
    }
    oc.same_ref = same; oc.right = right; oc.cyc_min = lo; oc.cyc_max = hi;
    oc.seq_out = oc.seq_in;
    oc.magic = 0x0CC00002u;
    while (1) {
        if (oc.seq_in != oc.seq_out) {
            uint32_t seq = oc.seq_in, v = oc.idx;
            if (v >= N_TEST) v = N_TEST - 1u;
            oc.pred = timed(v, lg, &c);
            for (int i = 0; i < 4; i++) oc.logits[i] = lg[i];
            oc.cycles = c;
            oc.seq_out = seq;
            if (frl_get(0)) led(1);
            delay_cycles(frl_get(0) * CYCLES_PER_MS);
            led(0);
        }
    }
}
