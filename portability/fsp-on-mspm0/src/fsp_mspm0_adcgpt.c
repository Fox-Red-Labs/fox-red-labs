/* ELC, DTC and ADC group scan on MSPM0 (LP-MSPM0G3519). Names and call order follow the public FSP interface; the code is ours.
 * FSP ELC link (timer overflow to ADC start)  -> event fabric: TIMG0 zero event published on channel 1, ADC0 subscribes to channel 1.
 * FSP DTC transfer_info_t elements             -> one DMA channel each (0, 1 = group A; 2, 3 = group B), all triggered by ADC0's
 *                                                 "MEM7 result loaded" DMA event, the last conversion of the four-channel sequence.
 * FSP ADC group scan A and B                   -> one sequence MEM4 to MEM7 on one trigger (this ADC has one start trigger per sequence). */
#include "ti_msp_dl_config.h"
#include "hal_data.h"
#include "SEGGER_RTT/SEGGER_RTT.h"

#define EVENT_CHAN   1
static void (*s_acb)(adc_callback_args_t *); static void const *s_actx;
static transfer_ctrl_t *s_dtc[2];                     /* group A, group B */

/* ---- ELC ---- */
static const elc_cfg_t *s_ecfg;
fsp_err_t R_ELC_Open(elc_ctrl_t *c, const elc_cfg_t *f) { if (!c || !f) { return FSP_ERR_ASSERTION; } s_ecfg = f; c->open = true; return FSP_SUCCESS; }
fsp_err_t R_ELC_Enable(elc_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    if (s_ecfg->link[ELC_PERIPHERAL_ADC0] == ELC_EVENT_GPT0_COUNTER_OVERFLOW) {
        DL_Timer_setPublisherChanID(TIMG0, DL_TIMER_PUBLISHER_INDEX_0, EVENT_CHAN);
        DL_Timer_enableEvent(TIMG0, DL_TIMER_EVENT_ROUTE_1, DL_TIMER_EVENT_ZERO_EVENT);
        DL_ADC12_setSubscriberChanID(ADC12_0_INST, EVENT_CHAN);
    }
    return FSP_SUCCESS;
}
fsp_err_t R_ELC_Close(elc_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    DL_ADC12_setSubscriberChanID(ADC12_0_INST, 0); DL_Timer_setPublisherChanID(TIMG0, DL_TIMER_PUBLISHER_INDEX_0, 0); c->open = false;
    return FSP_SUCCESS;
}

/* ---- ADC group scan: sequence and pins come from SysConfig, the trigger and the DMA event from here ---- */
void adc_group_open(const adc_cfg_t *f) { s_acb = f->p_callback; s_actx = f->p_context; }
void adc_group_scancfg(const adc_channel_cfg_t *f)
{
    (void) f;                                          /* the mask names MEM4 to MEM7, which board.syscfg's sequence converts */
    DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 1); DL_ADC12_enableDMA(ADC12_0_INST); DL_ADC12_enableDMATrigger(ADC12_0_INST, DL_ADC12_DMA_MEM7_RESULT_LOADED);
}
#ifdef FSP_SHIM_MEASURE
static void measure_start(void);
#endif
void adc_group_start(void)
{
#ifdef FSP_SHIM_MEASURE
    measure_start();
#endif
    DL_ADC12_enableConversions(ADC12_0_INST);
}
void adc_group_stop(void) { DL_ADC12_disableConversions(ADC12_0_INST); }

/* ---- DTC ---- */
static uint8_t chain_count(const transfer_info_t *p) { uint8_t n = 1; while (n < 2 && p[n - 1].transfer_settings_word_b.chain_mode != TRANSFER_CHAIN_MODE_DISABLED) { n++; } return n; }

