/*
 * Draw a digit: BitNetMCU's cnn_64 model on an MSPM0 LaunchPad, fed by the browser bench through the debugger.
 * The page writes a 16 x 16 drawing (BitNetMCU's input format) into dg.input and then bumps dg.seq_in; the chip
 * classifies it, times it with SysTick (CPU clock cycles) and sets dg.pred, dg.cycles and dg.seq_out. At start it
 * reads the ten test digits that ship with BitNetMCU and leaves how many it got right in dg.selftest. LED2 red
 * flashes once per answer. Replaces the main source of TI's gpio_toggle_output example for the board; built by
 * tools/webflash/build_digit.py. GPL-3.0, like BitNetMCU (third-party/bitnetmcu/LICENSE).
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"
#include "bitnet_run.c"
#include "frl_params.h"

#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

volatile struct {
    uint32_t magic;       /* 0xD1617001 once the self-test is done and the chip waits for drawings */
    uint32_t cpuclk_hz;
    uint32_t selftest;    /* test digits read right at start, of N_TEST */
    uint32_t n_test;
    uint32_t seq_in;      /* the page writes input[], then sets seq_in to a new value */
    uint32_t seq_out;     /* equals seq_in once pred and cycles belong to that input */
    uint32_t pred;
    uint32_t cycles;
    int8_t input[256];
} dg;

static uint32_t timed(const int8_t *in, uint32_t *cycles)
{
    SysTick->LOAD = 0xFFFFFFu;            /* 24 bits: 16.7 M cycles, about 6 times the model's 2.6 M */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    uint32_t a = SysTick->VAL;
    uint32_t p = BitMnistInference(in);
    uint32_t b = SysTick->VAL;
    *cycles = (a - b) & 0xFFFFFFu;
    return p;
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
    dg.cpuclk_hz = CPUCLK_FREQ;
    dg.n_test = N_TEST;
    uint32_t right = 0, c;
    for (int t = 0; t < N_TEST; t++) {
        right += (timed(test_in[t], &c) == *test_label[t]);
    }
    dg.selftest = right;
    dg.seq_out = dg.seq_in;
    dg.magic = 0xD1617001u;
    while (1) {
        if (dg.seq_in != dg.seq_out) {
            uint32_t seq = dg.seq_in;
            dg.pred = timed((const int8_t *) dg.input, &c);
            dg.cycles = c;
            dg.seq_out = seq;
            if (frl_get(0)) led(1);
            delay_cycles(frl_get(0) * CYCLES_PER_MS);
            led(0);
        }
    }
}
