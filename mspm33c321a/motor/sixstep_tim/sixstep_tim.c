/*
 * Six-step Hall spin with the PWM from the TIMA0_1 timer, LP-MSPM33C321A + DRV8376EVM + BLY171S (Fox Red Labs,
 * 2026-10-20; rung 8 of portability/motor/PORTING.md). The same script as ../sixstep (software PWM, rung 7) so the two
 * compare run for run. Timer: 20 kHz edge-aligned, complementary outputs with a 125 ns dead band. Per phase, one of three
 * states: PWM (INH chops, INL its complement: the low side carries the freewheel current), LOW (INH low, INL high,
 * as GPIO), OFF (both pins GPIO low). The loop runs once per timer zero event.
 *
 * 1. Wake the driver with the outputs off, take the three current-sense zeros.
 * 2. Table search: twelve Hall-to-vector tables (with or against the Hall order, six offsets), each driven at DUTY_PCT
 *    for 0.4 s with 0.3 s off between (on time ON_CYC cycles per period); net Hall sectors travelled are the score (the AM13 program's learn, step 2).
 * 3. Run the best table forward for 3 s at the same on time, Hall sectors counted per 0.5 s; then off, driver asleep.
 *
 * Stops, every PWM period: any phase current over TRIP_A (software, from the sense outputs), nFAULT low, the bus
 * outside 15 to 21 V, no Hall edge for 1 s while driving. ILIMIT reaches the M33 only on PC2 (J3 pin 30), which has
 * no DAC; left floating it reads near 0 V and the driver faults on the first low-side current (2026-10-20, two runs).
 * PC2 is driven to 3.3 V, above VREF (3.0 V): the driver's cycle-by-cycle limit is off, as on the AM13's first FOC
 * runs. Backstops: the 6 % on time (about 0.7 A at stall), the software trip, the driver's own over-current shutdown,
 * the pack's 5 A fuse.
 * Pins: BoosterPack site 1 (results/drvcheck-20261020.md). Call SYSCFG_DL_init before any pin setup (it resets GPIO).
 */
#include "ti_msp_dl_config.h"

#ifndef CPU_HZ
#define CPU_HZ   32000000u             /* 160000000u with the 160 MHz SysConfig (build.sh CLK160=1) */
#endif
#define PERIOD   (PWM_0_INST_CLK_FREQ / 20000u)   /* timer counts per 20 kHz period: 1,600 at 32 MHz */
#define PCT(x)   ((PERIOD * (x)) / 100u)  /* on time in timer counts for x percent */
#ifndef ON_CYC
#define ON_CYC   PCT(6u)               /* 6 %: 18.1 V x 0.06 over 2 x 0.78 ohm, about 0.7 A at stall */
#endif
#define TRIP_A   0.8f
#define CNT_PER_A (4096.0f / 3.3f)     /* 1 V/A sense gain on the 3.3 V ADC */

