/* Three ways, DriverLib glue for the MSPM33: the five calls in app.h on TI DriverLib, bare metal, no RTOS.
 * Same as glue_driverlib.c except adc_read: the MSPM33 has the HSADC, not the ADC12.
 * Names (HSADC0_INST, UART_0_INST, GPIO_LEDS_*) come from the SysConfig file. */
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
    DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1);
    while (!DL_HSADC_getInterruptResultReadyStatus(HSADC0_INST, DL_HSADC_INT_1)) {
    }
    DL_HSADC_InterruptStatusClear(HSADC0_INST, DL_HSADC_INT_1);
    return (uint16_t) DL_HSADC_getResult(HSADC0_RESULT_INST, DL_HSADC_SOC_NUMBER0);
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
