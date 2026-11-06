/* Which wire runs the backlight? Repeats forever:
 *   10 s: PA28 (2nd blue, J4.38) blinks SLOW, 1 s on / 1 s off, PB1 held high
 *   10 s: PB1 (brown, J4.39) blinks FAST, 4 times a second, PA28 held high */
#include "ti_msp_dl_config.h"
volatile uint32_t phase;
static void ms(uint32_t n) { while (n--) delay_cycles(CPUCLK_FREQ / 1000); }
int main(void)
{
    SYSCFG_DL_init();
    for (;;) {
        phase = 1; DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN);
        for (int i = 0; i < 5; i++) {
            DL_GPIO_clearPins(PA_PORT, PA_LCD_BL_PIN); ms(1000);
            DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);   ms(1000);
        }
        phase = 2;
        for (int i = 0; i < 40; i++) {
            DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN); ms(125);
            DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN);   ms(125);
        }
    }
}
