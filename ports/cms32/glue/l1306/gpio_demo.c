#include "gpio_demo.h"
/* Both LEDs as outputs, off (the Cmsemicon boards' LEDs are on when the pin is low; the
 * LaunchPad kit's are on when high, so "off" here means the pin high, as the examples set it). */
void Led_Init(void)
{
    PORT_Init(PORTB, PIN6, OUTPUT);
    PORT_Init(PORTB, PIN7, OUTPUT);
    PORT_SetBit(PORTB, PIN6);
    PORT_SetBit(PORTB, PIN7);
    RESTPinGpio_Set(ENABLE);
}
void GPIO_Init(void) { Led_Init(); }
