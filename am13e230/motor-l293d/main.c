/* Kit DC motor through an L293D on the LP-AM13E230.
 * IN1 = MCPWM4 1A (PB3, J4 pin 40), IN2 = MCPWM4 2A (PB1, J4 pin 38). 20 kHz PWM.
 * Forward: IN1 carries the duty, IN2 held low. Reverse: the other way round. Stop: both low (coast).
 * Loop: ramp up forward, hold, ramp down, stop, same in reverse. LED4 (PA5, low = on) is on while driven.
 * Live values for run_image.py: g_pct (signed percent), g_phase, g_cycles.
 */
#include <stdlib.h>
#include "device.h"
#include "ti_sdk_dl_config.h"

#define PRD        9999U   /* MOTOR_PWM period count: 200 MHz / 10,000 = 20 kHz */
#define TOP_PCT    80      /* top speed, percent of 5 V (the kit motor is a 3 to 6 V part) */
#define STEP_MS    50      /* ramp step time */
#define HOLD_MS    2000
#define STOP_MS    1000

volatile int32_t  g_pct    = 0;   /* -100..100, what the motor is told right now */
volatile uint32_t g_phase  = 0;   /* 1 ramp up fwd, 2 hold fwd, 3 ramp down fwd, 4 stop, 5..8 reverse */
volatile uint32_t g_cycles = 0;   /* full forward-and-reverse loops done */

static void pin(DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE out, DL_MCPWM_COUNTER_COMPARE_MODULE cmp, uint32_t pct)
{
    if (pct == 0U) {
        DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM_INST, out, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    } else if (pct >= 100U) {
        DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM_INST, out, DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
    } else {
        DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM_INST, cmp, (uint16_t)((pct * (PRD + 1U)) / 100U));
        DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM_INST, out, DL_MCPWM_AQ_SW_FORCE_DISABLED);
    }
}

static void drive(int32_t pct)
{
    g_pct = pct;
    if (pct > 0) {
        pin(DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_COUNTER_COMPARE_2A, 0U);
        pin(DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_COUNTER_COMPARE_1A, (uint32_t)pct);
    } else if (pct < 0) {
        pin(DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_COUNTER_COMPARE_1A, 0U);
        pin(DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_COUNTER_COMPARE_2A, (uint32_t)(-pct));
    } else {
        pin(DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_COUNTER_COMPARE_1A, 0U);
        pin(DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_COUNTER_COMPARE_2A, 0U);
    }
    if (pct != 0) DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
    else          DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
}

static void run_one_direction(int32_t sign, uint32_t phase0)
{
    int32_t p;
    g_phase = phase0;
    for (p = 0; p <= TOP_PCT; p += 2) { drive(sign * p); DEVICE_DELAY_MS(STEP_MS); }
    g_phase = phase0 + 1U;
    DEVICE_DELAY_MS(HOLD_MS);
    g_phase = phase0 + 2U;
    for (p = TOP_PCT; p >= 0; p -= 2) { drive(sign * p); DEVICE_DELAY_MS(STEP_MS); }
    g_phase = phase0 + 3U;
    drive(0);
    DEVICE_DELAY_MS(STOP_MS);
}

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    drive(0);                 /* both inputs low before the counter runs: no kick at power-up */
    DL_MCPWM_enableTBCLK();   /* time-base clock on: the PWM counter runs from here */
    while (1) {
        run_one_direction(+1, 1U);
        run_one_direction(-1, 5U);
        g_cycles++;
    }
}
