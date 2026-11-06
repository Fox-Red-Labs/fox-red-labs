/*
 * Hall FOC on the LP-MSPM33C321A + DRV8376EVM + BLY171S (Fox Red Labs, 2026-10-20; rungs 10 and up of
 * portability/motor/PORTING.md). Control math from portability/motor/core (float build). Board: 160 MHz, TIMA0_1
 * center-aligned with complementary outputs and a 125 ns dead band on all three phases, its zero event starting HSADC0
 * (the three phase currents) by event channel 1; HSADC1 (bus) by software. Scripted at boot; results in RAM (`fc`),
 * read over the probe. Bench tests are chosen at build time (build.sh TESTS="...").
 *
 * Safety, every period: phase-current trip, nFAULT on a top-priority interrupt (DRVOFF high), bus window, run cap.
 * ILIMIT (PC2) is driven above VREF: the driver's cycle-by-cycle limit is off on this LaunchPad (no DAC on that pin).
 * Call SYSCFG_DL_init before any pin setup (it resets the GPIO ports).
 */
#include "ti_msp_dl_config.h"
#include "kernel.h"
#include "angle_hall.h"

#define CPU_HZ   160000000u
#define CNT_PER_A (4096.0f / 3.3f)     /* 1 V/A sense gain on the 3.3 V ADC */
#define TRIP_A   0.8f

static GPIO_Regs *const HP[3] = {GPIOC, GPIOC, GPIOB}; static const uint32_t HPIN[3] = {DL_GPIO_PIN_17, DL_GPIO_PIN_18, DL_GPIO_PIN_6};
static GPIO_Regs *const LP[3] = {GPIOC, GPIOC, GPIOB}; static const uint32_t LPIN[3] = {DL_GPIO_PIN_29, DL_GPIO_PIN_19, DL_GPIO_PIN_7};
static const uint32_t HPCM[3] = {IOMUX_PINCM70, IOMUX_PINCM71, IOMUX_PINCM23};
static const uint32_t LPCM[3] = {IOMUX_PINCM94, IOMUX_PINCM72, IOMUX_PINCM24};
static const DL_TIMER_CC_INDEX CCI[3] = {DL_TIMER_CC_1_INDEX, DL_TIMER_CC_3_INDEX, DL_TIMER_CC_0_INDEX};
static uint32_t g_period;                 /* timer load value, read back at boot */

volatile struct {
    uint32_t magic, done, period, zero[3], vm0;
    /* TEST_PADS: duty set on A, B, C; samples high of 4,000 on INH and INL of each; zero-event spacing; INH at zero */
    uint32_t p_cc[3], p_hi[3][2], p_zero_cyc_min, p_zero_cyc_max, p_inh_at_zero[3];
    /* TEST_ADC: fixed Vd at angle 0 for 60 periods; phase currents (A, into the motor) from the timer-triggered ADC */
    float a_vd, a_ia[60], a_ib[60], a_ic[60]; uint32_t a_sw_triggers, a_fault, a_peak_mA;
    /* TEST_CAL: field-angle sweep, one mechanical turn forward then back; every accepted Hall edge: state (core bits
     * A = 1, B = 2, C = 4), field angle (turn32), sweep direction */
    uint32_t c_n, c_state[64], c_ang[64], c_dir[64], c_peak_mA, c_stop;
    /* TEST_FOC: the table from the sweep (boundaries in forward order 3, 2, 6, 4, 5, 1), then the current loop */
    uint32_t t_bound[6], t_ok;
    float f_iq_ref, f_vmax; uint32_t f_stop, f_isr_n, f_loop_max, f_loop_mean;
    float f_hz[6], f_id[6], f_iq[6], f_vd[6], f_vq[6]; uint32_t f_peak_mA[6];
    /* TEST_SPEED: speed loop over the current loop; four steps of 1 s; speed (Hz el) and Iq every 0.5 ms for the first
     * 0.2 s of each step; per step the mean and spread of the last 0.5 s */
    float s_ref[4], s_trace[4][400], s_iqtr[4][400], s_mean[4], s_sd[4], s_iq[4]; uint32_t s_stop, s_loop_mean, s_loop_max, s_peak_mA;
} fc;

