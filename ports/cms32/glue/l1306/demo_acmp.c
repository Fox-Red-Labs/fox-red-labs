/* The MSPM0L1306 has one comparator, COMP0 (the CMS32F040 has two). ACMP0: IN+ from a pin, IN- from the
 * internal 8-bit DAC at VDD/4, output on PA7, 20 mV hysteresis. ACMP1: IN+ and IN- from pins, interrupt on
 * the output edge; it also lands on COMP0. */
#include "demo_acmp.h"

static void comp_common(const DL_COMP_Config *c, const DL_COMP_RefVoltageConfig *r)
{
    DL_COMP_reset(COMP0); DL_COMP_enablePower(COMP0); delay_cycles(16);
    DL_COMP_init(COMP0, (DL_COMP_Config *) c);
    if (r) { DL_COMP_refVoltageInit(COMP0, (DL_COMP_RefVoltageConfig *) r); DL_COMP_setDACCode0(COMP0, 64); }   /* 64/256 of VDD */
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM8, IOMUX_PINCM8_PF_COMP0_OUT);     /* PA7 */
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_7);
    DL_COMP_enable(COMP0);
}
void ACMP_ACMP0_Config(void)
{
    static const DL_COMP_Config c = { .mode = DL_COMP_MODE_FAST, .channelEnable = DL_COMP_ENABLE_CHANNEL_POS,
        .posChannel = DL_COMP_IPSEL_CHANNEL_0, .negChannel = DL_COMP_IMSEL_CHANNEL_0,
        .polarity = DL_COMP_POLARITY_NON_INV, .hysteresis = DL_COMP_HYSTERESIS_20 };
    static const DL_COMP_RefVoltageConfig r = { .mode = DL_COMP_REF_MODE_STATIC, .source = DL_COMP_REF_SOURCE_VDDA_DAC,
        .terminalSelect = DL_COMP_REF_TERMINAL_SELECT_NEG, .controlSelect = DL_COMP_DAC_CONTROL_SW, .inputSelect = DL_COMP_DAC_INPUT_DACCODE0 };
    comp_common(&c, &r);
}
void ACMP_ACMP1_Config(void)
{
    static const DL_COMP_Config c = { .mode = DL_COMP_MODE_FAST, .channelEnable = DL_COMP_ENABLE_CHANNEL_POS_NEG,
        .posChannel = DL_COMP_IPSEL_CHANNEL_0, .negChannel = DL_COMP_IMSEL_CHANNEL_0,
        .polarity = DL_COMP_POLARITY_NON_INV, .hysteresis = DL_COMP_HYSTERESIS_NONE };
    comp_common(&c, 0);
    DL_COMP_setOutputInterruptEdge(COMP0, DL_COMP_OUTPUT_INT_EDGE_RISING);
    DL_COMP_enableInterrupt(COMP0, DL_COMP_INTERRUPT_OUTPUT_EDGE);
    ISR_Register(ACMP1_IRQn, acmp_interrupt);
}
void acmp_interrupt(void) { }
