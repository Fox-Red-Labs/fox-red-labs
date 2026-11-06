/* Three ways, C2000Ware DriverLib on the LAUNCHXL-F2800137: the five glue calls for common/app.c. */
#include "driverlib.h"
#include "device.h"
#include "app.h"

void board_init(void)
{
    Device_init();
    Device_initGPIO();
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    SCI_performSoftwareReset(SCIA_BASE);
    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, 115200, SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE | SCI_CONFIG_PAR_NONE);
    SCI_resetChannels(SCIA_BASE);
    SCI_enableFIFO(SCIA_BASE);
    SCI_enableModule(SCIA_BASE);
    SCI_performSoftwareReset(SCIA_BASE);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);
    GPIO_writePin(DEVICE_GPIO_PIN_LED1, 1);
    ADC_setVREF(ADCA_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V);
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);
    DEVICE_DELAY_US(1000);
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN6, 10);
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

uint16_t adc_read(void)
{
    ADC_forceSOC(ADCA_BASE, ADC_SOC_NUMBER0);
    while (!ADC_getInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1)) {
    }
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    return ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
}

void uart_write(const char *s, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        SCI_writeCharBlockingFIFO(SCIA_BASE, (uint16_t)s[i] & 0xFFu);
    }
}

void led_toggle(void)
{
    GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
}

void sleep_ms(uint32_t ms)
{
    DEVICE_DELAY_US(ms * 1000UL);
}

int main(void)
{
    board_init();
    app_run();
    return 0;
}