static inline uint32_t cyc(void) { return DWT->CYCCNT; }
static void wait_cyc(uint32_t n) { uint32_t t = cyc(); while (cyc() - t < n) {} }
static void delay_ms(uint32_t ms) { while (ms--) wait_cyc(CPU_HZ / 1000u); }
static void drv_off(int off) { if (off) DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28); else DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_28); }
static uint32_t nfault(void) { return DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_30) ? 1u : 0u; }
static void in_pin(uint32_t pcm) { DL_GPIO_initDigitalInputFeatures(pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE); }
static void tick(void)
{
    while (!(DL_TimerA_getRawInterruptStatus(PWM_0_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT))) {}
    DL_TimerA_clearInterruptStatus(PWM_0_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT);
}
static void set_cc(int p, uint32_t cc) { DL_TimerA_setCaptureCompareValue(PWM_0_INST, cc, CCI[p]); }
/* duty 0..1 of the high side -> compare: the high side is on for (1 - compare / load) of the period (pad test) */
static void set_duty(int p, float d) { if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f; set_cc(p, (uint32_t)((1.0f - d) * (float)g_period)); }
/* phase currents into the motor, amps: the sense reads current into each low-side switch, so the motor current is minus
 * that (pulse test, rung 4: a vector A+ B- reads positive on B). Sampled at the zero event, all low sides on. */
static void currents(float *i)
{
    while (DL_HSADC_isBusy(HSADC0_INST)) {}
    for (int k = 0; k < 3; k++) i[k] = -((float)DL_HSADC_getResult(HSADC0_RESULT_INST, (DL_HSADC_SOC_NUMBER)k) - (float)fc.zero[k]) / CNT_PER_A;
}
static float bus(void)
{
    DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
    wait_cyc(64u);                                     /* busy is not set at once after a software trigger: read 0 V without this */
    while (DL_HSADC_isBusy(HSADC1_INST)) {}
    return (float)DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER0) * (3.3f / 4096.0f) * (149.04f / 6.04f);
}
/* core Hall bits: A = 1, B = 2, C = 4 (portability/motor/core/angle_hall.h) */
static uint32_t hall_core(void)
{
    return (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_10) ? 1u : 0u) | (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_11) ? 2u : 0u) |
           (DL_GPIO_readPins(GPIOC, DL_GPIO_PIN_21) ? 4u : 0u);
}
/* apply a voltage vector of amplitude v (volts, phase peak) at electrical angle th (turn32) */
static void apply_v(float v, uint32_t th, float inv_vbus)
{
    frl_real sn, cs, da, db, dc;
    FRL_SINCOS(FRL_ANG_FROM_T32(th), &sn, &cs);
    frl_modulate(FRL_R(v) * cs, FRL_R(v) * sn, FRL_R(inv_vbus), &da, &db, &dc);
    set_duty(0, da); set_duty(1, db); set_duty(2, dc);
}
static void outputs_off(void) { drv_off(1); for (int k = 0; k < 3; k++) set_duty(k, 0.0f); }

static volatile uint32_t g_trip = 0, g_isr_n = 0;
void GPIOB_IRQHandler(void)
{
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28);           /* nFAULT fell: outputs off at once */
    g_isr_n++; g_trip = 1;
    DL_GPIO_clearInterruptStatus(GPIOB, DL_GPIO_PIN_30);
}
static void fault_irq_on(void)
{
    DL_GPIO_setUpperPinsPolarity(GPIOB, DL_GPIO_PIN_30_EDGE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOB, DL_GPIO_PIN_30);
    DL_GPIO_enableInterrupt(GPIOB, DL_GPIO_PIN_30);
    NVIC_SetPriority(GPIOB_INT_IRQn, 0); NVIC_ClearPendingIRQ(GPIOB_INT_IRQn); NVIC_EnableIRQ(GPIOB_INT_IRQn);
}

