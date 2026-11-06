/* Pico SDK on MSPM0: hardware/pwm.h and hardware/irq.h. A slice is a timer from the board table, set up here at pwm_init (not
 * in SysConfig, so the same pin can be a plain GPIO in another program). Edge-aligned, counting up, high while count < level. */
#include "ti_msp_dl_config.h"
#include "pico_mspm0_board.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
static irq_handler_t s_handler[32];
void irq_set_exclusive_handler(uint num, irq_handler_t handler) { if (num < 32) { s_handler[num] = handler; } }
void irq_set_enabled(uint num, bool on) { if (on) { NVIC_EnableIRQ((IRQn_Type) num); } else { NVIC_DisableIRQ((IRQn_Type) num); } }
void TIMA1_IRQHandler(void) { if (s_handler[TIMA1_INT_IRQn]) { s_handler[TIMA1_INT_IRQn](); } }
static uint8_t s_on;                                      /* slices powered: the first call that touches a slice powers and resets it */
static GPTIMER_Regs *slice(uint n)
{
    GPTIMER_Regs *t = pico_pwm_slices[n];
    if (!(s_on & (1u << n))) { DL_Timer_reset(t); DL_Timer_enablePower(t); delay_cycles(16); s_on |= (uint8_t) (1u << n); }
    return t;
}
uint pwm_gpio_to_slice_num(uint gpio) { const pico_pin_t *p = PICO_PIN(gpio); return (p && p->pwm >= 0) ? (uint) p->pwm >> 1 : 0; }
void pwm_init(uint slice_num, pwm_config *c, bool start)   /* no reset here: the example enables the wrap interrupt before pwm_init */
{
    GPTIMER_Regs *t = slice(slice_num);
    uint32_t div = c->div16 / 16u; if (div < 1u) { div = 1u; } if (div > 256u) { div = 256u; }   /* integer part; fraction not supported */
    DL_Timer_ClockConfig cc = { .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = (uint8_t) (div - 1u) };
    DL_Timer_setClockConfig(t, &cc);
    DL_Timer_PWMConfig pc = { .period = (uint32_t) c->top + 1u, .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP, .isTimerWithFourCC = false, .startTimer = DL_TIMER_STOP };
    DL_Timer_initPWMMode(t, &pc);
    for (uint32_t i = 0; i < 2; i++) {
        DL_Timer_setCaptureCompareOutCtl(t, DL_TIMER_CC_OCTL_INIT_VAL_LOW, DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL, i);
        DL_Timer_setCaptCompUpdateMethod(t, DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT, i);       /* new level at the wrap, as on the Pico */
        DL_Timer_setCaptureCompareValue(t, 0, i);
    }
    DL_Timer_enableClock(t); DL_Timer_setCCPDirection(t, DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT);
    if (start) { DL_Timer_startCounter(t); }
}
void pwm_set_gpio_level(uint gpio, uint16_t level)
{
    const pico_pin_t *p = PICO_PIN(gpio); if (!p || p->pwm < 0) { return; }
    DL_Timer_setCaptureCompareValue(slice((uint) p->pwm >> 1), level, (uint32_t) (p->pwm & 1));
}
void pwm_clear_irq(uint n) { DL_Timer_clearInterruptStatus(slice(n), DL_TIMER_INTERRUPT_ZERO_EVENT); }
void pwm_set_irq_enabled(uint n, bool on)
{
    if (on) { DL_Timer_enableInterrupt(slice(n), DL_TIMER_INTERRUPT_ZERO_EVENT); }
    else { DL_Timer_disableInterrupt(slice(n), DL_TIMER_INTERRUPT_ZERO_EVENT); }
}