/* line vectors 0..5, 60 degrees apart: high side, low side */
static const uint8_t VHI[6] = {0, 0, 1, 1, 2, 2};
static const uint8_t VLO[6] = {1, 2, 2, 0, 0, 1};
/* phases 0 A, 1 B, 2 C: timer channel and pins (board profile portability/motor/boards/mspm33c321a/board.json) */
static GPIO_Regs *const HP[3] = {GPIOC, GPIOC, GPIOB}; static const uint32_t HPIN[3] = {DL_GPIO_PIN_17, DL_GPIO_PIN_18, DL_GPIO_PIN_6};
static GPIO_Regs *const LP[3] = {GPIOC, GPIOC, GPIOB}; static const uint32_t LPIN[3] = {DL_GPIO_PIN_29, DL_GPIO_PIN_19, DL_GPIO_PIN_7};
static const uint32_t HPCM[3] = {IOMUX_PINCM70, IOMUX_PINCM71, IOMUX_PINCM23};
static const uint32_t LPCM[3] = {IOMUX_PINCM94, IOMUX_PINCM72, IOMUX_PINCM24};
static const uint32_t HFN[3] = {GPIO_PWM_0_C1_IOMUX_FUNC, GPIO_PWM_0_C3_IOMUX_FUNC, GPIO_PWM_0_C0_IOMUX_FUNC};
static const uint32_t LFN[3] = {GPIO_PWM_0_C1_CMPL_IOMUX_FUNC, GPIO_PWM_0_C3_CMPL_IOMUX_FUNC, GPIO_PWM_0_C0_CMPL_IOMUX_FUNC};
static const DL_TIMER_CC_INDEX CCI[3] = {DL_TIMER_CC_1_INDEX, DL_TIMER_CC_3_INDEX, DL_TIMER_CC_0_INDEX};
enum { PH_OFF = 0, PH_LOW = 1, PH_PWM = 2 };
static uint8_t phmode[3] = {9, 9, 9};
static void phase(int p, int mode, uint32_t on)
{
    /* OFF: both pins GPIO low. LOW: GPIO, INH low then INL high (low side on). PWM: the timer, INH high for `on` counts
     * of the down-counting period (compare = period - on), INL its complement with the dead band. A compare equal to
     * the period does not give 0 %: it holds INH high all period (rung 8 pad test, 2026-10-20), so LOW is never a
     * timer state. */
    if (mode == PH_PWM) {
        DL_TimerA_setCaptureCompareValue(PWM_0_INST, PERIOD - on, CCI[p]);
        if (phmode[p] != PH_PWM) {
            DL_GPIO_initPeripheralOutputFunction(LPCM[p], LFN[p]); DL_GPIO_initPeripheralOutputFunction(HPCM[p], HFN[p]);
        }
    } else if (phmode[p] != mode) {
        DL_GPIO_clearPins(HP[p], HPIN[p]); DL_GPIO_initDigitalOutput(HPCM[p]); DL_GPIO_enableOutput(HP[p], HPIN[p]);
        if (mode == PH_LOW) DL_GPIO_setPins(LP[p], LPIN[p]); else DL_GPIO_clearPins(LP[p], LPIN[p]);
        DL_GPIO_initDigitalOutput(LPCM[p]); DL_GPIO_enableOutput(LP[p], LPIN[p]);
    }
    phmode[p] = (uint8_t)mode;
}
static void all_off(void) { for (int k = 0; k < 3; k++) phase(k, PH_OFF, 0); }
/* wait for the next timer zero event (the start of a period) */
static void tick(void)
{
    while (!(DL_TimerA_getRawInterruptStatus(PWM_0_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT))) {}
    DL_TimerA_clearInterruptStatus(PWM_0_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT);
}
static void vector(int v, uint32_t on)
{
    for (int k = 0; k < 3; k++) if (k != VHI[v] && k != VLO[v]) phase(k, PH_OFF, 0);
    phase(VLO[v], PH_LOW, 0); phase(VHI[v], PH_PWM, on);
}

static const uint8_t SEQ[6] = {1, 5, 4, 6, 2, 3};   /* Hall order (A = 4, B = 2, C = 1), the AM13 bench's, unchanged wiring */

enum { STOP_NONE = 0, STOP_TRIP = 1, STOP_FAULT = 2, STOP_BUS = 3, STOP_STALL = 4, STOP_HALL = 5 };

volatile struct {
    uint32_t magic, zero[3], vm0;
    int32_t  net[12]; uint32_t peak_mA[12], stop[12];
    uint32_t best, best_dir;
    uint32_t run_sectors[6]; int32_t run_net; uint32_t run_peak_mA, run_stop, run_vm, run_cycles;
    uint32_t done;
    uint32_t pulse_i[6][3], pulse_v[6][3];   /* TEST_PULSE (software PWM build only) */
    uint32_t adc_wait_max, adc_cyc;          /* cycles waited for the ADC after a period; one blocking conversion pair */
    uint16_t burst[2][40];                   /* drive-loop debug: current and timing of the first 40 periods */
    uint32_t dbg_n;
    uint16_t burst6[6][40];                  /* TEST_BURST6: each vector at ON_CYC for 40 periods, its low phase's sense */
    uint32_t pads[6][4];                     /* TEST_PADS: per vector, samples high of 4,000 on INH hi, INL hi, INH lo, INL lo */
    /* TEST_RUNS: fault-interrupt latency and the speed sweep */
    uint32_t f_best, f_isr_count, f_latency_cyc, f_drvoff_after, f_stop, f_nfault_after_wake;
    uint32_t sw_on[6], sw_sectors[6], sw_ms[6], sw_mean_mA[6], sw_peak_mA[6], sw_stop;
    /* TEST_LAT: a real driver fault (ILIMIT at 0 V, so its cycle-by-cycle limit trips on the first pulse) */
    uint32_t l_isr_n, l_last_high_to_isr, l_seen_low_after_isr, l_periods, l_peak_mA, l_nfault_after_clear, l_trials_ok;
    uint32_t l_lat[5];
    /* TEST_CHAR: start from standstill, loop cost, coast capture */
    uint32_t c_best, c_start_edges[64], c_n_start, c_loop_max, c_loop_sum, c_loop_n, c_coast_n, c_coast_dt_cyc;
    /* TEST_TABLES: each of the 12 tables at 6 % for 1.5 s from rest; sectors in the last second, mean current, stop */
    uint32_t t_sec[12], t_mean_mA[12], t_peak_mA[12], t_stop[12]; int32_t t_net[12];   /* net: forward minus backward sectors in the last second */
} ss;
#define COAST_N 7000u
volatile struct { uint16_t va[COAST_N], vb[COAST_N], vc[COAST_N]; uint8_t h[COAST_N]; } cap;