/* Hall calibration: the field angle swept one mechanical turn forward then back at 2 Hz electrical, 0.5 V; each
 * boundary between states is the circular mean of where it was crossed going up and going down (the rotor's lag and
 * the sensors' hysteresis cancel). Fills tab (enter forward / backward) and fc.t_bound; returns 0 on success. */
static int hall_cal(frl_hall_tab_t *tab)
{
    float vb = bus(), inv = 1.0f / vb, i[3], pk = 0.0f; uint32_t th = 0, st = hall_core(), cand = st, cn = 0, stop = 0;
    const uint32_t dth = (uint32_t)(4294967296.0 * 2.0 / 20000.0), nsweep = 4u * 10000u;
    double sx[6] = {0}, sy[6] = {0}; uint32_t cnt[6] = {0};
    set_duty(0, 0.5f); set_duty(1, 0.5f); set_duty(2, 0.5f); tick(); drv_off(0);
    for (uint32_t n = 0; n < 10000u + nsweep + 6000u + nsweep + 2000u && !stop; n++) {
        int dir = 0;
        if (n >= 10000u && n < 10000u + nsweep) { th += dth; dir = 1; }
        else if (n >= 16000u + nsweep && n < 16000u + 2u * nsweep) { th -= dth; dir = -1; }
        tick(); apply_v(0.5f, th, inv);
        wait_cyc(300u); currents(i);
        for (int k = 0; k < 3; k++) { float x = i[k] < 0 ? -i[k] : i[k]; if (x > pk) pk = x; }
        if (pk > TRIP_A) stop = 1; else if (!nfault() || g_trip) stop = 2;
        uint32_t h = hall_core();
        if (h != st) { if (h == cand) cn++; else { cand = h; cn = 1; } } else cn = 0;
        if (cn >= 3) {
            int a = frl_hall_idx[st], b = frl_hall_idx[cand];
            if (dir != 0 && a >= 0 && b >= 0) {
                int step = (b - a + 6) % 6, k = -1;
                if (dir > 0 && step == 1) k = b;            /* boundary index = position of the state entered going forward */
                if (dir < 0 && step == 5) k = a;            /* going back across the same boundary: the state left */
                if (k >= 0) { double ang = (double)th / 4294967296.0 * 6.283185307179586;
                              sx[k] += __builtin_cos(ang); sy[k] += __builtin_sin(ang); cnt[k]++; }
            }
            st = cand; cn = 0;
        }
    }
    outputs_off(); fc.c_peak_mA = (uint32_t)(pk * 1000.0f); fc.c_stop = stop;
    if (stop) return 1;
    static const uint8_t ORD[6] = {3, 2, 6, 4, 5, 1};
    for (int k = 0; k < 6; k++) {
        if (cnt[k] < 4u) return 2;
        double a = __builtin_atan2(sy[k], sx[k]); if (a < 0) a += 6.283185307179586;
        fc.t_bound[k] = (uint32_t)(a / 6.283185307179586 * 4294967296.0);
    }
    for (int k = 0; k < 6; k++) {                     /* state ORD[k]: entered forward at its boundary k, backward at k+1 */
        tab->fwd[ORD[k]] = fc.t_bound[k];
        tab->rev[ORD[k]] = fc.t_bound[(k + 1) % 6];
    }
    tab->fwd[0] = tab->rev[0] = tab->fwd[7] = tab->rev[7] = 0;
    return 0;
}

