/*
 * First run on the LP-MSPM33C321A + DRV8376EVM + BLY171S (Fox Red Labs, 2026-10-20). Our six-step Hall program in
 * its smallest form, scripted at boot (no console: the Halls sit on the backchannel UART pins), results in RAM (`ss`)
 * read over the probe. PWM is software at 20 kHz from the cycle counter (first run only; the timer comes next).
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

#define CPU_HZ   32000000u
#define PERIOD   (CPU_HZ / 20000u)     /* 1,600 cycles: 20 kHz */
#ifndef ON_CYC
#define ON_CYC   96u                   /* 6 % of the 1,600-cycle period: 18.1 V x 0.06 over 2 x 0.78 ohm, about 0.7 A at stall */
#endif
#define TRIP_A   0.8f
#define CNT_PER_A (4096.0f / 3.3f)     /* 1 V/A sense gain on the 3.3 V ADC */

/* phases 0 A, 1 B, 2 C: INH and INL pins */
static GPIO_Regs *const HP[3] = {GPIOC, GPIOC, GPIOB}; static const uint32_t HPIN[3] = {DL_GPIO_PIN_17, DL_GPIO_PIN_18, DL_GPIO_PIN_6};
static GPIO_Regs *const LP[3] = {GPIOC, GPIOC, GPIOB}; static const uint32_t LPIN[3] = {DL_GPIO_PIN_29, DL_GPIO_PIN_19, DL_GPIO_PIN_7};
static const uint32_t HPCM[3] = {IOMUX_PINCM70, IOMUX_PINCM71, IOMUX_PINCM23};
static const uint32_t LPCM[3] = {IOMUX_PINCM94, IOMUX_PINCM72, IOMUX_PINCM24};
/* line vectors 0..5, 60 degrees apart: high side, low side */
static const uint8_t VHI[6] = {0, 0, 1, 1, 2, 2};
static const uint8_t VLO[6] = {1, 2, 2, 0, 0, 1};
static const uint8_t SEQ[6] = {1, 5, 4, 6, 2, 3};   /* Hall order (A = 4, B = 2, C = 1), the AM13 bench's, unchanged wiring */

enum { STOP_NONE = 0, STOP_TRIP = 1, STOP_FAULT = 2, STOP_BUS = 3, STOP_STALL = 4, STOP_HALL = 5 };

volatile struct {
    uint32_t magic, zero[3], vm0;
    int32_t  net[12]; uint32_t peak_mA[12], stop[12];
    uint32_t best, best_dir;
    uint32_t run_sectors[6]; int32_t run_net; uint32_t run_peak_mA, run_stop, run_vm, run_cycles;
    uint32_t done;
    uint32_t pulse_i[6][3], pulse_v[6][3];
    uint32_t adc_wait_max, adc_cyc;
    uint16_t burst[2][40];
    uint32_t dbg_n;
    uint16_t burst6[6][40];                  /* TEST_BURST6: each vector at ON_CYC for 40 periods, its low phase's sense after each pulse */                   /* TEST_BURST: vector 0 at ON_CYC, B sense at the start and the end of each off time */          /* cycles waited for the ADC after a period; one blocking conversion pair */   /* TEST_PULSE: sense counts and phase-voltage counts at the end of a 20 us vector */
} ss;

static inline uint32_t cyc(void) { return DWT->CYCCNT; }
static void wait_cyc(uint32_t n) { uint32_t t = cyc(); while (cyc() - t < n) {} }
static void delay_ms(uint32_t ms) { while (ms--) wait_cyc(CPU_HZ / 1000u); }

static void out_low(uint32_t pcm, GPIO_Regs *p, uint32_t pin) { DL_GPIO_initDigitalOutput(pcm); DL_GPIO_clearPins(p, pin); DL_GPIO_enableOutput(p, pin); }
static void all_off(void) { for (int k = 0; k < 3; k++) { DL_GPIO_clearPins(HP[k], HPIN[k]); DL_GPIO_clearPins(LP[k], LPIN[k]); } }
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
static uint32_t drive(const uint8_t *tab, uint32_t ms, int32_t *net, uint32_t *peak_mA, volatile uint32_t *win, uint32_t *vm_last)
{
    uint32_t on = ON_CYC, t0 = cyc(), tedge = t0, i3[3], vm, stop = STOP_NONE;
    uint32_t h = hall(), cand = h, candn = 0; int p0 = pos(h); float pk = 0.0f;
    if (p0 < 0) return STOP_HALL;
    drv_off(0);
    uint32_t periods = ms * 20u; uint8_t vlast = 0xFF;
    for (uint32_t n = 0; n < periods; n++) {
        uint32_t ts = cyc();
        uint8_t v = tab[h];
        int hi = VHI[v], lo = VLO[v];
        if (v != vlast) {   /* change the low side only on a new vector (the burst tests built current; rewriting every period did not) */
            for (int k = 0; k < 3; k++) { if (k != lo) DL_GPIO_clearPins(LP[k], LPIN[k]); if (k != hi) DL_GPIO_clearPins(HP[k], HPIN[k]); }
            DL_GPIO_setPins(LP[lo], LPIN[lo]); vlast = v;
        }
        DL_GPIO_setPins(HP[hi], HPIN[hi]);
        while (cyc() - ts < on) {}
        DL_GPIO_clearPins(HP[hi], HPIN[hi]);
        /* start the conversions in the off time and collect them at the period's end: waiting for them here stretched
         * every period (run 3: about 23 mA where 0.27 A was expected) */
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
        while (cyc() - ts < PERIOD) {}
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

int main(void)
{
    SYSCFG_DL_init();                                  /* first: it resets GPIOA, B and C */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_GPIO_initDigitalOutput(IOMUX_PINCM65); DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_28);  /* DRVOFF high */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM30); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_13); /* nSLEEP low */
    for (int k = 0; k < 3; k++) { out_low(HPCM[k], HP[k], HPIN[k]); out_low(LPCM[k], LP[k], LPIN[k]); }
    DL_GPIO_initDigitalOutput(IOMUX_PINCM76); DL_GPIO_setPins(GPIOC, DL_GPIO_PIN_2); DL_GPIO_enableOutput(GPIOC, DL_GPIO_PIN_2);   /* ILIMIT high: off */
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM67, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM21, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM22, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM80, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    ss.magic = 0x55E16001u;

    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); delay_ms(10);          /* wake, outputs off */
    { uint32_t i3[3], vm, s[3] = {0, 0, 0}; for (int n = 0; n < 64; n++) { adc(i3, &vm); for (int k = 0; k < 3; k++) s[k] += i3[k]; }
      for (int k = 0; k < 3; k++) ss.zero[k] = s[k] / 64u; ss.vm0 = vm;
      uint32_t t = cyc(); adc(i3, &vm); ss.adc_cyc = cyc() - t; }
    if (!nfault() || vm_volts(ss.vm0) < 15.0f || vm_volts(ss.vm0) > 21.0f) { ss.run_stop = 99; goto end; }

