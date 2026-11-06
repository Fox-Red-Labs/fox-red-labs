/* Three ways, glue for the LP-AM13E230: the five calls in app.h on the AM13E2x SDK's DriverLib, bare metal, no RTOS.
 * Names (APP_ADC_*, UART_0_INST, GPIO_LEDS_*) come from am13e230.syscfg. The ADC is a sequencer/SOC block, not the MSPM0 ADC12. */
#include "device.h"
#include "ti_sdk_dl_config.h"
#include "app.h"

void board_init(void)
{
    Device_Init();
    SYSCFG_DL_init();
    DL_ADC_powerUp(APP_ADC_INST);
    DEVICE_DELAY_US(5000);
}

uint16_t adc_read(void)
{
    DL_ADC_forceSequencer(APP_ADC_INST, APP_ADC_SEQ1);
    while (DL_ADC_getSOCStatus(APP_ADC_INST, APP_ADC_SOC0)) { /* trigger pending, conversion not started */
    }
    while (DL_ADC_isBusy(APP_ADC_INST)) {
    }
    return (uint16_t) DL_ADC_readResult(APP_ADC_RESULT_INST, APP_ADC_SOC0);
}

void uart_write(const char *s, uint32_t len)
{
    while (len--) {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) *s++);
    }
}

void led_toggle(void)
{
    DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_PIN);
}

void sleep_ms(uint32_t ms)
{
    DEVICE_DELAY_MS(ms);
}

int main(void)
{
    board_init();
    app_run();
}
