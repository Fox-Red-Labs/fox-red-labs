/* Pico SDK on MSPM0: hardware/gpio.h through the board's pin table. */
#include "ti_msp_dl_config.h"
#include "pico_mspm0_board.h"
#include "hardware/gpio.h"
void gpio_init(uint gpio)                                 /* as on the Pico: input, output latch low, function SIO */
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p) { return; }
    DL_GPIO_disableOutput(p->port, p->bit); DL_GPIO_clearPins(p->port, p->bit); DL_GPIO_initDigitalInput(p->iomux);
}
void gpio_set_dir(uint gpio, bool out)
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p) { return; }
    if (out) { DL_GPIO_enableOutput(p->port, p->bit); } else { DL_GPIO_disableOutput(p->port, p->bit); }
}
void gpio_put(uint gpio, bool value)
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p) { return; }
    if (value) { DL_GPIO_setPins(p->port, p->bit); } else { DL_GPIO_clearPins(p->port, p->bit); }
}
void gpio_set_function(uint gpio, gpio_function_t fn)
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p) { return; }
    switch (fn) {
        case GPIO_FUNC_I2C:  if (p->pf_i2c) { DL_GPIO_initPeripheralInputFunctionFeatures(p->iomux, p->pf_i2c, DL_GPIO_INVERSION_DISABLE,
                                 DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE); DL_GPIO_enableHiZ(p->iomux); } break;
        case GPIO_FUNC_UART: if (p->pf_uart) { DL_GPIO_initPeripheralOutputFunction(p->iomux, p->pf_uart); IOMUX->SECCFG.PINCM[p->iomux] |= IOMUX_PINCM_INENA_ENABLE; } break;
        case GPIO_FUNC_PWM:  if (p->pf_pwm) { DL_GPIO_initPeripheralOutputFunction(p->iomux, p->pf_pwm); } break;
        case GPIO_FUNC_SIO:  DL_GPIO_initDigitalInput(p->iomux); break;
        default: break;                                    /* SPI, PIO, USB, clock out: not in this layer */
    }
}
void gpio_pull_up(uint gpio)
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p) { return; }
    IOMUX->SECCFG.PINCM[p->iomux] = (IOMUX->SECCFG.PINCM[p->iomux] & ~IOMUX_PINCM_PIPD_ENABLE) | IOMUX_PINCM_PIPU_ENABLE;
}
