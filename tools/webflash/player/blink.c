/*
 * Blink for MSPM0 LaunchPad kits (every board but the LP-MSPM0L1306): the board's first LED shows the pattern
 * below, on for the first number of milliseconds, off for the second, and so on, forever. Replaces the main
 * source of TI's gpio_toggle_output example for the board; board_leds.h comes from tools/webflash/build_board.py.
 * PATTERN_MS is filled in by the build script. When the build gives the image a parameter block (frl_params.h,
 * patterns up to 8 values), the values in the board's last flash sector replace the pattern, so the browser bench
 * changes the pattern without a compiler.
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"
#include "frl_params.h"

#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

static const uint32_t pattern_ms[] = {PATTERN_MS};

int main(void)
{
    SYSCFG_DL_init();
    while (1) {
#if FRL_N_PARAMS > 0u
        uint32_t n = FRL_N_PARAMS;
#else
        uint32_t n = sizeof(pattern_ms) / sizeof(pattern_ms[0]);
#endif
        for (uint32_t i = 0; i < n; i++) {
            uint32_t on = (i % 2u == 0u);
#if FRL_N_PARAMS > 0u
            uint32_t ms = frl_get(i);
#else
            uint32_t ms = pattern_ms[i];
#endif
            if (on == PL_ACTIVE_HIGH) {
                DL_GPIO_setPins(PL_R_PORT, PL_R_PIN);
            } else {
                DL_GPIO_clearPins(PL_R_PORT, PL_R_PIN);
            }
            delay_cycles(ms * CYCLES_PER_MS);
        }
    }
}