#ifdef FSP_SHIM_MEASURE
static volatile uint32_t s_tdone, s_gap_max, s_gap_sum, s_gap_n;
#endif
fsp_err_t R_DTC_Open(transfer_ctrl_t *c, const transfer_cfg_t *f)
{
    if (!c || !f) { return FSP_ERR_ASSERTION; }
    if (c->open) { return FSP_ERR_ALREADY_OPEN; }
    c->first_channel = (f->event == ADC_EVENT_SCAN_COMPLETE) ? 0 : 2; c->event = f->event; c->p_info = f->p_info; c->count = 1;
    s_dtc[c->first_channel / 2] = c; c->open = true;
    return FSP_SUCCESS;
}
fsp_err_t R_DTC_Reconfigure(transfer_ctrl_t *c, transfer_info_t *p)
{
    if (!c || !p) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    c->p_info = p; c->count = chain_count(p);
#ifdef FSP_SHIM_ONECH   /* experiment: one DMA channel on the trigger */
    c->count = 1;
#endif
    return FSP_SUCCESS;
}
fsp_err_t R_DTC_Enable(transfer_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
#ifdef FSP_SHIM_ONECH
    if (c->first_channel) { return FSP_SUCCESS; }
#endif
    static const DL_DMA_Config dc = { DMA_ADC0_EVT_GEN_BD_TRIG, DL_DMA_TRIGGER_TYPE_EXTERNAL, DL_DMA_SINGLE_TRANSFER_MODE, DL_DMA_NORMAL_MODE,
                                      DL_DMA_WIDTH_HALF_WORD, DL_DMA_WIDTH_HALF_WORD, DL_DMA_ADDR_UNCHANGED, DL_DMA_ADDR_INCREMENT };
    uint8_t n = c->count;
#ifdef FSP_SHIM_TSONLY   /* experiment: only the timestamp channel on the trigger */
    n = 0;
#endif
    for (uint8_t i = 0; i < n; i++) {
        uint8_t ch = c->first_channel + i;
        DL_DMA_disableChannel(DMA, ch); DL_DMA_initChannel(DMA, ch, (DL_DMA_Config *) &dc);
        DL_DMA_setSrcAddr(DMA, ch, (uint32_t) c->p_info[i].p_src); DL_DMA_setDestAddr(DMA, ch, (uint32_t) c->p_info[i].p_dest);
        DL_DMA_setTransferSize(DMA, ch, c->p_info[i].length);
        if (i == c->count - 1) { DL_DMA_clearInterruptStatus(DMA, 1u << ch); DL_DMA_enableInterrupt(DMA, 1u << ch); }
        DL_DMA_enableChannel(DMA, ch);
    }
    NVIC_EnableIRQ(DMA_INT_IRQn);
    DL_ADC12_enableDMA(ADC12_0_INST);                  /* the ADC clears this bit when a transfer ends (ADC12 CTL2.DMAEN), so each Enable sets it again */
#ifdef FSP_SHIM_MEASURE
    if (c->first_channel == 0 && s_tdone) { uint32_t g = (s_tdone - DL_Timer_getTimerCount(TIMG12)); s_gap_sum += g; s_gap_n++; if (g > s_gap_max) { s_gap_max = g; } s_tdone = 0; }
#endif
    return FSP_SUCCESS;
}
fsp_err_t R_DTC_Close(transfer_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    for (uint8_t i = 0; i < 2; i++) { DL_DMA_disableChannel(DMA, c->first_channel + i); DL_DMA_disableInterrupt(DMA, 1u << (c->first_channel + i)); }
    c->open = false;
    return FSP_SUCCESS;
}

