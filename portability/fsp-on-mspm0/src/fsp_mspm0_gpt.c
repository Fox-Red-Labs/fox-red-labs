/* GPT on MSPM0 timers (LP-MSPM0G3519). Names and call order follow the public FSP interface; the code is ours.
 * Channel 0: TIMG12, 32 bit, periodic and one-shot. Channel 1: TIMA1 CC1 on PB27, PWM (SysConfig sets it up). Channel 2: TIMG0, 16 bit. */
#include "ti_msp_dl_config.h"
#include "hal_data.h"
#include "SEGGER_RTT/SEGGER_RTT.h"

#define GPT_FREQ_HZ   CPUCLK_FREQ                 /* timers on BUSCLK, no prescale, no divide */
#define PWM_FREQ_HZ   (CPUCLK_FREQ / 32)          /* PWM_0 has prescale 32 in board.syscfg */

typedef struct { GPTIMER_Regs *t; IRQn_Type irq; timer_cfg_t const *cfg; } gpt_hw_t;
static gpt_hw_t s_hw[3] = { { TIMG12, TIMG12_INT_IRQn, 0 }, { TIMA1, TIMA1_INT_IRQn, 0 }, { TIMG0, TIMG0_INT_IRQn, 0 } };

uint32_t R_FSP_SystemClockHzGet(fsp_priv_clock_t clock) { (void) clock; return GPT_FREQ_HZ; }

#ifdef FSP_SHIM_MEASURE   /* measurement build only: counters read by the console service below and by the pin sampler */
static volatile uint32_t s_ticks, s_next, s_os_cycles, s_os_t0; static volatile bool s_os_done;
static void systick_free_run(void) { SysTick->LOAD = 0xFFFFFFu; SysTick->VAL = 0; SysTick->CTRL = 5; }
#endif

fsp_err_t R_GPT_Open(timer_ctrl_t *c, const timer_cfg_t *f)
{
    if (!c || !f) { return FSP_ERR_ASSERTION; }
    if (c->open) { return FSP_ERR_ALREADY_OPEN; }
    if (f->channel > 2) { return FSP_ERR_INVALID_ARGUMENT; }
    gpt_hw_t *h = &s_hw[f->channel];
    c->channel = f->channel; c->period_counts = f->period_counts; h->cfg = f;
    if (f->mode != TIMER_MODE_PWM) {
        DL_Timer_ClockConfig ck = { DL_TIMER_CLOCK_BUSCLK, DL_TIMER_CLOCK_DIVIDE_1, 0 };
        DL_Timer_TimerConfig tc = { f->mode == TIMER_MODE_ONE_SHOT ? DL_TIMER_TIMER_MODE_ONE_SHOT : DL_TIMER_TIMER_MODE_PERIODIC,
                                    f->period_counts - 1, DL_TIMER_STOP, DL_TIMER_INTERM_INT_DISABLED, 0 };
        DL_Timer_reset(h->t); DL_Timer_enablePower(h->t); delay_cycles(POWER_STARTUP_DELAY);
        DL_Timer_setClockConfig(h->t, &ck); DL_Timer_initTimerMode(h->t, &tc);
        if (f->channel == 0 || f->p_callback) {     /* channel 0 also blinks the user LED (PA0) on each period end */
            DL_Timer_clearInterruptStatus(h->t, DL_TIMER_INTERRUPT_ZERO_EVENT);
            DL_Timer_enableInterrupt(h->t, DL_TIMER_INTERRUPT_ZERO_EVENT); NVIC_EnableIRQ(h->irq);
        }
    }
    c->open = true;
    return FSP_SUCCESS;
}
fsp_err_t R_GPT_Start(timer_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
#ifdef FSP_SHIM_MEASURE
    if (!SysTick->CTRL) { systick_free_run(); }
    if (c->channel == 0) { s_ticks = 0; s_os_done = false; s_os_t0 = SysTick->VAL; }
#endif
    DL_Timer_startCounter(s_hw[c->channel].t);
    return FSP_SUCCESS;
}
fsp_err_t R_GPT_Stop(timer_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    DL_Timer_stopCounter(s_hw[c->channel].t);
    return FSP_SUCCESS;
}
fsp_err_t R_GPT_PeriodSet(timer_ctrl_t *c, uint32_t counts)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    if (counts == 0 || (c->channel == 2 && counts > 0x10000u)) { return FSP_ERR_INVALID_ARGUMENT; }
    DL_Timer_setLoadValue(s_hw[c->channel].t, counts - 1); c->period_counts = counts;
#ifdef FSP_SHIM_MEASURE
    s_ticks = 0; s_next = 0;
