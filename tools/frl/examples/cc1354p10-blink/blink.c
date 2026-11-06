/* LED pattern on the LP-CC1354P10-1 red LED (CONFIG_GPIO_LED_0, DIO6), the shape of TI's drivers/empty example.
 * frl edits the pattern table. Each LED change is logged with its time in ms, so a debugger can time the pattern
 * after a halt: this chip's GPIO output register cannot be read while the program runs. */
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_drivers_config.h"

/* Fox Red Labs pattern (frl prepare --pattern): LED on, off, on, off ... in ms */
static const uint32_t pattern_ms[] = {150, 150, 150, 900};

#define FRL_LOG_LEN 64
volatile uint32_t frl_log_ms[FRL_LOG_LEN];   /* time of each LED change, ms since start, a ring */
volatile uint32_t frl_log_n;                 /* LED changes so far */

static uint32_t now_ms(void)
{
    return (uint32_t)(((uint64_t)ClockP_getSystemTicks() * ClockP_getSystemTickPeriod()) / 1000u);
}

void *mainThread(void *arg0)
{
    GPIO_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    while (1) {
        for (unsigned i = 0; i < sizeof(pattern_ms) / sizeof(pattern_ms[0]); i++) {
            GPIO_write(CONFIG_GPIO_LED_0, (i % 2 == 0) ? CONFIG_GPIO_LED_ON : CONFIG_GPIO_LED_OFF);
            frl_log_ms[frl_log_n % FRL_LOG_LEN] = now_ms();
            frl_log_n++;
            usleep(pattern_ms[i] * 1000u);
        }
    }
}