static inline uint32_t cyc(void) { return DWT->CYCCNT; }
static void wait_cyc(uint32_t n) { uint32_t t = cyc(); while (cyc() - t < n) {} }
static void delay_ms(uint32_t ms) { while (ms--) wait_cyc(CPU_HZ / 1000u); }

static void out_low(uint32_t pcm, GPIO_Regs *p, uint32_t pin) { DL_GPIO_initDigitalOutput(pcm); DL_GPIO_clearPins(p, pin); DL_GPIO_enableOutput(p, pin); }
static void drv_off(int off) { if (off) DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28); else DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_28); }
static uint32_t nfault(void) { return DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_30) ? 1u : 0u; }
static uint32_t hall(void)
{
    return (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_10) ? 4u : 0u) | (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_11) ? 2u : 0u) |
           (DL_GPIO_readPins(GPIOC, DL_GPIO_PIN_21) ? 1u : 0u);
}
static int pos(uint32_t h) { for (int i = 0; i < 6; i++) if (SEQ[i] == h) return i; return -1; }

static void adc(uint32_t *i3, uint32_t *vm)
{
    DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1);
    DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
    while (DL_HSADC_isBusy(HSADC0_INST) || DL_HSADC_isBusy(HSADC1_INST)) {}
    for (int k = 0; k < 3; k++) i3[k] = DL_HSADC_getResult(HSADC0_RESULT_INST, (DL_HSADC_SOC_NUMBER)k);
    *vm = DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER0);
}
static float vm_volts(uint32_t c) { return (float)c * (3.3f / 4096.0f) * (149.04f / 6.04f); }

/* Drive with table tab[hall] = vector for `ms`, low side of the vector's second phase on, high side chopped for ON_CYC cycles.
 * Returns the stop reason; counts net sectors along SEQ and the peak current. */
static uint32_t drive(const uint8_t *tab, uint32_t ms, int32_t *net, uint32_t *peak_mA, volatile uint32_t *win, uint32_t *vm_last, uint32_t on_cyc)
{
    uint32_t ts0 = 0, on = on_cyc, t0 = cyc(), tedge = t0, i3[3], vm, stop = STOP_NONE;
    uint32_t h = hall(), cand = h, candn = 0; int p0 = pos(h); float pk = 0.0f;
    if (p0 < 0) return STOP_HALL;
    drv_off(0);
    uint32_t periods = ms * 20u; uint8_t vlast = 0xFF; (void)ts0;
    for (uint32_t n = 0; n < periods; n++) {
        tick();
        uint32_t ts = cyc();
        uint8_t v = tab[h];
        if (v != vlast) { vector(v, on); vlast = v; }
        DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1);
        DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
        /* Hall: a new state counts after two periods in a row */
        uint32_t hn = hall();
        if (hn != h) { if (hn == cand) candn++; else { cand = hn; candn = 1; } } else candn = 0;
        if (candn >= 2) {
            int a0 = pos(h), b0 = pos(cand);
            if (b0 < 0) { stop = STOP_HALL; break; }
            int st = (b0 - a0 + 6) % 6;
            if (st == 1) (*net)++; else if (st == 5) (*net)--;
            h = cand; candn = 0; tedge = cyc();
            if (win) win[(n / 10000u) % 6]++;
        }
        if (cyc() - tedge > CPU_HZ) { stop = STOP_STALL; break; }
        uint32_t tw = cyc();
        while (DL_HSADC_isBusy(HSADC0_INST) || DL_HSADC_isBusy(HSADC1_INST)) {}
        tw = cyc() - tw; if (tw > ss.adc_wait_max) ss.adc_wait_max = tw;
        for (int k = 0; k < 3; k++) i3[k] = DL_HSADC_getResult(HSADC0_RESULT_INST, (DL_HSADC_SOC_NUMBER)k);
        vm = DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER0);
        float a = 0.0f;
        for (int k = 0; k < 3; k++) { float x = ((float)i3[k] - (float)ss.zero[k]) / CNT_PER_A; if (x < 0) x = -x; if (x > a) a = x; }
        if (a > pk) pk = a;
        if (ss.dbg_n < 40u) { ss.burst[0][ss.dbg_n] = (uint16_t)(a * 1000.0f); ss.burst[1][ss.dbg_n] = (uint16_t)(cyc() - ts); ss.dbg_n++; }
        if (a > TRIP_A) { stop = STOP_TRIP; break; }
        if (!nfault()) { stop = STOP_FAULT; break; }
        float vb = vm_volts(vm);
        if (vb < 15.0f || vb > 21.0f) { stop = STOP_BUS; break; }
        *vm_last = vm;
    }
    all_off(); drv_off(1);
    *peak_mA = (uint32_t)(pk * 1000.0f);
    return stop;
}

