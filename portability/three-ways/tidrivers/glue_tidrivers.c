/* Three ways, TI Drivers glue: the five calls in app.h on ti/drivers (ADC, GPIO, UART) and the
 * driver porting layer (ClockP). The same file builds under FreeRTOS (main_freertos.c, the way TI
 * ships every MSPM0 TI Drivers example) and with no RTOS (main_nortos.c). */
#include <ti/drivers/ADC.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_drivers_config.h"
#include "app.h"

static ADC_Handle adc;
static UART_Handle uart;

void board_init(void)
{
    ADC_Params ap;
    UART_Params up;
    GPIO_init();
    ADC_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW | CONFIG_GPIO_LED_0_IOMUX);
    ADC_Params_init(&ap);
    adc = ADC_open(CONFIG_ADC_0, &ap);
    UART_Params_init(&up);
    up.baudRate = 115200;
    uart = UART_open(CONFIG_UART_0, &up);
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
    UART_write(uart, s, len, NULL);
}

void led_toggle(void)
{
    GPIO_toggle(CONFIG_GPIO_LED_0);
}

void sleep_ms(uint32_t ms)
{
    ClockP_usleep(ms * 1000u);
}
