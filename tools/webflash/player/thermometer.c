/*
 * Thermometer for MSPM0 LaunchPad kits: ADC0 reads the chip's own temperature sensor ten times a second and
 * leaves each result in its MEMRES0 register, where the browser bench reads it through the debugger, with the
 * factory trim (FACTORYREGION TEMP_SENSE0), and works out degrees C. LED2 red gives a short flash each second so
 * you can see the program running. Replaces the main source of TI's gpio_toggle_output example for the board;
 * board_leds.h and TH_CHAN come from tools/webflash/build_sensor.py.
 *
 * The ADC settings are TI's, from the SysConfig file of the MSPM0 SDK example
 * LP_MSPM0G3519/driverlib/adc12_internal_temp_sensor_mathacl: SYSOSC divided by 8, 12 bits, VDDA reference,
 * 200 sample clocks, repeat mode, a conversion started by software and stopped after its result loads.
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"
#include "frl_params.h"

#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

/* DriverLib names the VDDA reference differently by family; both are ADC12_MEMCTL_VRSEL_VDDA_VSSA. */
#ifdef DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA
#define TH_VREF DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA
#else
#define TH_VREF DL_ADC12_REFERENCE_VOLTAGE_VDDA
#endif

static const DL_ADC12_ClockConfig adcClock = {
    .clockSel = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};

volatile uint32_t th_raw;     /* the last result, also left in ADC0 MEMRES0 */
volatile uint32_t th_count;   /* conversions done since reset */

static void led(uint32_t on)
{
    if (on == PL_ACTIVE_HIGH) {
        DL_GPIO_setPins(PL_R_PORT, PL_R_PIN);
    } else {
        DL_GPIO_clearPins(PL_R_PORT, PL_R_PIN);
    }
}

int main(void)
{
    SYSCFG_DL_init();
    DL_ADC12_reset(ADC0);
    DL_ADC12_enablePower(ADC0);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_ADC12_setClockConfig(ADC0, (DL_ADC12_ClockConfig *) &adcClock);
    DL_ADC12_initSingleSample(ADC0, DL_ADC12_REPEAT_MODE_ENABLED, DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_0, TH_CHAN, TH_VREF,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_TRIGGER_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setSampleTime0(ADC0, 200);
    DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC0);

    while (1) {
        DL_ADC12_startConversion(ADC0);
        while (!DL_ADC12_getRawInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED)) {
        }
        DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        DL_ADC12_stopConversion(ADC0);
        th_raw = DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_0);
        th_count++;
        led(th_count % frl_get(1) == 0u);
        delay_cycles(frl_get(0) * CYCLES_PER_MS);
    }
}
