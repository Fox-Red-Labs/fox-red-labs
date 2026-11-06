/* Backlight off and held off: PA28 (LCD_BL) driven low forever. */
#include "ti_msp_dl_config.h"
int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_clearPins(PA_PORT, PA_LCD_BL_PIN);
    for (;;) ;
}
