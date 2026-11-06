/* Three ways, TI Drivers glue for the Wi-Fi SDK (CC35X1): the five calls in app.h on ti/drivers (ADC, GPIO,
 * UART2) and POSIX sleep, run from the SDK's FreeRTOS main_freertos.c, which calls mainThread. */
#include <unistd.h>
#include <ti/drivers/ADC.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include "app.h"

static ADC_Handle adc;
static UART2_Handle uart;

void board_init(void)
{
    ADC_Params ap;
    UART2_Params up;
    GPIO_init();
    ADC_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    ADC_Params_init(&ap);
    adc = ADC_open(CONFIG_ADC_0, &ap);
    UART2_Params_init(&up);
    up.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &up);
    while (adc == NULL || uart == NULL) {
    }
}

uint16_t adc_read(void)
{
    uint16_t v = 0;
    ADC_convert(adc, &v);
    return v;
}

void uart_write(const char *s, uint32_t len)
{
    UART2_write(uart, s, len, NULL);
}

void led_toggle(void)
{
    GPIO_toggle(CONFIG_GPIO_LED_0);
}

void sleep_ms(uint32_t ms)
{
    sleep(ms / 1000u);              /* usleep() fails for 1 s or more (POSIX), so split */
    usleep((ms % 1000u) * 1000u);
}

void *mainThread(void *arg0)
{
    board_init();
    app_run();
    return NULL;
}
