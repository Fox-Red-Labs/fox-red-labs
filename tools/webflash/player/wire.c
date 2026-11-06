/*
 * Your first wire, on the LP-MSPM0G3519: one jumper from J3 pin 30 (PA15, the DAC's output) to J1 pin 6 (PA25,
 * ADC0 channel 2) closes a loop the page drives through the debugger. The page writes a DAC code into lp.code and
 * bumps lp.seq_in; the chip sets the DAC, waits 2 ms, takes 16 ADC readings and leaves their mean, lowest and
 * highest in lp, then sets lp.seq_out. With the wire in, the ADC follows the DAC; without it, PA25 floats.
 * DAC and ADC both use VDDA as their reference, so a perfect loop reads back the code it was sent.
 * DAC settings from TI's dac12_fixed_voltage_vref_internal example (MFPCLK on, output buffer on) with the VDDA
 * reference; ADC settings as in thermometer.c. Replaces the main source of TI's gpio_toggle_output example;
 * built by tools/webflash/build_wire.py.
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"

#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

#ifdef DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA
#define LP_VREF DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA
#else
#define LP_VREF DL_ADC12_REFERENCE_VOLTAGE_VDDA
#endif

static const DL_ADC12_ClockConfig adcClock = {
    .clockSel = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};

static const DL_DAC12_Config dacConfig = {
    .outputEnable = DL_DAC12_OUTPUT_ENABLED,
    .resolution = DL_DAC12_RESOLUTION_12BIT,
    .representation = DL_DAC12_REPRESENTATION_BINARY,
    .voltageReferenceSource = DL_DAC12_VREF_SOURCE_VDDA_VSSA,
    .amplifierSetting = DL_DAC12_AMP_ON,
    .fifoEnable = DL_DAC12_FIFO_DISABLED,
    .fifoTriggerSource = DL_DAC12_FIFO_TRIGGER_SAMPLETIMER,
    .dmaTriggerEnable = DL_DAC12_DMA_TRIGGER_DISABLED,
    .dmaTriggerThreshold = DL_DAC12_FIFO_THRESHOLD_ONE_QTR_EMPTY,
    .sampleTimeGeneratorEnable = DL_DAC12_SAMPLETIMER_DISABLE,
    .sampleRate = DL_DAC12_SAMPLES_PER_SECOND_500,
};

volatile struct {
    uint32_t magic;     /* 0xD1A10001 once ready */
    uint32_t cpuclk_hz;
    uint32_t seq_in;    /* the page writes code, then sets seq_in to a new value */
    uint32_t seq_out;   /* equals seq_in once mean, lo and hi belong to that code */
    uint32_t code;      /* DAC code, 0 to 4095 */
    uint32_t mean;      /* ADC mean of 16 readings, times 16 */
    uint32_t lo;
    uint32_t hi;
} lp;

static uint32_t adc_once(void)
{
    DL_ADC12_startConversion(ADC0);
    while (!DL_ADC12_getRawInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED)) {
    }
    DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_stopConversion(ADC0);
    return DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_0);
}

int main(void)
{
    SYSCFG_DL_init();
    DL_SYSCTL_enableMFPCLK();
    DL_DAC12_reset(DAC0);
    DL_ADC12_reset(ADC0);
    DL_DAC12_enablePower(DAC0);
    DL_ADC12_enablePower(ADC0);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_DAC12_init(DAC0, (DL_DAC12_Config *) &dacConfig);
    DL_DAC12_enable(DAC0);
    DL_ADC12_setClockConfig(ADC0, (DL_ADC12_ClockConfig *) &adcClock);
    DL_ADC12_initSingleSample(ADC0, DL_ADC12_REPEAT_MODE_ENABLED, DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_0, DL_ADC12_INPUT_CHAN_2, LP_VREF,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_TRIGGER_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setSampleTime0(ADC0, 40);
    DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC0);
    DL_DAC12_output12(DAC0, 0);
    lp.cpuclk_hz = CPUCLK_FREQ;
    lp.seq_out = lp.seq_in;
    lp.magic = 0xD1A10001u;
    uint32_t beat = 0;
    while (1) {
        if (lp.seq_in != lp.seq_out) {
            uint32_t seq = lp.seq_in, sum = 0, lo = 4095, hi = 0;
            DL_DAC12_output12(DAC0, lp.code & 0xFFFu);
            delay_cycles(2u * CYCLES_PER_MS);
            for (int i = 0; i < 16; i++) {
                uint32_t v = adc_once();
                sum += v;
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
            lp.mean = sum;
            lp.lo = lo;
            lp.hi = hi;
            lp.seq_out = seq;
        }
        if (++beat == 200000u) {
            beat = 0;
            DL_GPIO_togglePins(PL_R_PORT, PL_R_PIN);
        }
    }
}
