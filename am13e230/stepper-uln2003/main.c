/* Kit 28BYJ-48 stepper through its ULN2003 board on the LP-AM13E230.
 * IN1 PB3 (J4 pin 40), IN2 PB1 (J4 pin 38), IN3 PB2 (J4 pin 36), IN4 PB7 (J4 pin 34); board + to 5V, - to GND.
 * Half-step sequence, 8 states; 4,096 half-steps is one turn of the output shaft (64-step motor, 1:64 gearbox).
 * Loop: one turn one way, pause, one turn the other way. LED4 (PA5, low = on) is on while stepping.
 * Live values for run_image.py: g_turns, g_dir, g_step.
 */
#include "device.h"
#include "ti_sdk_dl_config.h"

#define STEP_US    1200U        /* per half-step; 1,000 is about the fastest this motor keeps up with from 5 V */
#define STEPS_TURN 4096U
#define PAUSE_MS   1000

volatile uint32_t g_turns = 0, g_step = 0;
volatile int32_t  g_dir   = 1;

static const uint32_t seq[8] = {
    STEP_IN1_PIN,
    STEP_IN1_PIN | STEP_IN2_PIN,
    STEP_IN2_PIN,
    STEP_IN2_PIN | STEP_IN3_PIN,
    STEP_IN3_PIN,
    STEP_IN3_PIN | STEP_IN4_PIN,
    STEP_IN4_PIN,
    STEP_IN4_PIN | STEP_IN1_PIN,
};
#define ALL (STEP_IN1_PIN | STEP_IN2_PIN | STEP_IN3_PIN | STEP_IN4_PIN)

static void coils(uint32_t on)
{
    DL_GPIO_clearPins(STEP_PORT, ALL & ~on);
    DL_GPIO_setPins(STEP_PORT, on);
}

static void turn(int32_t dir)
{
    uint32_t i, s = 0;
    g_dir = dir;
    DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
    for (i = 0; i < STEPS_TURN; i++) {
        s = (uint32_t)((int32_t)s + dir) & 7U;
        coils(seq[s]);
        g_step = i;
        DEVICE_DELAY_US(STEP_US);
    }
    coils(0);   /* all coils off between turns: no heat, no hold */
    DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
}

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    coils(0);
    while (1) {
        turn(+1);
        DEVICE_DELAY_MS(PAUSE_MS);
        turn(-1);
        DEVICE_DELAY_MS(PAUSE_MS);
        g_turns++;
    }
}
