/* Blink the red LED (CONFIG_GPIO_LED_0) of the LP-CC1354P10-1 five times a second, count toggles in `blink_count`.
 * Same job as TI's drivers/empty example with the LED toggle sped up to 0.2 s and a counter a debugger can read. */
#include <stdint.h>
#include <stddef.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_drivers_config.h"

volatile uint32_t blink_count;

void *mainThread(void *arg0)
{
    GPIO_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    while (1) {
        ClockP_usleep(100000);
        GPIO_toggle(CONFIG_GPIO_LED_0);
        blink_count++;
    }
}
