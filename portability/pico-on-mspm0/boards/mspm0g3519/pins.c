/* Pico SDK on MSPM0, LP-MSPM0G3519: Pico GPIO number to MSPM0 pin. Each row: port, pin bit, IOMUX index, and the IOMUX function
 * for UART, I2C and PWM (0 when the pin has none), the PWM slice and channel, and the ADC channel (-1 when none). */
#include "ti_msp_dl_config.h"
#include "pico_mspm0_board.h"
const pico_pin_t pico_pins[30] = {
    [0]  = { GPIOA, DL_GPIO_PIN_10, IOMUX_PINCM21, IOMUX_PINCM21_PF_UART0_TX, 0, 0, -1, -1 },
    [1]  = { GPIOA, DL_GPIO_PIN_11, IOMUX_PINCM22, IOMUX_PINCM22_PF_UART0_RX, 0, 0, -1, -1 },
    [4]  = { GPIOC, DL_GPIO_PIN_3,  IOMUX_PINCM77, 0, IOMUX_PINCM77_PF_I2C2_SDA, 0, -1, -1 },
    [5]  = { GPIOC, DL_GPIO_PIN_2,  IOMUX_PINCM76, 0, IOMUX_PINCM76_PF_I2C2_SCL, 0, -1, -1 },
    [25] = { GPIOB, DL_GPIO_PIN_27, IOMUX_PINCM58, 0, 0, IOMUX_PINCM58_PF_TIMA1_CCP1, 0x01, -1 },   /* slice 0, channel B */
    [26] = { GPIOA, DL_GPIO_PIN_27, IOMUX_PINCM60, 0, 0, 0, -1, 0 },
};
GPTIMER_Regs *const pico_pwm_slices[1] = { TIMA1 };
const int8_t pico_adc_inputs[5] = { 0, -1, -1, -1, -1 };       /* inputs 0 to 3 = GP26 to GP29; 4 (temperature sensor) not mapped */
void pico_board_init(void)
{
    /* Waveshare panel touch controller (FT6336U) reset on PB11: low 5 ms, then high and 400 ms before the first I2C transfer;
     * it holds the bus busy for about 65 ms after reset. */
    DL_GPIO_clearPins(PB_PORT, PB_TP_RST_PIN); delay_cycles(CPUCLK_FREQ / 200);
    DL_GPIO_setPins(PB_PORT, PB_TP_RST_PIN); delay_cycles(CPUCLK_FREQ / 1000 * 400);
}