/* nFAULT (PB30) falling edge: outputs off at once (DRVOFF high), the loop then parks the phases. PB30 has no timer fault
 * function and no comparator input on this LaunchPad (device data), so this interrupt is the MCU's fastest stop. */
static volatile uint32_t g_trip = 0, g_isr_cyc = 0, g_isr_n = 0;
#ifdef ISR_RAM   /* the handler runs from SRAM (.TI.ramfunc, copied at boot); the vector itself is still read from flash */
__attribute__((section(".TI.ramfunc")))
#endif
void GPIOB_IRQHandler(void)
{
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28);
    g_isr_cyc = DWT->CYCCNT; g_isr_n++; g_trip = 1;
    DL_GPIO_clearInterruptStatus(GPIOB, DL_GPIO_PIN_30);
}
static void fault_irq_on(void)
{
    DL_GPIO_setUpperPinsPolarity(GPIOB, DL_GPIO_PIN_30_EDGE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOB, DL_GPIO_PIN_30);
    DL_GPIO_enableInterrupt(GPIOB, DL_GPIO_PIN_30);
    NVIC_SetPriority(GPIOB_INT_IRQn, 0); NVIC_ClearPendingIRQ(GPIOB_INT_IRQn); NVIC_EnableIRQ(GPIOB_INT_IRQn);
}

/* Run table tab through steps of (on counts, ms); at fault_ms (0: never) take nSLEEP low, which pulls nFAULT low, and
 * time the interrupt. Per step: Hall sectors, mean and peak of the largest phase current. */