int main(void)
{
    SYSCFG_DL_init();                                  /* first: it resets GPIOA, B and C; PWM pins are timer outputs now */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_GPIO_initDigitalOutput(IOMUX_PINCM65); DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_28);  /* DRVOFF high */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM30); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_13); /* nSLEEP low */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM76); DL_GPIO_setPins(GPIOC, DL_GPIO_PIN_2); DL_GPIO_enableOutput(GPIOC, DL_GPIO_PIN_2);   /* ILIMIT high: off */
    in_pin(IOMUX_PINCM67); in_pin(IOMUX_PINCM21); in_pin(IOMUX_PINCM22); in_pin(IOMUX_PINCM80);
    g_period = DL_TimerA_getLoadValue(PWM_0_INST); fc.period = g_period;
    fc.magic = 0xF0C00001u;

    /* wake the driver (outputs still off), start the timer at 0 % on every phase (all low sides on once enabled),
     * take the current zeros from the timer-triggered conversions */
    for (int k = 0; k < 3; k++) set_duty(k, 0.0f);
    DL_TimerA_startCounter(PWM_0_INST);
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); delay_ms(10);
    { uint32_t s3[3] = {0, 0, 0};
      for (int n = 0; n < 64; n++) { tick(); while (DL_HSADC_isBusy(HSADC0_INST)) {} wait_cyc(400u);
          for (int k = 0; k < 3; k++) s3[k] += DL_HSADC_getResult(HSADC0_RESULT_INST, (DL_HSADC_SOC_NUMBER)k); }
      for (int k = 0; k < 3; k++) fc.zero[k] = s3[k] / 64u; }
    fc.vm0 = (uint32_t)(bus() * 1000.0f);
    if (!nfault() || fc.vm0 < 15000u || fc.vm0 > 21000u) goto end;