#endif
    return FSP_SUCCESS;
}
fsp_err_t R_GPT_InfoGet(timer_ctrl_t *c, timer_info_t *i)
{
    if (!c || !i) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    i->count_direction = TIMER_DIRECTION_DOWN; i->period_counts = c->period_counts;
    i->clock_frequency = c->channel == 1 ? PWM_FREQ_HZ : GPT_FREQ_HZ;
    return FSP_SUCCESS;
}
/* FSP counts the pulse as the high time; this timer's edge-aligned down count writes the low time. */
fsp_err_t R_GPT_DutyCycleSet(timer_ctrl_t *c, uint32_t duty, uint32_t pin)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    if (c->channel != 1 || pin != GPT_IO_PIN_GTIOCB || duty > c->period_counts) { return FSP_ERR_INVALID_ARGUMENT; }
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, c->period_counts - duty, DL_TIMER_CC_1_INDEX);
    /* compare 0 and compare = period leave the pin high for the whole cycle bar one count, so 0 and 100 percent are forced */
    DL_Timer_overrideCCPOut(PWM_0_INST, duty == 0 ? DL_TIMER_FORCE_OUT_LOW : duty == c->period_counts ? DL_TIMER_FORCE_OUT_HIGH : DL_TIMER_FORCE_OUT_DISABLED,
                            DL_TIMER_FORCE_CMPL_OUT_DISABLED, DL_TIMER_CC_1_INDEX);
#ifdef FSP_SHIM_MEASURE   /* read PB27 for 100 ms: share of reads high, rising edges seen */
    {
        uint32_t hi = 0, n = 0, edges = 0, prev = 0, now; systick_free_run();
        IOMUX->SECCFG.PINCM[GPIO_PWM_0_C1_IOMUX] |= IOMUX_PINCM_INENA_ENABLE; delay_cycles(CPUCLK_FREQ / 100);
        SysTick->VAL = 0;
        while ((0xFFFFFFu - SysTick->VAL) < CPUCLK_FREQ / 10) {
            now = DL_GPIO_readPins(GPIO_PWM_0_C1_PORT, GPIO_PWM_0_C1_PIN) ? 1u : 0u;
            hi += now; edges += (now && !prev); prev = now; n++;
        }
        SEGGER_RTT_printf(0, "[pwm duty %u of %u counts: %u of %u reads high, %u rising edges in 100 ms]\r\n",
                          (unsigned) duty, (unsigned) c->period_counts, (unsigned) hi, (unsigned) n, (unsigned) edges);
    }
#endif
    return FSP_SUCCESS;
}
fsp_err_t R_GPT_Close(timer_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    gpt_hw_t *h = &s_hw[c->channel];
    DL_Timer_stopCounter(h->t);
    if (c->channel != 1) { NVIC_DisableIRQ(h->irq); DL_Timer_disableInterrupt(h->t, DL_TIMER_INTERRUPT_ZERO_EVENT); }
    c->open = false;
    return FSP_SUCCESS;
}

static void gpt_irq(gpt_hw_t *h)
{
    DL_Timer_clearInterruptStatus(h->t, DL_TIMER_INTERRUPT_ZERO_EVENT);
#ifdef FSP_SHIM_MEASURE
    if (h == &s_hw[0]) { if (h->cfg->mode == TIMER_MODE_ONE_SHOT) { s_os_cycles = (s_os_t0 - SysTick->VAL) & 0xFFFFFFu; s_os_done = true; } else { s_ticks++; } }
#endif
    if (h->cfg->mode == TIMER_MODE_PERIODIC && h->t == TIMG12) { DL_GPIO_togglePins(LED_PORT, LED_RED_PIN); }
    if (h->cfg->p_callback) { timer_callback_args_t a = { TIMER_EVENT_CYCLE_END, 0, h->cfg->p_context }; h->cfg->p_callback(&a); }
}
void TIMG12_IRQHandler(void) { gpt_irq(&s_hw[0]); }
void TIMG0_IRQHandler(void) { gpt_irq(&s_hw[2]); }

#ifdef FSP_SHIM_MEASURE   /* called from SEGGER_RTT_HasKey, which the example's main loop polls: prints outside interrupts */
void fsp_measure_service(void)
{
    if (s_os_done) { s_os_done = false; SEGGER_RTT_printf(0, "[one-shot %u counts: %u cycles]\r\n", (unsigned) g_timer_one_shot_mode_cfg.period_counts, (unsigned) s_os_cycles); }
    if (s_hw[0].cfg && s_hw[0].cfg->mode == TIMER_MODE_PERIODIC && g_timer_periodic_ctrl.open) {
        uint32_t per_s = CPUCLK_FREQ / g_timer_periodic_ctrl.period_counts; if (per_s == 0) { per_s = 1; }
        if (s_ticks >= s_next + per_s) { s_next += per_s; SEGGER_RTT_printf(0, "[periodic %u ticks at %u counts]\r\n", (unsigned) s_next, (unsigned) g_timer_periodic_ctrl.period_counts); }
    }
}
#endif
