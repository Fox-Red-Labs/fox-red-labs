/* Wire continuity probe for the display wiring. Each pin becomes a plain input, read once
 * with the chip's weak pull-up and once with its weak pull-down. A loose pin follows the
 * pull (up=1, down=0). A pin wired to the display reads what the module holds it at.
 * res.pin[i] = bit0: level with pull-up, bit1: level with pull-down. */
#include "ti_msp_dl_config.h"

static const struct { uint32_t pincm; GPIO_Regs *port; uint32_t pin; } P[] = {
    {GPIO_LCD_SPI_IOMUX_SCLK, GPIOB, DL_GPIO_PIN_9},   /* 0 SCLK   1st green  J1.7  */
    {GPIO_LCD_SPI_IOMUX_PICO, GPIOB, DL_GPIO_PIN_8},   /* 1 MOSI   1st blue   J2.15 */
    {PB_LCD_CS_IOMUX,         GPIOB, DL_GPIO_PIN_6},   /* 2 LCD_CS purple     J2.13 */
    {PB_LCD_DC_IOMUX,         GPIOB, DL_GPIO_PIN_1},   /* 3 LCD_DC brown      J4.39 */
    {PB_LCD_RST_IOMUX,        GPIOB, DL_GPIO_PIN_4},   /* 4 LCD_RST grey      J4.40 */
    {PA_LCD_BL_IOMUX,         GPIOA, DL_GPIO_PIN_28},  /* 5 LCD_BL 2nd blue   J4.38 */
    {GPIO_TP_I2C_IOMUX_SDA,   GPIOB, DL_GPIO_PIN_3},   /* 6 TP_SDA 2nd yellow J1.10 */
    {GPIO_TP_I2C_IOMUX_SCL,   GPIOB, DL_GPIO_PIN_2},   /* 7 TP_SCL 2nd orange J1.9  */
    {PB_TP_INT_IOMUX,         GPIOB, DL_GPIO_PIN_16},  /* 8 INT    2nd green  J2.11 */
    {PB_TP_RST_IOMUX,         GPIOB, DL_GPIO_PIN_0},   /* 9 RST    2nd white  J2.12 */
    {GPIO_LCD_SPI_IOMUX_POCI, GPIOB, DL_GPIO_PIN_7},   /* 10 PB7 unwired: the control */
};
#define N (sizeof(P) / sizeof(P[0]))
struct { uint32_t magic; uint8_t pin[N]; } res;

int main(void)
{
    SYSCFG_DL_init();
    for (unsigned i = 0; i < N; i++) {
        uint8_t r = 0;
        DL_GPIO_disableOutput(P[i].port, P[i].pin);   /* outputs must let go of the pin first */
        for (int k = 0; k < 2; k++) {
            DL_GPIO_initDigitalInputFeatures(P[i].pincm, DL_GPIO_INVERSION_DISABLE,
                k ? DL_GPIO_RESISTOR_PULL_DOWN : DL_GPIO_RESISTOR_PULL_UP,
                DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
            delay_cycles(32000);
            if (DL_GPIO_readPins(P[i].port, P[i].pin)) r |= 1 << k;
        }
        res.pin[i] = r;
    }
    res.magic = 0x9A0BE000 | N;
    for (;;) ;
}
