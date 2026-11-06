/*
 * FreeRTOS blink on the LP-MSPM0C1104: one task toggles LED1 (red, PA22) every 500 ms
 * with vTaskDelay. Clock from the SysConfig of TI's gpio_toggle_output example (SYSOSC 24 MHz).
 * The red LED blinks; TI's own blink example cycles the RGB LED, so the two are easy to tell apart.
 *
 * Results in RAM (struct fr_blink), read with the debugger:
 *   magic       0xB1E00001 after the 4th toggle (2 s after reset)
 *   toggles     LED toggles so far, 2 per second
 *   ticks       FreeRTOS tick count at the last toggle, 1,000 per second; it goes up by 500 per toggle
 *   hwm_blink   blink task: stack words never used (uxTaskGetStackHighWaterMark)
 *   hwm_idle    idle task: stack words never used
 *   stack_words stack of each task, in 32-bit words
 */
#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fr_common.h"

volatile struct {
    uint32_t magic;
    uint32_t toggles;
    uint32_t ticks;
    uint32_t hwm_blink;
    uint32_t hwm_idle;
    uint32_t stack_words;
} fr_blink;

static StaticTask_t blink_tcb;
static StackType_t blink_stack[configMINIMAL_STACK_SIZE];

#ifdef SECOND_TASK /* fit test: one more task that only sleeps */
static StaticTask_t second_tcb;
static StackType_t second_stack[configMINIMAL_STACK_SIZE];
static void second_task(void *arg)
{
    (void) arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

static void blink_task(void *arg)
{
    (void) arg;
    for (;;) {
        DL_GPIO_togglePins(GPIOA, GPIO_LEDS_USER_LED_1_PIN);
        fr_blink.toggles++;
        fr_blink.ticks = xTaskGetTickCount();
#if INCLUDE_uxTaskGetStackHighWaterMark && INCLUDE_xTaskGetIdleTaskHandle /* TI's own file has both off */
        fr_blink.hwm_blink = uxTaskGetStackHighWaterMark(NULL);
        fr_blink.hwm_idle = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
#endif
        if (fr_blink.toggles >= 4) {
            fr_blink.magic = 0xB1E00001u;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    SYSCFG_DL_init();
    fr_led1_init();
    fr_blink.stack_words = configMINIMAL_STACK_SIZE;
    xTaskCreateStatic(blink_task, "blnk", configMINIMAL_STACK_SIZE, NULL, 1, blink_stack, &blink_tcb);
#ifdef SECOND_TASK
    xTaskCreateStatic(second_task, "two", configMINIMAL_STACK_SIZE, NULL, 1, second_stack, &second_tcb);
#endif
    vTaskStartScheduler();
    for (;;) {
    }
}