#ifdef TEST_BURST6
    { uint32_t vmd, i3[3]; drv_off(0);
      for (int v = 0; v < 6; v++) {
          DL_GPIO_setPins(LP[VLO[v]], LPIN[VLO[v]]);
          for (int n = 0; n < 40; n++) {
              uint32_t ts = cyc();
              DL_GPIO_setPins(HP[VHI[v]], HPIN[VHI[v]]);
              while (cyc() - ts < ON_CYC) {}
              DL_GPIO_clearPins(HP[VHI[v]], HPIN[VHI[v]]);
              adc(i3, &vmd); ss.burst6[v][n] = (uint16_t)i3[VLO[v]];
              while (cyc() - ts < PERIOD) {}
          }
          all_off(); delay_ms(100);
      }
      drv_off(1); goto end; }
#endif
#ifdef TEST_BURST
    { uint32_t vmd, i3[3]; drv_off(0);
      DL_GPIO_setPins(LP[1], LPIN[1]);                      /* B low side on throughout */
      for (int n = 0; n < 40; n++) {
          uint32_t ts = cyc();
          DL_GPIO_setPins(HP[0], HPIN[0]);
          while (cyc() - ts < ON_CYC) {}
          DL_GPIO_clearPins(HP[0], HPIN[0]);
          adc(i3, &vmd); ss.burst[0][n] = (uint16_t)i3[1];
          while (cyc() - ts < PERIOD - 300u) {}
          adc(i3, &vmd); ss.burst[1][n] = (uint16_t)i3[1];
          while (cyc() - ts < PERIOD) {}
      }
      all_off(); drv_off(1); goto end; }
#endif
#ifdef TEST_PULSE
    /* one 20 us pulse per line vector: high side VHI and low side VLO on, then all off; at 18 V into 1.7 mH line-to-line
     * the current reaches about 0.2 A and decays in tens of microseconds through the body diodes */
    { uint32_t vmd; drv_off(0);
      for (int v = 0; v < 6; v++) {
          uint32_t ts = cyc(), i3[3];
          DL_GPIO_setPins(LP[VLO[v]], LPIN[VLO[v]]); DL_GPIO_setPins(HP[VHI[v]], HPIN[VHI[v]]);
          while (cyc() - ts < 32u * 18u) {}                 /* 18 us, then sample during the last 2 us */
          adc(i3, &vmd);
          all_off();
          for (int k = 0; k < 3; k++) ss.pulse_i[v][k] = i3[k];
          delay_ms(50);
      }
      drv_off(1); goto end; }
#endif
    /* table search */
    uint8_t tab[12][8];
    for (int t = 0; t < 12; t++) {
        int off = t % 6, with = t < 6;
        for (int h = 0; h < 8; h++) tab[t][h] = 0;
        for (int i = 0; i < 6; i++) tab[t][SEQ[i]] = (uint8_t)(with ? (i + off) % 6 : (off - i + 12) % 6);
        int32_t net = 0; uint32_t pk = 0, vml = 0;
        ss.stop[t] = drive(tab[t], 400u, &net, &pk, 0, &vml);
        ss.net[t] = net; ss.peak_mA[t] = pk;
        delay_ms(300);
        if (ss.stop[t] == STOP_TRIP || ss.stop[t] == STOP_FAULT || ss.stop[t] == STOP_BUS) { ss.run_stop = 100 + ss.stop[t]; goto end; }
    }
    { int b = 0; for (int t = 1; t < 12; t++) if (ss.net[t] > ss.net[b]) b = t;
      ss.best = (uint32_t)b; ss.best_dir = 1;
      if (ss.net[b] < 12) { ss.run_stop = 98; goto end; }        /* nothing turned two electrical cycles forward */
      int32_t net = 0; uint32_t pk = 0, vml = 0; uint32_t t0 = cyc();
      ss.run_stop = drive(tab[b], 3000u, &net, &pk, ss.run_sectors, &vml);
      ss.run_cycles = cyc() - t0; ss.run_net = net; ss.run_peak_mA = pk; ss.run_vm = vml; }
end:
    all_off(); drv_off(1); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13);   /* outputs off, driver asleep */
    ss.done = 0x600Du;
    while (1) { __WFI(); }
}