#ifdef FSP_SHIM_MEASURE   /* measurement build: DMA channel 4 copies a free-running 32 MHz counter at every conversion; stats once 5 s of callbacks are in */
#define TS_N 1024
static uint32_t s_ts[TS_N]; static volatile bool s_ts_done; static uint32_t s_imin, s_imax, s_isum;
static uint32_t s_cb_n, s_t1;
static void measure_start(void)
{
    static const DL_DMA_Config dc = { DMA_ADC0_EVT_GEN_BD_TRIG, DL_DMA_TRIGGER_TYPE_EXTERNAL, DL_DMA_SINGLE_TRANSFER_MODE, DL_DMA_NORMAL_MODE,
                                      DL_DMA_WIDTH_WORD, DL_DMA_WIDTH_WORD, DL_DMA_ADDR_UNCHANGED, DL_DMA_ADDR_INCREMENT };
    DL_Timer_ClockConfig ck = { DL_TIMER_CLOCK_BUSCLK, DL_TIMER_CLOCK_DIVIDE_1, 0 };
    DL_Timer_TimerConfig tc = { DL_TIMER_TIMER_MODE_PERIODIC, 0xFFFFFFFFu, DL_TIMER_START, DL_TIMER_INTERM_INT_DISABLED, 0 };
    DL_Timer_reset(TIMG12); DL_Timer_enablePower(TIMG12); delay_cycles(POWER_STARTUP_DELAY);
    DL_Timer_setClockConfig(TIMG12, &ck); DL_Timer_initTimerMode(TIMG12, &tc);
    DL_DMA_initChannel(DMA, 4, (DL_DMA_Config *) &dc); DL_DMA_setSrcAddr(DMA, 4, (uint32_t) &TIMG12->COUNTERREGS.CTR);
    DL_DMA_setDestAddr(DMA, 4, (uint32_t) s_ts); DL_DMA_setTransferSize(DMA, 4, TS_N);
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL4); DL_DMA_enableInterrupt(DMA, DL_DMA_INTERRUPT_CHANNEL4); DL_DMA_enableChannel(DMA, 4);
}
static void mm(const transfer_info_t *p, uint32_t *lo, uint32_t *hi)
{
    const uint16_t *d = (const uint16_t *) p->p_dest; *lo = 0xFFFF; *hi = 0;
    for (uint32_t i = 0; i < 128; i++) { if (d[i] < *lo) { *lo = d[i]; } if (d[i] > *hi) { *hi = d[i]; } }
}
static void measure_report(uint32_t t2)
{
    uint32_t el = s_t1 - t2, trig = el / g_timer0_cfg.period_counts, got = 256u * (s_cb_n - 1), lo[4], hi[4];
    mm(&s_dtc[0]->p_info[0], &lo[0], &hi[0]); mm(&s_dtc[0]->p_info[1], &lo[1], &hi[1]); mm(&s_dtc[1]->p_info[0], &lo[2], &hi[2]); mm(&s_dtc[1]->p_info[1], &lo[3], &hi[3]);
    if (s_ts_done) SEGGER_RTT_printf(0, "[adcgpt interval counts: min %u max %u mean x100 %u of %u nominal over %u intervals]\r\n", (unsigned) s_imin, (unsigned) s_imax,
                      (unsigned) (s_isum * 100u / (TS_N - 1)), (unsigned) g_timer0_cfg.period_counts, TS_N - 1);
    SEGGER_RTT_printf(0, "[adcgpt %u group A callbacks in %u counts: %u triggers, %u samples captured, %d not captured; re-arm gap max %u mean %u counts over %u]\r\n",
                      (unsigned) s_cb_n, (unsigned) el, (unsigned) trig, (unsigned) got, (int) (trig - got), (unsigned) s_gap_max, (unsigned) (s_gap_n ? s_gap_sum / s_gap_n : 0), (unsigned) s_gap_n);
    SEGGER_RTT_printf(0, "[adcgpt MEM4 %u-%u MEM5 %u-%u MEM6 %u-%u MEM7 %u-%u]\r\n", (unsigned) lo[0], (unsigned) hi[0], (unsigned) lo[1], (unsigned) hi[1], (unsigned) lo[2], (unsigned) hi[2], (unsigned) lo[3], (unsigned) hi[3]);
}
#endif

void DMA_IRQHandler(void)
{
    uint32_t iidx;
    while ((iidx = DL_DMA_getPendingInterrupt(DMA)) != 0) {
        uint32_t ch = iidx - DL_DMA_EVENT_IIDX_DMACH0;
#ifdef FSP_SHIM_MEASURE
        if (ch == 4) {
            s_imin = 0xFFFFFFFFu; s_imax = 0; s_isum = 0;
            for (uint32_t i = 1; i < TS_N; i++) { uint32_t d = s_ts[i - 1] - s_ts[i]; if (d < s_imin) { s_imin = d; } if (d > s_imax) { s_imax = d; } s_isum += d; }
            s_ts_done = true;
#ifdef FSP_SHIM_TSONLY
            SEGGER_RTT_printf(0, "[adcgpt interval counts: min %u max %u mean x100 %u of %u nominal over %u intervals]\r\n", (unsigned) s_imin, (unsigned) s_imax, (unsigned) (s_isum * 100u / (TS_N - 1)), (unsigned) g_timer0_cfg.period_counts, TS_N - 1);
#endif
            continue;
        }
#endif
        if (ch < 4 && s_dtc[ch / 2] && ch == (uint32_t) s_dtc[ch / 2]->first_channel + s_dtc[ch / 2]->count - 1) {
#ifdef FSP_SHIM_MEASURE
            if (ch < 2) {
                uint32_t now = DL_Timer_getTimerCount(TIMG12); s_tdone = now; s_cb_n++;
                if (s_cb_n == 1) { s_t1 = now; s_gap_max = s_gap_sum = s_gap_n = 0; }
                if (s_cb_n == 931) { measure_report(now); }
            }
#endif
            if (s_acb) { adc_callback_args_t a = { s_dtc[ch / 2]->event, s_actx }; s_acb(&a); }
        }
    }
}