static volatile uint32_t *g_edge_log = 0; static int32_t g_net = 0; static uint32_t g_edge_n = 0, g_t_start = 0, g_loop_max = 0, g_loop_sum = 0, g_loop_n = 0;
static uint32_t run_steps(const uint8_t *tab, const uint32_t *on, const uint32_t *ms, int nsteps, uint32_t fault_ms,
                          volatile uint32_t *sec, volatile uint32_t *mean_mA, volatile uint32_t *peak_mA, uint32_t *t_write)
{
    uint32_t i3[3], vm, stop = STOP_NONE, h = hall(), cand = h, candn = 0, tedge = cyc(), nall = 0;
    if (pos(h) < 0) return STOP_HALL;
    uint8_t vlast = 0xFF; g_trip = 0;
    drv_off(0); g_t_start = cyc();
    for (int st = 0; st < nsteps && stop == STOP_NONE; st++) {
        float sum = 0.0f, pk = 0.0f; uint32_t np = ms[st] * 20u, cnt = 0;
        vlast = 0xFF;
        for (uint32_t n = 0; n < np; n++, nall++) {
            tick(); uint32_t tk = cyc();
            if (g_trip) { stop = STOP_FAULT; break; }
            uint8_t v = tab[h];
            if (v != vlast) { vector(v, on[st]); vlast = v; }
            DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1);
            DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
            if (fault_ms && nall == fault_ms * 20u) { *t_write = cyc(); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); }
            uint32_t hn = hall();
            if (hn != h) { if (hn == cand) candn++; else { cand = hn; candn = 1; } } else candn = 0;
            if (candn >= 2) { if (pos(cand) < 0) { stop = STOP_HALL; break; }
                              { int d = (pos(cand) - pos(h) + 6) % 6; if (d == 1) g_net++; else if (d == 5) g_net--; }
                              h = cand; candn = 0; tedge = cyc(); sec[st]++;
                              if (g_edge_log && g_edge_n < 64u) g_edge_log[g_edge_n++] = tedge - g_t_start; }
            if (cyc() - tedge > CPU_HZ) { stop = STOP_STALL; break; }
            while (DL_HSADC_isBusy(HSADC0_INST) || DL_HSADC_isBusy(HSADC1_INST)) {}
            for (int k = 0; k < 3; k++) i3[k] = DL_HSADC_getResult(HSADC0_RESULT_INST, (DL_HSADC_SOC_NUMBER)k);
            vm = DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER0);
            float a = 0.0f;
            for (int k = 0; k < 3; k++) { float x = ((float)i3[k] - (float)ss.zero[k]) / CNT_PER_A; if (x < 0) x = -x; if (x > a) a = x; }
            sum += a; cnt++; if (a > pk) pk = a;
            if (a > TRIP_A) { stop = STOP_TRIP; break; }
            if (g_trip) { stop = STOP_FAULT; break; }
            float vb = vm_volts(vm);
            if (vb < 15.0f || vb > 21.0f) { stop = STOP_BUS; break; }
            { uint32_t c = cyc() - tk; if (c > g_loop_max) g_loop_max = c; g_loop_sum += c; g_loop_n++; }
        }
        if (cnt) mean_mA[st] = (uint32_t)(sum / (float)cnt * 1000.0f);
        peak_mA[st] = (uint32_t)(pk * 1000.0f);
    }
    all_off(); drv_off(1);
    return stop;
}

int main(void)
{
    SYSCFG_DL_init();                                  /* first: it resets GPIOA, B and C */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_GPIO_initDigitalOutput(IOMUX_PINCM65); DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_28);  /* DRVOFF high */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM30); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_13); /* nSLEEP low */
    all_off();                                          /* gate pins to GPIO low until a phase is used */
    DL_TimerA_startCounter(PWM_0_INST);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM76); DL_GPIO_setPins(GPIOC, DL_GPIO_PIN_2); DL_GPIO_enableOutput(GPIOC, DL_GPIO_PIN_2);   /* ILIMIT high: off */
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM67, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM21, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM22, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM80, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    ss.magic = 0x55E16008u;

    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); delay_ms(10);          /* wake, outputs off */
    { uint32_t i3[3], vm, s[3] = {0, 0, 0}; for (int n = 0; n < 64; n++) { adc(i3, &vm); for (int k = 0; k < 3; k++) s[k] += i3[k]; }
      for (int k = 0; k < 3; k++) ss.zero[k] = s[k] / 64u; ss.vm0 = vm;
      uint32_t t = cyc(); adc(i3, &vm); ss.adc_cyc = cyc() - t; }
    if (!nfault() || vm_volts(ss.vm0) < 15.0f || vm_volts(ss.vm0) > 21.0f) { ss.run_stop = 99; goto end; }

#ifdef TEST_TABLES
    {   uint8_t tb[12][8];
        fault_irq_on();
        for (int t = 0; t < 12; t++) {
            int off = t % 6, with = t < 6;
            for (int hh = 0; hh < 8; hh++) tb[t][hh] = 0;
            for (int i = 0; i < 6; i++) tb[t][SEQ[i]] = (uint8_t)(with ? (i + off) % 6 : (off - i + 12) % 6);
            volatile uint32_t sec[2] = {0, 0}, mn[2] = {0, 0}, pk[2] = {0, 0}; uint32_t tw = 0;
            g_net = 0;
            { static const uint32_t on0[1] = {PCT(6u)}, ms0[1] = {500u}; volatile uint32_t s0[1] = {0}, m0[1] = {0}, p0[1] = {0};
              ss.t_stop[t] = run_steps(tb[t], on0, ms0, 1, 0u, s0, m0, p0, &tw); ss.t_peak_mA[t] = p0[0]; }
            if (ss.t_stop[t] != STOP_NONE) { delay_ms(1500); continue; }
            g_net = 0;
            { static const uint32_t on1[1] = {PCT(6u)}, ms1[1] = {1000u};
              ss.t_stop[t] = run_steps(tb[t], on1, ms1, 1, 0u, sec + 1, mn + 1, pk + 1, &tw); }
            ss.t_net[t] = g_net;
            ss.t_sec[t] = sec[1]; ss.t_mean_mA[t] = mn[1]; if (pk[1] > ss.t_peak_mA[t]) ss.t_peak_mA[t] = pk[1];
            if (ss.t_stop[t] == STOP_TRIP || ss.t_stop[t] == STOP_FAULT || ss.t_stop[t] == STOP_BUS) break;
            delay_ms(1500);                                  /* coast to rest */
        }
        goto end;
    }
