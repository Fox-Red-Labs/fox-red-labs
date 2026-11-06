/*
 * Breathing LED for MSPM0 LaunchPad kits: the board's red LED slowly brightens and dims, a triangle of 1 percent
 * steps up and down about every two seconds. The dimming is software PWM at 1 kHz: each millisecond the pin is on for
 * duty percent of the time (delay_cycles for the on and off parts). The current duty is left in br.duty, where the
 * browser bench reads it through the debugger, together with the LED's pin, to check that the pin follows the duty.
 * The time for one breath is parameter 0 of the shared block (frl_params.h). Replaces the main source of TI's gpio_toggle_output example for the board; board_leds.h comes from
 * tools/webflash/build_board.py.
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"
#include "frl_params.h"

#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)
/* 1 kHz PWM periods spent at each duty step: 200 steps per breath, 10 per step is 2 s; the block's period_ms sets it (period / 200) */

volatile struct {
    uint32_t magic;           /* 0xB2EA7401 once the loop runs */
    uint32_t duty;            /* current duty, 0 to 100 percent */
    uint32_t breaths;         /* full up-and-down cycles since reset */
} br;

static void led(uint32_t on)
{
    if (on == PL_ACTIVE_HIGH) {
        DL_GPIO_setPins(PL_R_PORT, PL_R_PIN);
    } else {
        DL_GPIO_clearPins(PL_R_PORT, PL_R_PIN);
    }
}

static void hold(uint32_t duty, uint32_t ms_per_step)
{
    uint32_t on = duty * (CYCLES_PER_MS / 100u), off = CYCLES_PER_MS - on;
    br.duty = duty;
    for (uint32_t k = 0; k < ms_per_step; k++) {
        if (on) {
            led(1);
            delay_cycles(on);
        }
        if (off) {
            led(0);
            delay_cycles(off);
        }
    }
}

int main(void)
{
    SYSCFG_DL_init();
    br.magic = 0xB2EA7401u;
    while (1) {
        uint32_t step = frl_get(0) / 200u;     /* period_ms, 400 to 10000: 2 to 50 ms per step */
        for (uint32_t d = 0; d < 100u; d++) hold(d, step);
        for (uint32_t d = 100u; d > 0u; d--) hold(d, step);
        br.breaths++;
    }
}
