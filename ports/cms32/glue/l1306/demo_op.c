/* General-purpose op amps, inputs and output on pins, gain set by external parts, as in the original.
 * OPA0: IN0+ PA26, IN0- PA24, OUT PA22 (the LaunchPad kit's light sensor hangs on OPA0). OPA1: IN0+ PA18, IN1- PA17, OUT PA16. */
#include "demo_op.h"
static void opa(OA_Regs *o, DL_OPA_NSEL n)
{
    DL_OPA_reset(o); DL_OPA_enablePower(o); delay_cycles(16);
    DL_OPA_Config c = { .pselChannel = DL_OPA_PSEL_IN0_POS, .nselChannel = n, .mselChannel = DL_OPA_MSEL_OPEN,
        .gain = DL_OPA_GAIN_N0_P1, .outputPinState = DL_OPA_OUTPUT_PIN_ENABLED, .choppingMode = DL_OPA_CHOPPING_MODE_DISABLE };
    DL_OPA_init(o, &c);
    DL_OPA_enableRailToRailInput(o);
    DL_OPA_enable(o);
}
void OP0_Config(void)
{
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM27); DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM25); DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM23);
    opa(OPA0, DL_OPA_NSEL_IN0_NEG);
}
void OP1_Config(void)
{
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM19); DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM18); DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM17);
    opa(OPA1, DL_OPA_NSEL_IN1_NEG);
}
