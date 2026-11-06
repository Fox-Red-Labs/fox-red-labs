/* Which BoosterPack site is the DRV8376EVM on? (Fox Red Labs, 2026-10-20). Read only: every candidate pin of both
 * sites is an input; each is read with the MCU's pull-up and then its pull-down. A pin the EVM holds (a pull resistor,
 * a Hall output, a sense amplifier, the bus divider) reads the same both ways; an unconnected pin follows the pull.
 * Nothing is driven. Results in RAM (`ps`), read over the probe. */
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m33/dl_core.h>
typedef struct { uint32_t site; const char *sig; uint32_t pcm; GPIO_Regs *port; uint32_t pin; } P;
static const P pins[] = {
#include "pins.inc"
};
#define N (sizeof pins / sizeof pins[0])
volatile struct { uint32_t magic, n; uint8_t up[64], down[64]; } ps;
int main(void)
{
    DL_GPIO_reset(GPIOA); DL_GPIO_reset(GPIOB); DL_GPIO_reset(GPIOC);
    DL_GPIO_enablePower(GPIOA); DL_GPIO_enablePower(GPIOB); DL_GPIO_enablePower(GPIOC);
    DL_Common_delayCycles(320000);
    for (uint32_t i = 0; i < N; i++) {
        DL_GPIO_initDigitalInputFeatures(pins[i].pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        DL_Common_delayCycles(160000);
        ps.up[i] = DL_GPIO_readPins(pins[i].port, pins[i].pin) ? 1 : 0;
        DL_GPIO_initDigitalInputFeatures(pins[i].pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        DL_Common_delayCycles(160000);
        ps.down[i] = DL_GPIO_readPins(pins[i].port, pins[i].pin) ? 1 : 0;
        DL_GPIO_initDigitalInputFeatures(pins[i].pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    }
    ps.n = N; ps.magic = 0x5CA11ED0u;
    while (1) { __WFI(); }
}