#endif
#ifdef TEST_CHAR
    {   uint8_t tb[12][8]; int32_t best = -1, bnet = 0;
        for (int t = 0; t < 12; t++) {
            int off = t % 6, with = t < 6;
            for (int hh = 0; hh < 8; hh++) tb[t][hh] = 0;
            for (int i = 0; i < 6; i++) tb[t][SEQ[i]] = (uint8_t)(with ? (i + off) % 6 : (off - i + 12) % 6);
            int32_t net = 0; uint32_t pk = 0, vml = 0;
            uint32_t r = drive(tb[t], 300u, &net, &pk, 0, &vml, PCT(4u));
            if (r == STOP_TRIP || r == STOP_FAULT || r == STOP_BUS) { ss.run_stop = 100 + r; goto end; }
            if (net > bnet) { bnet = net; best = t; }
            delay_ms(300);
        }
        if (best < 0 || bnet < 8) { ss.run_stop = 98; goto end; }
        ss.c_best = (uint32_t)best;
        fault_irq_on();
        delay_ms(1500);                                     /* standstill */
        /* start from standstill at 8 %, 1.5 s; the first 64 Hall edges timed from the start, the loop's cost per period */
        static const uint32_t on1[1] = {PCT(8u)}, ms1[1] = {1500u}; volatile uint32_t sec1[1] = {0}, mn1[1] = {0}, pk1[1] = {0}; uint32_t tw = 0;
        g_edge_log = ss.c_start_edges; g_edge_n = 0; g_loop_max = 0; g_loop_sum = 0; g_loop_n = 0;
        ss.run_stop = run_steps(tb[best], on1, ms1, 1, 0u, sec1, mn1, pk1, &tw);
        ss.c_n_start = g_edge_n; ss.c_loop_max = g_loop_max; ss.c_loop_sum = g_loop_sum; ss.c_loop_n = g_loop_n;
        ss.run_peak_mA = pk1[0]; ss.run_sectors[0] = sec1[0]; ss.run_net = (int32_t)mn1[0];
        /* coast: outputs off (run_steps ended with all phases Hi-Z and DRVOFF high); phase voltages and Halls every 20 us */
        uint32_t t0 = cyc(), tn = t0;
        for (uint32_t n = 0; n < COAST_N; n++) {
            while (cyc() - tn < CPU_HZ / 50000u) {}
            tn += CPU_HZ / 50000u;
            DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
            cap.h[n] = (uint8_t)hall();
            while (DL_HSADC_isBusy(HSADC1_INST)) {}
            cap.va[n] = (uint16_t)DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER1);
            cap.vb[n] = (uint16_t)DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER2);
            cap.vc[n] = (uint16_t)DL_HSADC_getResult(HSADC1_RESULT_INST, DL_HSADC_SOC_NUMBER3);
        }
        ss.c_coast_n = COAST_N; ss.c_coast_dt_cyc = CPU_HZ / 50000u;
        goto end;
    }
