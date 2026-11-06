/* External interrupt INTP0 = the LaunchPad kit's S2 button on PA14 (pull-up, both edges). */
#include "intp_demo.h"
#include "gpio_demo.h"

uint8_t flag;
void Intp_Init(void)
{
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM15, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
                                     DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_setLowerPinsPolarity(GPIOA, DL_GPIO_PIN_14_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOA, DL_GPIO_PIN_14);
    ISR_Register(INTP01_IRQn, intp0_interrupt);
    INTP_Start(INTP0);
}
void Key_Init(void) { Intp_Init(); }
void INTP_Start(uint16_t m) { (void) m; DL_GPIO_enableInterrupt(GPIOA, DL_GPIO_PIN_14); NVIC_EnableIRQ(GPIOA_INT_IRQn); }
void INTP_Stop(uint16_t m)  { (void) m; DL_GPIO_disableInterrupt(GPIOA, DL_GPIO_PIN_14); }
void intp0_interrupt(void)
{
    DL_GPIO_clearInterruptStatus(GPIOA, DL_GPIO_PIN_14);
    flag = 1;
}
void key_interrupt(void)      { intp0_interrupt(); }
void intp4To15_interrupt(void) { intp0_interrupt(); }
