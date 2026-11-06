/* Three ways, DriverLib glue: the five calls in app.h on TI DriverLib, bare metal, no RTOS.
 * Names (ADC12_0_INST, UART_0_INST, GPIO_LEDS_*) come from the SysConfig file for each board. */
#include "ti_msp_dl_config.h"
#include "app.h"

void board_init(void)
{
    SYSCFG_DL_init();
    while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
    }
}

uint16_t adc_read(void)
{
    DL_ADC12_startConversion(ADC12_0_INST);
    while (!(DL_ADC12_getRawInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))) {
    }
    uint16_t v = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
    DL_ADC12_clearInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC12_0_INST);
    return v;
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
    while (ms--) {
        delay_cycles(CPUCLK_FREQ / 1000u);
    }
}

int main(void)
{
    board_init();
    app_run();
}
