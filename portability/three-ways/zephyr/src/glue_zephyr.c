/* Three ways, Zephyr glue: the five calls in app.h on Zephyr's GPIO and UART APIs.
 * Zephyr 4.4.2 has no ADC driver for MSPM0, so the ADC and its internal reference are
 * DriverLib calls from Zephyr's TI module: the same settings SysConfig generates for the DriverLib way. */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <ti/driverlib/dl_adc12.h>
#include <ti/driverlib/dl_vref.h>
#include "app.h"

#define ADC_CHANNEL DT_PROP(DT_PATH(zephyr_user), adc_channel)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct device *const uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static const DL_ADC12_ClockConfig adc_clock = {.clockSel = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8, .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32};
static const DL_VREF_ClockConfig vref_clock = {
    .clockSel = DL_VREF_CLOCK_BUSCLK, .divideRatio = DL_VREF_CLOCK_DIVIDE_1};
static const DL_VREF_Config vref_config = {.vrefEnable = DL_VREF_ENABLE_ENABLE,
    .bufConfig = DL_VREF_BUFCONFIG_OUTPUT_2_5V, .shModeEnable = DL_VREF_SHMODE_DISABLE,
    .holdCycleCount = DL_VREF_HOLD_MIN, .shCycleCount = DL_VREF_SH_MIN};

void board_init(void)
{
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    DL_ADC12_reset(ADC0);
    DL_VREF_reset(VREF);
    DL_ADC12_enablePower(ADC0);
    DL_VREF_enablePower(VREF);
    k_busy_wait(1);
    DL_VREF_setClockConfig(VREF, (DL_VREF_ClockConfig *) &vref_clock);
    DL_VREF_configReference(VREF, (DL_VREF_Config *) &vref_config);
    DL_ADC12_setClockConfig(ADC0, (DL_ADC12_ClockConfig *) &adc_clock);
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_0, ADC_CHANNEL,
        DL_ADC12_REFERENCE_VOLTAGE_INTREF, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(ADC0, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(ADC0, 500);
    DL_ADC12_enableConversions(ADC0);
    while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
    }
}

uint16_t adc_read(void)
{
    DL_ADC12_startConversion(ADC0);
    while (!(DL_ADC12_getRawInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))) {
    }
    uint16_t v = DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_0);
    DL_ADC12_clearInterruptStatus(ADC0, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC0);
    return v;
}

void uart_write(const char *s, uint32_t len)
{
    while (len--) {
        uart_poll_out(uart, *s++);
    }
}

void led_toggle(void)
{
    gpio_pin_toggle_dt(&led);
}

void sleep_ms(uint32_t ms)
{
    k_msleep(ms);
}

int main(void)
{
    board_init();
    app_run();
    return 0;
}
