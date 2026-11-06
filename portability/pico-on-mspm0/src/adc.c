/* Pico SDK on MSPM0: hardware/adc.h on ADC0, one conversion into MEM0 per read. ADC0 is set up by SysConfig (VDDA reference,
 * 12 bits); adc_select_input changes MEM0's channel. */
#include "ti_msp_dl_config.h"
#include "pico_mspm0_board.h"
#include "hardware/adc.h"
void adc_init(void) { }
void adc_gpio_init(uint gpio)                              /* digital side off, no pulls: the pin is high impedance */
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p) { return; }
    DL_GPIO_disableOutput(p->port, p->bit); IOMUX->SECCFG.PINCM[p->iomux] = IOMUX_PINCM_PC_UNCONNECTED;
}
void adc_select_input(uint input)
{
    if (input > 4 || pico_adc_inputs[input] < 0) { return; }
    DL_ADC12_disableConversions(PICO_ADC_INST);
    PICO_ADC_INST->ULLMEM.MEMCTL[0] = (PICO_ADC_INST->ULLMEM.MEMCTL[0] & ~ADC12_MEMCTL_CHANSEL_MASK) | (uint32_t) pico_adc_inputs[input];
}
uint16_t adc_read(void)
{
    DL_ADC12_clearInterruptStatus(PICO_ADC_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(PICO_ADC_INST); DL_ADC12_startConversion(PICO_ADC_INST);
    while (!DL_ADC12_getRawInterruptStatus(PICO_ADC_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED)) { }
    return (uint16_t) DL_ADC12_getMemResult(PICO_ADC_INST, DL_ADC12_MEM_IDX_0);
}
