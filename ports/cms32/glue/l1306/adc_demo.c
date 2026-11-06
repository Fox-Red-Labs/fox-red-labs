/* ADC0 of the MSPM0L1306 for the Cmsemicon ADC examples. Channel numbers map one to one (AN2 = A2 = PA25). */
#include "adc_demo.h"
#include "cms_pinmap.h"

volatile uint32_t g_AdcIntTaken;

static const DL_ADC12_ClockConfig clk = { .clockSel = DL_ADC12_CLOCK_ULPCLK, .divideRatio = DL_ADC12_CLOCK_DIVIDE_8, .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32 };

static void adc_power(void)
{
    DL_ADC12_reset(ADC0);
    DL_ADC12_enablePower(ADC0);
    delay_cycles(16);
    DL_ADC12_setClockConfig(ADC0, (DL_ADC12_ClockConfig *) &clk);
    DL_ADC12_setPowerDownMode(ADC0, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(ADC0, 500);
}
static void adc_single(uint32_t ch, DL_ADC12_TRIG_SRC trig)
{
    DL_ADC12_initSingleSample(ADC0, DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_SAMPLING_SOURCE_AUTO, trig,
                              DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_0, ch, DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableInterrupt(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC0);
}

/* software-triggered, one channel at a time; ADC_Converse picks the channel */
void Adc_Init(void)
{
    adc_power();
    ISR_Register(ADC_IRQn, adc_interrupt);
}
/* hardware trigger from TIM1's published event (TIM1_Config_TRGO), no wait, on AN2 */
void Adc_HardNoWait_Init(void)
{
    adc_power();
    PORT_Init(PORTA, PIN2, ANALOG_INPUT);
    adc_single(DL_ADC12_INPUT_CHAN_2, DL_ADC12_TRIG_SRC_EVENT);
    DL_ADC12_setSubscriberChanID(ADC0, 1);
    ISR_Register(ADC_IRQn, adc_interrupt);
}
void Adc_Hardwait_Init(void) { Adc_HardNoWait_Init(); }
/* scan AN0..AN3 (PA27, PA26, PA25, PA24) into MEM0..3, software trigger, continuous */
static void adc_scan(bool repeat)
{
    adc_power();
    for (PIN_TypeDef p = PIN0; p <= PIN3; p++) PORT_Init(PORTA, p, ANALOG_INPUT);
    DL_ADC12_initSeqSample(ADC0, repeat ? DL_ADC12_REPEAT_MODE_ENABLED : DL_ADC12_REPEAT_MODE_DISABLED,
                           DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_TRIG_SRC_SOFTWARE,
                           DL_ADC12_SEQ_START_ADDR_00, DL_ADC12_SEQ_END_ADDR_03,
                           DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    static const uint32_t chans[4] = { DL_ADC12_INPUT_CHAN_0, DL_ADC12_INPUT_CHAN_1, DL_ADC12_INPUT_CHAN_2, DL_ADC12_INPUT_CHAN_3 };
    for (int i = 0; i < 4; i++)
        DL_ADC12_configConversionMem(ADC0, (DL_ADC12_MEM_IDX) i, chans[i], DL_ADC12_REFERENCE_VOLTAGE_VDDA,
            DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
            DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);
    DL_ADC12_enableInterrupt(ADC0, DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC0);
    ISR_Register(ADC_IRQn, adc_interrupt);
}
void Adc_Scan_Init(void)     { adc_scan(false); }
void Adc_SoftTrig_Init(void) { adc_scan(true); }

void adc_interrupt(void) { g_AdcIntTaken++; }

/* n conversions of one channel, results into buf, average returned (software trigger, blocking) */
uint16_t ADC_Converse(ADC_Channel_t ch, uint32_t n, uint16_t *buf)
{
    uint32_t chan = (ch == ADC_Channel_Internal) ? DL_ADC12_INPUT_CHAN_11 : cms_adc_chan(ch);
    if (ch != ADC_Channel_Internal) PORT_Init(PORTA, (PIN_TypeDef) ch, ANALOG_INPUT);
    DL_ADC12_disableConversions(ADC0);
    adc_single(chan, DL_ADC12_TRIG_SRC_SOFTWARE);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++) {
        DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        uint32_t taken = g_AdcIntTaken;
        DL_ADC12_startConversion(ADC0);
        /* the result-loaded flag is cleared by the interrupt handler when one is registered, so wait on either */
        while (g_AdcIntTaken == taken && !(DL_ADC12_getRawInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))) { }
        uint16_t v = DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_0);
        if (buf) buf[i] = v;
        sum += v;
        DL_ADC12_enableConversions(ADC0);
    }
    return (uint16_t) (n ? sum / n : 0);
}
void ADC_Start(ADC_Channel_t ch) { (void) ch; DL_ADC12_startConversion(ADC0); }
void ADC_DMA_Init(ADC_Channel_t ch)
{
    (void) ch;
    DL_ADC12_enableDMA(ADC0);
    DL_ADC12_setDMASamplesCnt(ADC0, 4);
    DL_ADC12_enableDMATrigger(ADC0, DL_ADC12_DMA_MEM3_RESULT_LOADED);
    DL_ADC12_startConversion(ADC0);
}
double adc_get_temperature(int16_t dat, double CalcVolt)
{
    int16_t addat_adj = (int16_t) ((dat * 3) / CalcVolt);
    return (int16_t) (addat_adj - TSN->TSN25) * 60 / (TSN->TSN85 - TSN->TSN25) + 25;
}
