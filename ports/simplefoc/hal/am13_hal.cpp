/* SimpleFOC hardware layer for the LP-AM13E230 + DRV8376EVM: the whole "port".
   SimpleFOC's own drivers/hardware_api.h names the functions a new chip supplies; this file is those functions
   plus the few Arduino calls the core files use. SimpleFOC's code is not modified.
   Pins SimpleFOC sees: 0,1,2 = Hall A,B,C; 100 = DRVOFF (SimpleFOC's driver enable); the six PWM "pins" are unused,
   because MCPWM4 was set up by SysConfig (simplefoc.syscfg) on J4 40,39,38,37,36,35. */
#include "Arduino.h"
#include "drivers/hardware_api.h"
extern "C" {
#include "device.h"
#include "ti_sdk_dl_config.h"
}

#define PRD 9999U   /* 200 MHz / 10,000 = 20 kHz; the same as the lab's six-step and FOC images */

/* ---- time: DWT cycle counter -> microseconds, 32-bit math only ---- */
extern "C" void frl_time_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
unsigned long micros(void)
{
    static uint32_t last = 0, rem = 0, us = 0;
    uint32_t c = DWT->CYCCNT, d = c - last; last = c;
    rem += d; us += rem / 200U; rem %= 200U;
    return us;
}
unsigned long millis(void) { return micros() / 1000UL; }
void delay(unsigned long ms) { unsigned long t = micros(); while ((micros() - t) < ms * 1000UL) {} }
void delayMicroseconds(unsigned long us) { unsigned long t = micros(); while ((micros() - t) < us) {} }

/* ---- pins ---- */
void pinMode(int pin, int mode) { (void)pin; (void)mode; }   /* SysConfig set every direction */
int digitalRead(int pin)
{
    switch (pin) {
    case 0: return (DL_GPIO_readPins(HALLB_PORT, HALLB_A_PIN) != 0U);
    case 1: return (DL_GPIO_readPins(HALLB_PORT, HALLB_B_PIN) != 0U);
    case 2: return (DL_GPIO_readPins(HALLA_PORT, HALLA_C_PIN) != 0U);
    default: return 0;
    }
}
void digitalWrite(int pin, int v)
{
    if (pin == 100) { if (v) DL_GPIO_setPins(DRVB_PORT, DRVB_DRVOFF_PIN); else DL_GPIO_clearPins(DRVB_PORT, DRVB_DRVOFF_PIN); }
}
void analogWrite(int pin, int value) { (void)pin; (void)value; }

/* ---- PWM: the 6PWM entry points ---- */
static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE INH[3] = { DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_OUTPUT_3A };
static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE INL[3] = { DL_MCPWM_AQ_OUTPUT_1B, DL_MCPWM_AQ_OUTPUT_2B, DL_MCPWM_AQ_OUTPUT_3B };
static const DL_MCPWM_COUNTER_COMPARE_MODULE CMP[3] = { DL_MCPWM_COUNTER_COMPARE_1A, DL_MCPWM_COUNTER_COMPARE_2A, DL_MCPWM_COUNTER_COMPARE_3A };
static uint8_t s_off[3] = {1, 1, 1};
static GenericDriverParams s_params;

static void phase_set_off(int p, bool off)
{
    if (off == (s_off[p] != 0U)) return;
    s_off[p] = off ? 1U : 0U;
    DL_MCPWM_ACTION_QUALIFIER_SW_FORCE_OUTPUT a = off ? DL_MCPWM_AQ_SW_CONTINUOUS_LOW : DL_MCPWM_AQ_SW_FORCE_DISABLED;
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM_INST, INH[p], a);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM_INST, INL[p], a);
}

void* _configure6PWM(long pwm_frequency, float dead_zone, const int pinA_h, const int pinA_l, const int pinB_h, const int pinB_l, const int pinC_h, const int pinC_l)
{
    (void)dead_zone; (void)pinA_h; (void)pinA_l; (void)pinB_h; (void)pinB_l; (void)pinC_h; (void)pinC_l;
    if (pwm_frequency != 20000L) return SIMPLEFOC_DRIVER_INIT_FAILED;   /* the period is fixed in SysConfig */
    for (int p = 0; p < 3; p++) {
        DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM_INST, CMP[p], 0);
        s_off[p] = 0; phase_set_off(p, true);
    }
    s_params.pwm_frequency = pwm_frequency;
    return &s_params;
}

void _writeDutyCycle6PWM(float dc_a, float dc_b, float dc_c, PhaseState* phase_state, void* params)
{
    (void)params;
    const float dc[3] = { dc_a, dc_b, dc_c };
    for (int p = 0; p < 3; p++) {
        bool off = (phase_state[p] == PhaseState::PHASE_OFF);
        if (!off) {
            float d = dc[p]; if (d < 0.02f) d = 0.02f; if (d > 0.98f) d = 0.98f;   /* the same 2 to 98 percent clamp as the lab's sine image */
            DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM_INST, CMP[p], (uint16_t)(d * (float)(PRD + 1U)));
        }
        phase_set_off(p, off);
    }
}
