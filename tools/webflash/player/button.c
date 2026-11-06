/*
 * Button to LED for MSPM0 LaunchPad kits: the board's user button S2 lights the board's red LED while it is held.
 * Polls the button pin (internal pull-up, so held reads low) and drives the LED from it. Replaces the main source of
 * TI's gpio_toggle_output example for the board; board_leds.h (LED pin, polarity and the S2 pin: PL_SW_*) comes from
 * tools/webflash/build_board.py. The browser bench reads the button's input register and the LED's output register
 * through the debugger, so it can show the LED following your finger.
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"

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
    DL_GPIO_initDigitalInputFeatures(PL_SW_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    while (1) {
        led(DL_GPIO_readPins(PL_SW_PORT, PL_SW_PIN) == 0u);   /* S2 pulls the pin to ground when held */
    }
}