#endif
#ifdef TEST_LAT
    {   /* five trials: vector 0 at 6 %, PC2 (ILIMIT) driven to 0 V; main watches nFAULT in a tight loop while the
         * interrupt is enabled. Latency = interrupt entry minus the last cycle-counter reading with nFAULT still high. */
        uint32_t ok = 0;
        fault_irq_on();
        for (int tr = 0; tr < 5; tr++) {
            DL_GPIO_clearPins(GPIOC, DL_GPIO_PIN_2); delay_ms(2);
            g_trip = 0; uint32_t isr0 = g_isr_n;
            volatile uint32_t last_high = 0;
            drv_off(0); tick(); vector(0, ON_CYC);
            uint32_t t0 = cyc();
            while (cyc() - t0 < CPU_HZ / 100u) {            /* 10 ms at most */
                uint32_t t = cyc();
                if (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_30)) last_high = t; else break;
            }
            all_off(); drv_off(1);
            if (g_isr_n > isr0) { ss.l_lat[tr] = g_isr_cyc - last_high; ok++; }
            /* clear the driver's latched fault: sleep 2 ms, wake, limit back off (PC2 high) */
            DL_GPIO_setPins(GPIOC, DL_GPIO_PIN_2);
            DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); delay_ms(2); DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); delay_ms(10);
            DL_GPIO_clearInterruptStatus(GPIOB, DL_GPIO_PIN_30); NVIC_ClearPendingIRQ(GPIOB_INT_IRQn); g_trip = 0;
            ss.l_nfault_after_clear = nfault();
            delay_ms(100);
        }
        ss.l_trials_ok = ok; ss.l_isr_n = g_isr_n;
        goto end;
    }
#endif
#ifdef TEST_RUNS
    {   /* table search at 4 % (stall current about 0.46 A with synchronous freewheel), 0.3 s per table */
        uint8_t tb[12][8]; int32_t best = -1, bnet = 0;
        for (int t = 0; t < 12; t++) {
            int off = t % 6, with = t < 6;
            for (int hh = 0; hh < 8; hh++) tb[t][hh] = 0;
            for (int i = 0; i < 6; i++) tb[t][SEQ[i]] = (uint8_t)(with ? (i + off) % 6 : (off - i + 12) % 6);
            int32_t net = 0; uint32_t pk = 0, vml = 0;
            uint32_t r = drive(tb[t], 300u, &net, &pk, 0, &vml, PCT(4u));
            ss.net[t] = net; ss.peak_mA[t] = pk; ss.stop[t] = r;
            if (r == STOP_TRIP || r == STOP_FAULT || r == STOP_BUS) { ss.run_stop = 100 + r; goto end; }
            if (net > bnet) { bnet = net; best = t; }
            delay_ms(300);
        }
        if (best < 0 || bnet < 8) { ss.run_stop = 98; goto end; }
        ss.f_best = (uint32_t)best;
        fault_irq_on();
        /* 1. fault latency: 6 % for 1.5 s, then nSLEEP low at 1.0 s into the run */
        { static const uint32_t on1[1] = {PCT(6u)}, ms1[1] = {1500u}; volatile uint32_t sec1[1] = {0}, mn1[1] = {0}, pk1[1] = {0}; uint32_t tw = 0;
          ss.f_stop = run_steps(tb[best], on1, ms1, 1, 1000u, sec1, mn1, pk1, &tw);
          ss.f_isr_count = g_isr_n; ss.f_latency_cyc = g_isr_cyc - tw;
          ss.f_drvoff_after = DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_28) ? 1u : 0u; }
        delay_ms(500);
        DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); delay_ms(20);          /* wake again */
        DL_GPIO_clearInterruptStatus(GPIOB, DL_GPIO_PIN_30); g_trip = 0;
        ss.f_nfault_after_wake = nfault();
        delay_ms(300);
        /* 2. speed against on time, while running: 6, 4, 5, 6, 7, 8 % for 1 s each */
        { static const uint32_t on2[6] = {PCT(6u), PCT(4u), PCT(5u), PCT(6u), PCT(7u), PCT(8u)}, ms2[6] = {1000u, 1000u, 1000u, 1000u, 1000u, 1000u}; uint32_t tw = 0;
          for (int k = 0; k < 6; k++) { ss.sw_on[k] = on2[k]; ss.sw_ms[k] = ms2[k]; }
          ss.sw_stop = run_steps(tb[best], on2, ms2, 6, 0u, ss.sw_sectors, ss.sw_mean_mA, ss.sw_peak_mA, &tw); }
        goto end;
    }