#ifdef TEST_SPEED
    {   static frl_hall_tab_t tab; frl_hall_t hs;
        fault_irq_on();
        if (hall_cal(&tab)) goto end;
        delay_ms(1000);
        DL_TimerA_setCaptCompUpdateMethod(PWM_0_INST, DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
        DL_TimerA_setCaptCompUpdateMethod(PWM_0_INST, DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
        DL_TimerA_setCaptCompUpdateMethod(PWM_0_INST, DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT, DL_TIMERA_CAPTURE_COMPARE_3_INDEX);
        frl_hall_init(&hs, &tab, 0u, 2000u);
        for (int n = 0; n < 8; n++) frl_hall_step(&hs, hall_core());
        const float VMAX = 3.0f, TS = 1.0f / 20000.0f, KP = 0.84e-3f * 6283.0f, KI = 0.976f * 6283.0f * TS;
        /* speed: plant 4 x 0.0284 / (2 pi 2.58e-6) = 7,010 Hz/s per A; 15 Hz crossover; integral zero at 3.75 Hz; 2 kHz */
        const float SKP = 6.2832f * 15.0f / 7010.0f, SKI = SKP * 6.2832f * 3.75f * (10.0f * TS), IQMAX = 0.4f;
        static const float REF[4] = {20.0f, 40.0f, 30.0f, 10.0f};   /* Hz electrical: 300, 600, 450, 150 rpm */
        frl_pi_t pid = {FRL_R(KP), FRL_R(KI), 0, FRL_R(-VMAX), FRL_R(VMAX)}, piq = pid;
        frl_pi_t pis = {FRL_R(SKP), FRL_R(SKI), 0, FRL_R(-IQMAX), FRL_R(IQMAX)};
        float vb = bus(), inv = 1.0f / vb, i[3], iqref = 0.0f, pk = 0.0f; uint32_t stop = 0, lmax = 0; uint64_t lsum = 0, ln = 0;
        set_duty(0, 0.5f); set_duty(1, 0.5f); set_duty(2, 0.5f); tick(); drv_off(0);
        for (int st = 0; st < 4 && !stop; st++) {
            fc.s_ref[st] = REF[st]; double sm = 0, sq = 0, siq = 0; uint32_t m = 0;
            for (uint32_t n = 0; n < 20000u; n++) {
                tick(); uint32_t t0 = cyc();
                if (g_trip) { stop = 2; break; }
                wait_cyc(240u); currents(i);
                frl_hall_step(&hs, hall_core());
                float hz = (float)frl_hall_speed(&hs) * (20000.0f / 4294967296.0f);
                if ((n % 10u) == 0) {
                    iqref = frl_pi(&pis, FRL_R(REF[st] - hz));
                    if (n < 4000u) { fc.s_trace[st][n / 10u] = hz; fc.s_iqtr[st][n / 10u] = iqref; }
                }
                frl_real sn, cs, al, be, id, iq, va, vbe, da, db, dc, vd, vq;
                FRL_SINCOS(FRL_ANG_FROM_T32(frl_hall_angle(&hs)), &sn, &cs);
                frl_clarke(FRL_R(i[0]), FRL_R(i[1]), &al, &be);
                frl_park(al, be, sn, cs, &id, &iq);
                vd = frl_pi(&pid, FRL_R(0.0f) - id);
                piq.hi = frl_vq_limit(FRL_R(VMAX), vd); piq.lo = -piq.hi;
                vq = frl_pi(&piq, FRL_R(iqref) - iq);
                frl_ipark(vd, vq, sn, cs, &va, &vbe);
                frl_modulate(va, vbe, FRL_R(inv), &da, &db, &dc);
                set_duty(0, da); set_duty(1, db); set_duty(2, dc);
                for (int k = 0; k < 3; k++) { float x = i[k] < 0 ? -i[k] : i[k]; if (x > pk) pk = x; }
                if (pk > TRIP_A) { stop = 1; break; }
                if (n >= 10000u) { sm += hz; sq += (double)hz * hz; siq += iq; m++; }
                if ((n & 1023u) == 0) { float vbn = bus(); if (vbn < 15.0f || vbn > 21.0f) { stop = 3; break; } inv = 1.0f / vbn; }
                uint32_t c = cyc() - t0; if (c > lmax) lmax = c; lsum += c; ln++;
            }
            if (m) { double mu = sm / m; fc.s_mean[st] = (float)mu; fc.s_sd[st] = (float)__builtin_sqrt(sq / m - mu * mu); fc.s_iq[st] = (float)(siq / m); }
        }
        outputs_off(); fc.s_stop = stop; fc.s_loop_max = lmax; fc.s_loop_mean = ln ? (uint32_t)(lsum / ln) : 0; fc.s_peak_mA = (uint32_t)(pk * 1000.0f);
        goto end;
    }
#endif
#ifdef TEST_FOC
    {   static frl_hall_tab_t tab; frl_hall_t hs;
        fault_irq_on();
        int r = hall_cal(&tab); fc.t_ok = (uint32_t)(r == 0);
        if (r) goto end;
        delay_ms(1000);                                 /* the rotor settles */
        for (int k = 0; k < 3; k++) DL_TimerA_setCaptCompUpdateMethod(PWM_0_INST, DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT, CCI[k] == DL_TIMER_CC_0_INDEX ? DL_TIMERA_CAPTURE_COMPARE_0_INDEX : (CCI[k] == DL_TIMER_CC_1_INDEX ? DL_TIMERA_CAPTURE_COMPARE_1_INDEX : DL_TIMERA_CAPTURE_COMPARE_3_INDEX));
        frl_hall_init(&hs, &tab, 0u, 2000u);
        for (int n = 0; n < 8; n++) frl_hall_step(&hs, hall_core());
        const float IQ = 0.12f, VMAX = 1.5f, TS = 1.0f / 20000.0f, KP = 0.84e-3f * 6283.0f, KI = 0.976f * 6283.0f * TS;
        fc.f_iq_ref = IQ; fc.f_vmax = VMAX;
        frl_pi_t pid = {FRL_R(KP), FRL_R(KI), 0, FRL_R(-VMAX), FRL_R(VMAX)}, piq = pid;
        float vb = bus(), inv = 1.0f / vb, i[3], vd = 0, vq = 0; uint32_t stop = 0, lmax = 0; uint64_t lsum = 0;
        set_duty(0, 0.5f); set_duty(1, 0.5f); set_duty(2, 0.5f); tick(); drv_off(0);
        for (int w = 0; w < 6 && !stop; w++) {
            double sid = 0, siq = 0, svd = 0, svq = 0, shz = 0; float pk = 0; uint32_t m = 0;
            for (uint32_t n = 0; n < 10000u; n++) {      /* 0.5 s windows, 3 s in all */
                tick(); uint32_t t0 = cyc();
                if (g_trip) { stop = 2; break; }
                wait_cyc(240u); currents(i);
                frl_hall_step(&hs, hall_core());
                frl_real sn, cs, al, be, id, iq, va, vbe, da, db, dc;
                FRL_SINCOS(FRL_ANG_FROM_T32(frl_hall_angle(&hs)), &sn, &cs);
                frl_clarke(FRL_R(i[0]), FRL_R(i[1]), &al, &be);
                frl_park(al, be, sn, cs, &id, &iq);
                vd = frl_pi(&pid, FRL_R(0.0f) - id);
                piq.hi = frl_vq_limit(FRL_R(VMAX), vd); piq.lo = -piq.hi;
                vq = frl_pi(&piq, FRL_R(IQ) - iq);
                frl_ipark(vd, vq, sn, cs, &va, &vbe);
                frl_modulate(va, vbe, FRL_R(inv), &da, &db, &dc);
                set_duty(0, da); set_duty(1, db); set_duty(2, dc);
                for (int k = 0; k < 3; k++) { float x = i[k] < 0 ? -i[k] : i[k]; if (x > pk) pk = x; }
                if (pk > TRIP_A) { stop = 1; break; }
                sid += id; siq += iq; svd += vd; svq += vq; shz += (double)frl_hall_speed(&hs) * (20000.0 / 4294967296.0); m++;
                if ((n & 1023u) == 0) { float vbn = bus(); if (vbn < 15.0f || vbn > 21.0f) { stop = 3; break; } inv = 1.0f / vbn; }
                uint32_t c = cyc() - t0; if (c > lmax) lmax = c; lsum += c;
            }
            if (m) { fc.f_id[w] = (float)(sid / m); fc.f_iq[w] = (float)(siq / m); fc.f_vd[w] = (float)(svd / m); fc.f_vq[w] = (float)(svq / m); fc.f_hz[w] = (float)(shz / m); }
            fc.f_peak_mA[w] = (uint32_t)(pk * 1000.0f);
        }
        outputs_off(); fc.f_stop = stop; fc.f_isr_n = g_isr_n; fc.f_loop_max = lmax; fc.f_loop_mean = (uint32_t)(lsum / 60000u);
        goto end;
    }
#endif
#ifdef TEST_CAL
    {   /* align 0.5 s at angle 0, then the field angle up one mechanical turn (4 electrical) at 2 Hz electrical, hold
         * 0.3 s, then back down the same; 0.5 V phase peak (about 0.4 A); Hall state accepted after 3 agreeing periods */
        float vb = bus(), inv = 1.0f / vb, i[3], pk = 0.0f; uint32_t th = 0, st = hall_core(), cand = st, cn = 0, stop = 0;
        const float V = 0.5f;
        const uint32_t dth = (uint32_t)(4294967296.0 * 2.0 / 20000.0);   /* 2 Hz electrical per 20 kHz period */
        const uint32_t nsweep = 4u * 10000u;                              /* 4 electrical turns at 2 Hz: 2 s */
        set_duty(0, 0.5f); set_duty(1, 0.5f); set_duty(2, 0.5f); tick(); drv_off(0);
        for (uint32_t n = 0; n < 10000u + nsweep + 6000u + nsweep + 2000u && !stop; n++) {
            int dir = 0;
            if (n >= 10000u && n < 10000u + nsweep) { th += dth; dir = 1; }
            else if (n >= 16000u + nsweep && n < 16000u + 2u * nsweep) { th -= dth; dir = -1; }
            tick(); apply_v(V, th, inv);
            wait_cyc(300u); currents(i);
            for (int k = 0; k < 3; k++) { float x = i[k] < 0 ? -i[k] : i[k]; if (x > pk) pk = x; }
            if (pk > TRIP_A) stop = 1; else if (!nfault()) stop = 2;
            uint32_t h = hall_core();
            if (h != st) { if (h == cand) cn++; else { cand = h; cn = 1; } } else cn = 0;
            if (cn >= 3) { st = cand; cn = 0;
                if (fc.c_n < 64u) { fc.c_state[fc.c_n] = st; fc.c_ang[fc.c_n] = th; fc.c_dir[fc.c_n] = (uint32_t)(dir + 1); fc.c_n++; } }
        }
        outputs_off(); fc.c_peak_mA = (uint32_t)(pk * 1000.0f); fc.c_stop = stop;
        goto end;
    }
#endif
#ifdef TEST_ADC
    {   /* Vd 0.4 V at angle 0 (alpha = 0.4, beta = 0): modulated about 50 % on all three phases, centred by min-max */
        float vb = bus(), da, db, dc, i[3]; frl_real inv = FRL_R(1.0f / vb);
        fc.a_vd = 0.4f;
        frl_modulate(FRL_R(0.4f), FRL_R(0.0f), inv, &da, &db, &dc);
        set_duty(0, 0.5f); set_duty(1, 0.5f); set_duty(2, 0.5f);
        tick(); drv_off(0);
        set_duty(0, da); set_duty(1, db); set_duty(2, dc);
        float pk = 0.0f;
        for (int n = 0; n < 60; n++) {
            tick(); wait_cyc(300u); currents(i);
            fc.a_ia[n] = i[0]; fc.a_ib[n] = i[1]; fc.a_ic[n] = i[2];
            for (int k = 0; k < 3; k++) { float x = i[k] < 0 ? -i[k] : i[k]; if (x > pk) pk = x; }
            if (pk > TRIP_A || !nfault()) break;
        }
        outputs_off(); fc.a_peak_mA = (uint32_t)(pk * 1000.0f); fc.a_fault = nfault() ? 0u : 1u;
        goto end;
    }
#endif
#ifdef TEST_PADS
    {   /* outputs stay disabled (DRVOFF high). A, B, C at compare 1/4, 1/2, 3/4 of the load value */
        uint32_t cc[3] = {g_period / 4u, g_period / 2u, (3u * g_period) / 4u};
        for (int k = 0; k < 3; k++) { set_cc(k, cc[k]); fc.p_cc[k] = cc[k];
            IOMUX->SECCFG.PINCM[HPCM[k]] |= IOMUX_PINCM_INENA_ENABLE; IOMUX->SECCFG.PINCM[LPCM[k]] |= IOMUX_PINCM_INENA_ENABLE; }
        tick(); tick();
        for (int n = 0; n < 4000; n++) {
            for (int k = 0; k < 3; k++) { fc.p_hi[k][0] += DL_GPIO_readPins(HP[k], HPIN[k]) ? 1u : 0u; fc.p_hi[k][1] += DL_GPIO_readPins(LP[k], LPIN[k]) ? 1u : 0u; }
            wait_cyc(197u);
        }
        uint32_t mn = 0xFFFFFFFFu, mx = 0; tick(); uint32_t t0 = cyc();
        for (int n = 0; n < 50; n++) { tick(); uint32_t t = cyc(); if (t - t0 < mn) mn = t - t0; if (t - t0 > mx) mx = t - t0; t0 = t; }
        fc.p_zero_cyc_min = mn; fc.p_zero_cyc_max = mx;
        tick(); for (int k = 0; k < 3; k++) fc.p_inh_at_zero[k] = DL_GPIO_readPins(HP[k], HPIN[k]) ? 1u : 0u;
        goto end;
    }
#endif
end:
    drv_off(1); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13);
    fc.done = 0x600Du;
    while (1) { __WFI(); }
}