#endif
#ifdef TEST_PADS
    /* outputs disabled (DRVOFF stays high): the gate pins' own levels, read through their input buffers */
    for (int k = 0; k < 3; k++) { IOMUX->SECCFG.PINCM[HPCM[k]] |= IOMUX_PINCM_INENA_ENABLE; IOMUX->SECCFG.PINCM[LPCM[k]] |= IOMUX_PINCM_INENA_ENABLE; }
    for (int v = 0; v < 6; v++) {
        tick(); vector(v, ON_CYC);
        for (int k = 0; k < 3; k++) { IOMUX->SECCFG.PINCM[HPCM[k]] |= IOMUX_PINCM_INENA_ENABLE; IOMUX->SECCFG.PINCM[LPCM[k]] |= IOMUX_PINCM_INENA_ENABLE; }
        tick();
        int hi = VHI[v], lo = VLO[v];
        for (int n = 0; n < 4000; n++) {
            ss.pads[v][0] += DL_GPIO_readPins(HP[hi], HPIN[hi]) ? 1u : 0u; ss.pads[v][1] += DL_GPIO_readPins(LP[hi], LPIN[hi]) ? 1u : 0u;
            ss.pads[v][2] += DL_GPIO_readPins(HP[lo], HPIN[lo]) ? 1u : 0u; ss.pads[v][3] += DL_GPIO_readPins(LP[lo], LPIN[lo]) ? 1u : 0u;
            wait_cyc(37u);                                  /* not a divisor of the period: the samples walk across it */
        }
        all_off();
    }
    goto end;
#endif
#ifdef TEST_BURST6
    { uint32_t vmd, i3[3]; drv_off(0);
      for (int v = 0; v < 6; v++) {
          tick(); vector(v, ON_CYC);
          for (int n = 0; n < 40; n++) { tick(); adc(i3, &vmd); ss.burst6[v][n] = (uint16_t)i3[VLO[v]]; }
          all_off(); delay_ms(100);
      }
      drv_off(1); goto end; }
#endif
    /* table search, two stages (results/tables-20261020.md): 1. travel, 12 tables at 4 % for 0.3 s; 2. each forward
     * table that turned (net 8 or more) steady at 6 % for 1 s; keep the one with the lowest mean current (centred
     * commutation: the early table turns faster on four times the current) */
    uint8_t tab[12][8];
    fault_irq_on();
    for (int t = 0; t < 12; t++) {
        int off = t % 6, with = t < 6;
        for (int h = 0; h < 8; h++) tab[t][h] = 0;
        for (int i = 0; i < 6; i++) tab[t][SEQ[i]] = (uint8_t)(with ? (i + off) % 6 : (off - i + 12) % 6);
        int32_t net = 0; uint32_t pk = 0, vml = 0;
        ss.stop[t] = drive(tab[t], 300u, &net, &pk, 0, &vml, PCT(4u));
        ss.net[t] = net; ss.peak_mA[t] = pk;
        delay_ms(300);
        if (ss.stop[t] == STOP_TRIP || ss.stop[t] == STOP_FAULT || ss.stop[t] == STOP_BUS) { ss.run_stop = 100 + ss.stop[t]; goto end; }
    }
    { int b = -1; uint32_t bmn = 0xFFFFFFFFu;
      for (int t = 0; t < 12; t++) {
          if (ss.net[t] < 8) continue;
          static const uint32_t on0[2] = {PCT(6u), PCT(6u)}, ms0[2] = {500u, 1000u};
          volatile uint32_t s0[2] = {0, 0}, m0[2] = {0, 0}, p0[2] = {0, 0}; uint32_t tw = 0;
          g_net = 0;
          uint32_t r = run_steps(tab[t], on0, ms0, 2, 0u, s0, m0, p0, &tw);
          ss.t_sec[t] = s0[1]; ss.t_mean_mA[t] = m0[1]; ss.t_peak_mA[t] = p0[1]; ss.t_stop[t] = r; ss.t_net[t] = g_net;
          if (r == STOP_NONE && g_net > 0 && m0[1] < bmn) { bmn = m0[1]; b = t; }
          delay_ms(1500);
      }
      if (b < 0) { ss.run_stop = 98; goto end; }
      ss.best = (uint32_t)b; ss.best_dir = 1;
      int32_t net = 0; uint32_t pk = 0, vml = 0; uint32_t t0 = cyc();
      ss.run_stop = drive(tab[b], 3000u, &net, &pk, ss.run_sectors, &vml, ON_CYC);
      ss.run_cycles = cyc() - t0; ss.run_net = net; ss.run_peak_mA = pk; ss.run_vm = vml; }
end:
    all_off(); drv_off(1); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13);   /* outputs off, driver asleep */
    ss.done = 0x600Du;
    while (1) { __WFI(); }
}
