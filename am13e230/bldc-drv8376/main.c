/* Anaheim BLY17 brushless motor on the DRV8376EVM, driven by the LP-AM13E230 (EVM plugged onto the BoosterPack headers).
 * Six-step (trapezoidal) commutation from the motor's three Hall sensors. Everything is driven from the serial port:
 *   ?  help          l  learn the Hall table (motor must be free to turn)     g  go (run at the set duty)
 *   0..9  duty 0..90%   + -  duty up/down 5%     f r  forward/reverse        s  stop (coast)   b  brake
 *   a  switch the lead (1 or 2 steps)   c  clear a driver fault   d  re-run the PWM-mode check   v  volts
 * Startup leaves DRVOFF asserted and selects the 6x encoding. The manual mode diagnostic is not run at boot.
 * Hall table: unknown until learned. Learn holds the rotor on each of the six vectors at a low duty and reads the Halls.
 * Live values for run_image.py: g_duty, g_dir, g_hall, g_rpm, g_fault, g_mode3x, g_align[8].
 */
#include <stdint.h>
#include <stdbool.h>
#include "device.h"
#include "ti_sdk_dl_config.h"

#define PRD          9999U        /* 200 MHz / 10,000 = 20 kHz */
#define CPU_HZ       200000000UL
#define POLE_PAIRS   4            /* BLY17 series: 8 poles (Anaheim Automation product sheet L010228) */
#define MAX_DUTY     40           /* software cap, percent; M changes PWM encoding, not this cap */
#define LEARN_DUTY   20           /* percent used to hold the rotor during learn (2 V at 10 V bus; the first motor needed more than 10) */
#define RAMP_MS      20           /* one percent of duty per this many ms */
#define STALL_MS     1500         /* running with no Hall edge this long: stop and say so */

volatile int32_t  g_duty   = 0;   /* commanded, percent */
volatile int32_t  g_act    = 0;   /* what the outputs carry now, after the ramp */
volatile int32_t  g_dir    = 1;   /* +1 forward, -1 reverse */
volatile uint32_t g_hall   = 0;
volatile uint32_t g_rpm    = 0;   /* mechanical, from the last six Hall edges */
volatile uint32_t g_fault  = 0;
volatile uint32_t g_mode3x = 0;   /* 0 = 6x PWM mode, 1 = 3x, 2 = could not tell (no bus voltage?) */
volatile uint8_t  g_align[8];     /* Hall state -> the vector that holds the rotor there (0xFF = unseen) */
volatile uint32_t g_edges  = 0;
static uint32_t g_ol_hz = 50, g_ol_ms = 2000, g_vf = 0;   /* g_vf: extra duty percent per 100 Hz electrical during the open-loop ramp (V/f) */   /* open-loop start target (electrical Hz; 50 Hz = 750 rpm at 4 pole pairs) and ramp time */
static uint32_t g_learn_duty = LEARN_DUTY; /* L raises the learn hold in 5% steps, to 35% maximum */
static bool speed_mode = false; static int32_t g_target_rpm = 0;   /* speed loop: PI on the Hall rpm, output is the duty */
static int32_t sp_kp = 1, sp_ki = 1;                                  /* out = Kp x err / 100 (percent per 100 rpm) + I; I += Ki x err / 10 every 10 ms (x100 percent) */
static int32_t sp_i = 0;                                              /* integrator, in duty percent x 100 */
static int16_t g_log[150]; static uint32_t g_logn = 0;               /* rpm every 10 ms during a step, 1.5 s */
#define TRACE_SAMPLES 401U
static struct { uint32_t us, edges; uint16_t rpm, duty; } g_trace[TRACE_SAMPLES];
static uint32_t g_tracen = 0, g_trace_start = 0;
static int32_t g_trace_target = 0, g_trace_dir = 1;
static bool g_trace_sine = false;
static bool sine_mode = false; static int g_fam = 1;   /* sine: the three half bridges switch complementary (B = not A, the chip adds dead time) and carry 50 percent plus a sine at the interpolated rotor angle */
static uint32_t g_edge_t = 0, g_edge_h = 0;             /* the last Hall edge: time and new state */
static const int16_t SIN360[360] = {
#define S(a) (int16_t)(a)
0,175,349,523,698,872,1045,1219,1392,1564,1736,1908,2079,2250,2419,2588,2756,2924,3090,3256,3420,3584,3746,3907,4067,4226,4384,4540,4695,4848,5000,5150,5299,5446,5592,5736,5878,6018,6157,6293,6428,6561,6691,6820,6947,7071,7193,7314,7431,7547,7660,7771,7880,7986,8090,8192,8290,8387,8480,8572,8660,8746,8829,8910,8988,9063,9135,9205,9272,9336,9397,9455,9511,9563,9613,9659,9703,9744,9781,9816,9848,9877,9903,9925,9945,9962,9976,9986,9994,9998,10000,
9998,9994,9986,9976,9962,9945,9925,9903,9877,9848,9816,9781,9744,9703,9659,9613,9563,9511,9455,9397,9336,9272,9205,9135,9063,8988,8910,8829,8746,8660,8572,8480,8387,8290,8192,8090,7986,7880,7771,7660,7547,7431,7314,7193,7071,6947,6820,6691,6561,6428,6293,6157,6018,5878,5736,5592,5446,5299,5150,5000,4848,4695,4540,4384,4226,4067,3907,3746,3584,3420,3256,3090,2924,2756,2588,2419,2250,2079,1908,1736,1564,1392,1219,1045,872,698,523,349,175,0,
-175,-349,-523,-698,-872,-1045,-1219,-1392,-1564,-1736,-1908,-2079,-2250,-2419,-2588,-2756,-2924,-3090,-3256,-3420,-3584,-3746,-3907,-4067,-4226,-4384,-4540,-4695,-4848,-5000,-5150,-5299,-5446,-5592,-5736,-5878,-6018,-6157,-6293,-6428,-6561,-6691,-6820,-6947,-7071,-7193,-7314,-7431,-7547,-7660,-7771,-7880,-7986,-8090,-8192,-8290,-8387,-8480,-8572,-8660,-8746,-8829,-8910,-8988,-9063,-9135,-9205,-9272,-9336,-9397,-9455,-9511,-9563,-9613,-9659,-9703,-9744,-9781,-9816,-9848,-9877,-9903,-9925,-9945,-9962,-9976,-9986,-9994,-9998,-10000,
-9998,-9994,-9986,-9976,-9962,-9945,-9925,-9903,-9877,-9848,-9816,-9781,-9744,-9703,-9659,-9613,-9563,-9511,-9455,-9397,-9336,-9272,-9205,-9135,-9063,-8988,-8910,-8829,-8746,-8660,-8572,-8480,-8387,-8290,-8192,-8090,-7986,-7880,-7771,-7660,-7547,-7431,-7314,-7193,-7071,-6947,-6820,-6691,-6561,-6428,-6293,-6157,-6018,-5878,-5736,-5592,-5446,-5299,-5150,-5000,-4848,-4695,-4540,-4384,-4226,-4067,-3907,-3746,-3584,-3420,-3256,-3090,-2924,-2756,-2588,-2419,-2250,-2079,-1908,-1736,-1564,-1392,-1219,-1045,-872,-698,-523,-349,-175};
static int  lead = 0;             /* extra steps of commutation advance; a cycles -1, 0, +1 */
static bool running = false, learned = false;
static void commutate(uint32_t h, uint32_t pct);

enum { PH_A = 0, PH_B, PH_C };
static const uint8_t VEC_HI[6] = { PH_A, PH_A, PH_B, PH_B, PH_C, PH_C };
static const uint8_t VEC_LO[6] = { PH_B, PH_C, PH_C, PH_A, PH_A, PH_B };
static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE INH[3] = { DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_OUTPUT_3A };
static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE INL[3] = { DL_MCPWM_AQ_OUTPUT_1B, DL_MCPWM_AQ_OUTPUT_2B, DL_MCPWM_AQ_OUTPUT_3B };
static const DL_MCPWM_COUNTER_COMPARE_MODULE CMP[3] = { DL_MCPWM_COUNTER_COMPARE_1A, DL_MCPWM_COUNTER_COMPARE_2A, DL_MCPWM_COUNTER_COMPARE_3A };

/* ---- serial ---- */
static void putc_(char c) { while (DL_UART_isTXFIFOFull(UART_0_INST)) {} DL_UART_transmitData(UART_0_INST, (uint8_t)c); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void putn(int32_t n) { char b[12]; int i = 0; if (n < 0) { putc_('-'); n = -n; } do { b[i++] = (char)('0' + n % 10); n /= 10; } while (n); while (i) putc_(b[--i]); }
static void putkv(const char *k, int32_t v) { puts_(k); putn(v); putc_(' '); }
static int getc_(void) { return DL_UART_isRXFIFOEmpty(UART_0_INST) ? -1 : (int)DL_UART_receiveData(UART_0_INST); }

/* ---- time ---- */
static inline uint32_t cyc(void) { return DWT->CYCCNT; }
static void delay_ms(uint32_t ms) { DEVICE_DELAY_MS(ms); }

/* ---- driver pins ---- */
static void drv_sleep(bool sleep) { if (sleep) DL_GPIO_clearPins(DRVA_PORT, DRVA_NSLEEP_PIN); else DL_GPIO_setPins(DRVA_PORT, DRVA_NSLEEP_PIN); }
static void drv_off(bool off)     { if (off) DL_GPIO_setPins(DRVB_PORT, DRVB_DRVOFF_PIN); else DL_GPIO_clearPins(DRVB_PORT, DRVB_DRVOFF_PIN); }
/* J9 pin 6 -> LaunchPad J1 pin 8 / PB4 follows the driver's fault transition.
   PA22 via the unpopulated R25 stays high and is retained only for diagnostics. */
static bool drv_fault(void)       { return (DL_GPIO_readPins(DRVB_PORT, DRVB_NFAULT_ALT_PIN) == 0U); }
static void led(bool on)          { if (on) DL_GPIO_clearPins(DRVA_PORT, DRVA_LED4_PIN); else DL_GPIO_setPins(DRVA_PORT, DRVA_LED4_PIN); }

static uint32_t hall_raw(void)
{
    uint32_t b = DL_GPIO_readPins(HALLB_PORT, HALLB_A_PIN | HALLB_B_PIN);
    uint32_t c = DL_GPIO_readPins(HALLA_PORT, HALLA_C_PIN);
    return ((b & HALLB_A_PIN) ? 4U : 0U) | ((b & HALLB_B_PIN) ? 2U : 0U) | (c ? 1U : 0U);
}
/* Filtered: 24 reads 3 us apart (72 us, more than one 50 us PWM period), majority per bit. Switching noise on the Hall
   lines flips single reads; a true edge at 4,000 rpm is 630 us from the next. */
static uint32_t hall(void)
{
    uint32_t i, a = 0, b = 0, c = 0, h;
    for (i = 0; i < 24U; i++) { h = hall_raw(); a += h & 4U; b += h & 2U; c += h & 1U; DEVICE_DELAY_US(3); }
    return (a >= 13U * 4U ? 4U : 0U) | (b >= 13U * 2U ? 2U : 0U) | (c >= 13U ? 1U : 0U);
}

/* ---- the three half bridges ---- */
static void sw(DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o, DL_MCPWM_ACTION_QUALIFIER_SW_FORCE_OUTPUT a) { DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM_INST, o, a); }

static void phase_off(int p)  { sw(INH[p], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[p], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); }
static void phase_low(int p)  { sw(INH[p], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[p], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); }
static void phase_pwm(int p, uint32_t pct)
{
    /* 6x mode: INL low, the high-side chops and the current returns through the body diode.
       3x mode: INL high enables the half bridge, INH is the PWM. */
    sw(INL[p], g_mode3x == 1U ? DL_MCPWM_AQ_SW_CONTINUOUS_HIGH : DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    if (pct >= 100U) { sw(INH[p], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); return; }
    DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM_INST, CMP[p], (uint16_t)((pct * (PRD + 1U)) / 100U));
    sw(INH[p], pct == 0U ? DL_MCPWM_AQ_SW_CONTINUOUS_LOW : DL_MCPWM_AQ_SW_FORCE_DISABLED);
}
static void all_off(void)   { phase_off(PH_A); phase_off(PH_B); phase_off(PH_C); }
static void all_low(void)   { phase_low(PH_A); phase_low(PH_B); phase_low(PH_C); }   /* brake: all low sides on */
/* Phase-axis vectors, 60 degrees apart starting on the A axis: 0 A+ (B- C-), 1 A+ B+ (C-), 2 B+ (A- C-), 3 B+ C+ (A-), 4 C+ (A- B-), 5 A+ C+ (B-).
   They sit halfway between the six line vectors, in the middle of the Hall sectors of a motor whose Halls are aligned with the back-EMF. */
static void axis(int j, uint32_t pct)
{
    static const uint8_t HI[6][3] = { {1,0,0}, {1,1,0}, {0,1,0}, {0,1,1}, {0,0,1}, {1,0,1} };
    int p;
    for (p = 0; p < 3; p++) if (!HI[j][p]) phase_low(p);
    for (p = 0; p < 3; p++) if (HI[j][p]) phase_pwm(p, pct);
}
static uint8_t g_fwd[8], g_rev[8];   /* Hall state -> line vector to apply, forward and reverse */
static uint8_t g_seq[6] = {1, 5, 4, 6, 2, 3}; /* measured 2026-10-15; learn replaces this for new wiring */
static int hall_pos(uint32_t h)
{
    for (int i = 0; i < 6; i++) if (g_seq[i] == h) return i;
    return -1;
}
/* Number of consecutive Hall sectors in one direction. A reversal or skipped state
   discards the apparent travel, so dithering across one boundary is never a turn. */
static uint32_t track_hall(uint32_t prev, uint32_t next, int *dir, uint32_t count)
{
    int a = hall_pos(prev), b = hall_pos(next);
    int step = (a >= 0 && b >= 0) ? (b - a + 6) % 6 : 0;
    if (step != 1 && step != 5) { *dir = 0; return 0; }
    if (*dir != step) { *dir = step; return 1; }
    return count + 1;
}
static void vector(int v, uint32_t pct)
{
    int p;
    for (p = 0; p < 3; p++) if (p != VEC_HI[v] && p != VEC_LO[v]) phase_off(p);
    phase_low(VEC_LO[v]);
    phase_pwm(VEC_HI[v], pct);
}

/* ---- ADC: VSENA (SOC0) and VSENVM (SOC1), raw 12-bit counts through the EVM's dividers ---- */
static void adc_read(uint32_t *vsena, uint32_t *vsenvm)
{
    uint32_t t = 2000;
    DL_ADC_forceSequencer(APP_ADC_INST, APP_ADC_SEQ1);
    while (DL_ADC_isBusy(APP_ADC_INST) && t--) {}
    *vsena  = DL_ADC_readResult(APP_ADC_RESULT_INST, DL_ADC_SOC_NUMBER0);
    *vsenvm = DL_ADC_readResult(APP_ADC_RESULT_INST, DL_ADC_SOC_NUMBER1);
}
/* EVM phase and bus sense dividers: 143 k over 6.04 k with 100 nF (SLVUD20 figure 4-1, R37 to R44), ratio 24.675. */
static int32_t counts_to_mv(uint32_t c) { return (int32_t)((c * 3300ULL * 14904ULL) / (4095ULL * 604ULL)); }

/* The DRV8376's three current-sense outputs, sampled by the PWM period event (all low sides on at that moment):
   SOx = VREF/2 + 1 V/A x I (gain 1 V/A on this EVM, VREF jumper unknown: the rest level says which). */
static void isen_read(uint32_t *a, uint32_t *b, uint32_t *c)
{
    *a = DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER0);   /* ISENA PA27 */
    *b = DL_ADC_readResult(ADC_C_RESULT_INST, DL_ADC_SOC_NUMBER0);   /* ISENB PA30 */
    *c = DL_ADC_readResult(ADC_C_RESULT_INST, DL_ADC_SOC_NUMBER1);   /* ISENC PA29 */
}
static void isen_force(uint32_t *a, uint32_t *b, uint32_t *c)
{   /* a fresh conversion now, not the last PWM-triggered one */
    uint32_t t = 2000;
    DL_ADC_forceSequencer(ADC_B_INST, ADC_B_SEQ1); DL_ADC_forceSequencer(ADC_C_INST, ADC_C_SEQ1);
    while ((DL_ADC_isBusy(ADC_B_INST) || DL_ADC_isBusy(ADC_C_INST)) && t--) {}
    isen_read(a, b, c);
}
static int32_t adc_mv(uint32_t c) { return (int32_t)((c * 3300U) / 4095U); }
static int32_t g_rest[3] = {0, 0, 0};   /* SOx at zero current, mV */

/* Drive phase A high alone, read it back. No current can flow with B and C floating. */
/* Drive phase A high alone, read it back. No current can flow with B and C floating. Between tests phase A is pulled low
   for a moment so the sense divider does not remember the last reading. */
static void detect_mode(void)
{
    uint32_t a6, a3, vm;
    all_off(); drv_off(false); drv_sleep(false); delay_ms(5);
    sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400);   /* discharge; the EVM's sense divider is slow */
    sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW);  sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400); adc_read(&a6, &vm);
    sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400);   /* discharge; the EVM's sense divider is slow */
    sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400); adc_read(&a3, &vm);
    all_off();
    puts_("mode check: vm="); putn(counts_to_mv(vm)); puts_("mV  A(6x code)="); putn(counts_to_mv(a6)); puts_("mV  A(3x code)="); putn(counts_to_mv(a3)); puts_("mV -> ");
    if (vm < 300U)                 { g_mode3x = 2; puts_("no bus voltage on VM, cannot tell\r\n"); return; }
    if (a6 > vm / 2U && a3 < vm / 2U) { g_mode3x = 0; puts_("6x PWM mode\r\n"); return; }
    if (a3 > vm / 2U && a6 < vm / 2U) { g_mode3x = 1; puts_("3x PWM mode\r\n"); return; }
    g_mode3x = 2; puts_("ambiguous: both codes read the same; is nFAULT set, DRVOFF jumpered, or VSENA unconnected?\r\n");
}

static void stop(void) { running = false; all_off(); g_act = 0; g_rpm = 0; led(false); }

static uint32_t g_hist[16]; static uint32_t g_hn = 0;   /* the last Hall states seen, oldest first */

/* Net sectors travelled along the Hall order seq (forward +1, backward -1) while commutating with the current table. */
static int32_t run_net(const uint32_t *seq, uint32_t ms, uint32_t pct)
{
    uint32_t t0 = cyc(), h, last = hall(); int32_t net = 0; int li = -1, i;
    for (i = 0; i < 6; i++) if (seq[i] == last) li = i;
    while (cyc() - t0 < (CPU_HZ / 1000U) * ms) {
        if (drv_fault()) { all_off(); drv_off(true); return net; }
        h = hall(); commutate(h, pct);
        if (h != last && h != 0U && h != 7U) {
            int ni = -1; for (i = 0; i < 6; i++) if (seq[i] == h) ni = i;
            if (li >= 0 && ni >= 0) { if (ni == (li + 1) % 6) net++; else if (ni == (li + 5) % 6) net--; }
            last = h; li = ni; g_hall = h; g_edges++;
        }
    }
    all_off();
    return net;
}

static void learn(void)
{
    int i, k, off, best = -1; uint32_t h, seq[6], states[12]; int32_t best_score = 0, scores[12][2];
    stop();
    if (drv_fault()) { puts_("learn skipped: driver fault\r\n"); return; }
    drv_off(false);
    /* 1. Hold the rotor on each phase axis (the middle of a Hall sector), twice round, and read the state at the end of each hold. */
    puts_("learn 1: holding on the six phase axes at "); putn((int32_t)g_learn_duty); puts_("%: ");
    for (k = 0; k < 12; k++) {
        axis(k % 6, g_learn_duty);
        for (uint32_t wait = 0; wait < 350U; wait++) {
            if (drv_fault()) { all_off(); drv_off(true); puts_("\r\nlearn stopped: driver fault\r\n"); return; }
            delay_ms(1);
        }
        states[k] = hall(); putn((int32_t)states[k]); putc_(' ');
    }
    all_off(); delay_ms(300); puts_("\r\n");
    /* the second round is the order; it must be six different valid states */
    for (i = 0; i < 6; i++) seq[i] = states[6 + i];
    for (i = 0; i < 6; i++) for (k = 0; k < i; k++) if (seq[i] == seq[k] || seq[i] == 0U || seq[i] == 7U) {
        puts_("learn failed: the axes did not give six different Hall states (rotor stuck, or a Hall wire off)\r\n"); return; }
    /* 2. Twelve tables fit that order (vectors with or against it, six offsets). Score each forward and reverse by net sectors. */
    for (off = 0; off < 12; off++) {
        for (i = 0; i < 6; i++) { int v = off < 6 ? (i + off) : (off - i + 6); g_fwd[seq[i]] = (uint8_t)(v % 6); g_rev[seq[i]] = (uint8_t)((v + 3) % 6); }
        g_dir = 1;  scores[off][0] = run_net(seq, 800, g_learn_duty + 5); delay_ms(300);
        g_dir = -1; scores[off][1] = run_net(seq, 800, g_learn_duty + 5); delay_ms(300); g_dir = 1;
        puts_("  table "); putn(off); puts_(": forward "); putn(scores[off][0]); puts_(" sectors, reverse "); putn(scores[off][1]); puts_("\r\n");
        if (drv_fault()) { puts_("learn stopped: driver fault\r\n"); return; }
    }
    /* forward: the table that travelled furthest with +. reverse: the table and direction that travelled furthest with -. */
    { int bf = -1, br = -1, brd = 0; int32_t sf = 6, sr = -6;
      for (off = 0; off < 12; off++) { if (scores[off][0] > sf) { sf = scores[off][0]; bf = off; }
                                       if (scores[off][0] < sr) { sr = scores[off][0]; br = off; brd = 0; }
                                       if (scores[off][1] < sr) { sr = scores[off][1]; br = off; brd = 1; } }
      if (bf < 0 || br < 0) { puts_("learn failed: no table moved forward and reverse\r\n"); return; }
      for (i = 0; i < 6; i++) { int v = bf < 6 ? (i + bf) : (bf - i + 6); g_fwd[seq[i]] = (uint8_t)(v % 6); }
      for (i = 0; i < 6; i++) { int v = br < 6 ? (i + br) : (br - i + 6); g_rev[seq[i]] = (uint8_t)((v + (brd ? 3 : 0)) % 6); }
      best = bf; best_score = sf; (void)best_score; g_fam = bf < 6 ? 1 : -1; }
    for (i = 0; i < 6; i++) g_seq[i] = (uint8_t)seq[i];
    learned = true;
    puts_("learned: table "); putn(best); puts_(". hall -> forward vector / reverse vector: ");
    for (i = 1; i < 7; i++) { putn(i); putc_(':'); putn(g_fwd[i]); putc_('/'); putn(g_rev[i]); putc_(' '); }
    puts_("\r\nnow: g to run (duty "); putn(g_duty); puts_("%)\r\n");
}

/* Sine: rotor angle from the last Hall edge plus speed x time, clamped inside the sector; voltage 90 degrees ahead in the
   direction of travel; each half bridge gets 50 percent plus (pct/2) x cos of its phase angle, A then B then C 120 apart. */
static int vec_angle(int v) { return -30 + 60 * v; }
static void sine_update(uint32_t h, uint32_t pct)
{
    int v, c, enter, theta, thv, p, travel;
    uint32_t dt;
    if (h == 0U || h == 7U || !learned) { all_off(); return; }
    v = (int)(g_dir > 0 ? g_fwd[h] : g_rev[h]);
    c = vec_angle(v) - (g_dir > 0 ? 90 : -90) * 1;           /* sector centre: the field is 90 degrees ahead when the table's vector is applied */
    if (g_dir < 0) c = vec_angle(v) + 90;
    enter = c - g_fam * g_dir * 30;                           /* where the rotor was when it entered this sector */
    dt = cyc() - g_edge_t;
    travel = (int)(((uint64_t)dt * g_rpm * POLE_PAIRS * 6U) / CPU_HZ);   /* degrees electrical since the edge: rpm x pole pairs x 360 / 60 */
    if (travel > 60) travel = 60;
    theta = enter + g_fam * g_dir * travel;
    thv = theta + g_fam * g_dir * 90;
    for (p = 0; p < 3; p++) {
        int a = ((thv - 120 * p) % 360 + 360) % 360;
        int32_t duty = 5000 + ((int32_t)pct * 50 * SIN360[(a + 90) % 360]) / 10000;   /* cos = sin(a + 90); duty in hundredths of a percent */
        if (duty < 200) duty = 200; if (duty > 9800) duty = 9800;
        DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM_INST, CMP[p], (uint16_t)(((uint32_t)duty * (PRD + 1U)) / 10000U));
        sw(INH[p], DL_MCPWM_AQ_SW_FORCE_DISABLED); sw(INL[p], DL_MCPWM_AQ_SW_FORCE_DISABLED);
    }
}

static void commutate(uint32_t h, uint32_t pct)
{
    if (sine_mode) { sine_update(h, pct); return; }
    int v;
    if (h == 0U || h == 7U) { all_off(); return; }
    v = (int)(g_dir > 0 ? g_fwd[h] : g_rev[h]) + (g_dir > 0 ? lead : -lead);   /* lead: 0 normally; a trims it by one step */
    v = ((v % 6) + 6) % 6;
    vector(v, pct);
}

static void trace_sample(uint32_t us)
{
    if (g_tracen >= TRACE_SAMPLES) return;
    g_trace[g_tracen].us = us;
    g_trace[g_tracen].edges = g_edges;
    g_trace[g_tracen].rpm = (uint16_t)g_rpm;
    g_trace[g_tracen++].duty = (uint16_t)g_act;
}
static void trace_start(void)
{
    g_tracen = 0; g_trace_start = cyc(); g_trace_target = g_target_rpm;
    g_trace_dir = g_dir; g_trace_sine = sine_mode; trace_sample(0);
}

static void help(void)
{
    puts_("\r\nBLDC on DRV8376EVM. l learn  L learn duty +5%  g go  s stop  b brake  0-9 duty x10%  + - 5%  f r direction  a lead  c clear fault  d mode check  v volts  ? help\r\n");
    puts_("cap "); putn(MAX_DUTY); puts_("%  pole pairs "); putn(POLE_PAIRS); puts_("  mode "); putn((int32_t)g_mode3x); puts_("  learned "); putn(learned); puts_("\r\n");
}

static void status(void)
{
    uint32_t a, vm; adc_read(&a, &vm);
    puts_(running ? "run " : "idle "); puts_(sine_mode ? "sine " : "6step "); putkv("duty=", g_act); putkv("set=", g_duty); puts_(g_dir > 0 ? "fwd " : "rev "); putkv("lead=", lead);
    putkv("hall=", (int32_t)g_hall); putkv("rpm=", (int32_t)g_rpm); putkv("edges=", (int32_t)g_edges);
    putkv("hall_changes=", (int32_t)g_hn); putkv("vm_mV=", counts_to_mv(vm)); putkv("fault=", (int32_t)g_fault); putkv("speed_mode=", speed_mode); putkv("target_rpm=", g_target_rpm); putkv("kp=", sp_kp); putkv("ki=", sp_ki); puts_("\r\n");
}

/* One speed-loop update, every 10 ms: error in rpm; duty = Kp*err/100 + I, I += Ki*err/1000; both clamped. Kp 20 = 2 percent per 10 rpm... tuned by eye on the bench. */
static void speed_step(void)
{
    int32_t err = g_target_rpm - (int32_t)g_rpm, out;
    sp_i += (sp_ki * err) / 10;                                        /* x100 percent */
    if (sp_i > MAX_DUTY * 100) sp_i = MAX_DUTY * 100; if (sp_i < 0) sp_i = 0;
    out = (sp_kp * err) / 100 + sp_i / 100;
    if (out > MAX_DUTY) out = MAX_DUTY; if (out < 0) out = 0;
    g_duty = out; g_act = out;
}

int main(void)
{
    uint32_t last_hall, t_edge, t_ramp, t_stat, t_stall, dt[6] = {0}, di = 0, progress = 0;
    int progress_dir = 0;
    int c, i;
    Device_Init();
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_ADC_powerUp(APP_ADC_INST); DL_ADC_powerUp(ADC_B_INST); DL_ADC_powerUp(ADC_C_INST); DEVICE_DELAY_US(5000);
    DL_SYSCTL_disableDACOUT(); /* PA4 is the high-impedance meter input. */
    all_off();
    DL_MCPWM_enableTBCLK();
    drv_off(true); drv_sleep(false); delay_ms(10);          /* wake the driver with the outputs disabled */
    DL_GPIO_setPins(DRVB_PORT, DRVB_MCULED_PIN);            /* the EVM's MCU LED: the LaunchPad is talking */
    g_mode3x = 0;   /* TI table 7-2: floating MODE_SR selects 6x. Previous successful run used 6x. */
    help();
    /* Never run detect_mode() on boot: it enables the bridge and emits pulses.
       Leave DRVOFF asserted until an explicit command requests a test. */
    puts_("using 6x PWM encoding (M toggles); boot outputs disabled; F reads both fault inputs\r\n");
    g_hall = last_hall = hall(); t_edge = t_ramp = t_stat = t_stall = cyc();
    puts_("hall now "); putn((int32_t)g_hall); puts_("  (learn with l before the first run)\r\n");
    while (1) {
        uint32_t now = cyc(), h = hall();
        /* faults: nFAULT low */
        if (drv_fault() && !g_fault) {
            uint32_t fa, fb, fc; int32_t duty_at_fault = g_act;
            isen_read(&fa, &fb, &fc);
            g_fault = 1; stop(); drv_off(true);
            puts_("DRIVER FAULT (PB4 nFAULT low): outputs off. duty="); putn(duty_at_fault);
            puts_(" sense_mV="); putn(adc_mv(fa)); putc_('/'); putn(adc_mv(fb)); putc_('/'); putn(adc_mv(fc));
            puts_(" c to clear\r\n");
        }
        /* A rocking rotor can cross the same Hall boundary forever. Count a turn only after six
           consecutive sectors in one direction; opposite or skipped edges reset the speed estimate. */
        if (h != last_hall) {
            uint32_t d = now - t_edge;
            int prev = hall_pos(last_hall), next = hall_pos(h);
            int step = (prev >= 0 && next >= 0) ? (next - prev + 6) % 6 : 0;
            int want = running ? ((g_dir * g_fam > 0) ? 1 : 5) : (progress_dir ? progress_dir : step);
            t_edge = now; last_hall = h; g_hall = h; g_hist[g_hn % 16U] = h; g_hn++; g_edge_t = now; g_edge_h = h;
            if ((step == 1 || step == 5) && step == want) {
                progress_dir = step; dt[di++] = d; progress++;
                if (progress == 6U) {
                    uint32_t s = dt[0]+dt[1]+dt[2]+dt[3]+dt[4]+dt[5];
                    g_rpm = s ? (uint32_t)((60ULL * CPU_HZ) / ((uint64_t)s * POLE_PAIRS)) : 0;
                    g_edges += 6U; t_stall = now; progress = di = 0;
                }
            } else { progress = di = 0; progress_dir = 0; g_rpm = 0; }
            if (running) commutate(h, (uint32_t)g_act);
        }
        /* speed loop: every 10 ms set the duty from the rpm error, and log the rpm while a step is being recorded */
        if (running && speed_mode && (now - t_ramp) > (CPU_HZ / 100U)) {
            t_ramp = now; speed_step(); commutate(h, (uint32_t)g_act);
            if (g_logn < 150U) g_log[g_logn++] = (int16_t)g_rpm;
            trace_sample((now - g_trace_start) / (CPU_HZ / 1000000U));
        }
        /* ramp the duty toward the setpoint */
        if (running && !speed_mode && (now - t_ramp) > (CPU_HZ / 1000U) * RAMP_MS) {
            t_ramp = now;
            if (g_act < g_duty) g_act++; else if (g_act > g_duty) g_act--;
            commutate(h, (uint32_t)g_act);
        }
        if (running && g_act > 0 && (now - t_stall) > (CPU_HZ / 1000U) * STALL_MS) { stop(); puts_("no Hall edge for 1.5 s: stopped (shaft stuck, duty too low, or the table is wrong: try a)\r\n"); }
        if (running && (now - t_stat) > CPU_HZ) { t_stat = now; status(); }
        if ((now - t_edge) > CPU_HZ) g_rpm = 0;
        /* commands */
        c = getc_();
        if (c < 0) continue;
        if (c >= '0' && c <= '9' && speed_mode) { g_target_rpm = (c - '0') * 500; g_logn = 0; trace_start(); putkv("target rpm ", g_target_rpm); puts_("\r\n"); }
        else if (c >= '0' && c <= '9') { g_duty = (c - '0') * 10; if (g_duty > MAX_DUTY) g_duty = MAX_DUTY; putkv("duty set ", g_duty); puts_("\r\n"); }
        else { switch (c) {
            case '+': g_duty += 5; if (g_duty > MAX_DUTY) g_duty = MAX_DUTY; putkv("duty set ", g_duty); puts_("\r\n"); break;
            case '-': g_duty -= 5; if (g_duty < 0) g_duty = 0; putkv("duty set ", g_duty); puts_("\r\n"); break;
            case 'l': learn(); break;
            case 'L': g_learn_duty = g_learn_duty >= 35U ? LEARN_DUTY : g_learn_duty + 5U; putkv("learn duty percent ", (int32_t)g_learn_duty); puts_("\r\n"); break;
            case 'g': if (!learned) { puts_("learn first (l)\r\n"); break; } if (g_fault) { puts_("clear the fault first (c)\r\n"); break; }
                      if (speed_mode) { sp_i = 0; g_logn = 0; if (g_target_rpm == 0) g_target_rpm = 1000; }
                      if (g_duty == 0 && !speed_mode) g_duty = 15; running = true; g_act = 0; progress = di = 0; progress_dir = 0; g_rpm = 0; if (speed_mode) trace_start(); led(true); drv_off(false); t_stall = t_edge = cyc(); commutate(hall(), 0); puts_("running\r\n"); break;
            case 's': stop(); puts_("stopped (coast)\r\n"); break;
            case 'b': stop(); all_low(); puts_("brake (all low sides on); s to release\r\n"); break;
            case 'f': if (g_dir < 0) { stop(); g_dir = 1; puts_("forward; g to run\r\n"); } break;
            case 'r': if (g_dir > 0) { stop(); g_dir = -1; puts_("reverse; g to run\r\n"); } break;
            case 'a': lead = (lead == 0) ? 1 : (lead == 1 ? -1 : 0); putkv("lead ", lead); puts_("\r\n"); break;
            case 'c': stop(); drv_off(true); drv_sleep(true); delay_ms(1);
                      drv_sleep(false); delay_ms(6); g_fault = drv_fault() ? 1U : 0U;
                      puts_(g_fault ? "fault still present; outputs disabled\r\n" : "fault cleared; outputs disabled\r\n"); break;
            case 'd': stop(); detect_mode(); break;
            case 'h': { static int hv = 0; uint32_t a, vm, h1, h2, nf = 0, nn = 0, t0; stop(); vector(hv, LEARN_DUTY); delay_ms(300); adc_read(&a, &vm); h1 = hall();
                        { uint32_t ia, ib, ic; int64_t sa = 0, sb = 0, sc = 0; uint32_t ns = 0;
                          t0 = cyc(); while (cyc() - t0 < CPU_HZ / 2U) { if (drv_fault()) nf++; nn++; isen_read(&ia, &ib, &ic); sa += adc_mv(ia) - g_rest[0]; sb += adc_mv(ib) - g_rest[1]; sc += adc_mv(ic) - g_rest[2]; ns++; }
                          puts_("  current mA A "); putn((int32_t)(sa / ns)); puts_(" B "); putn((int32_t)(sb / ns)); puts_(" C "); putn((int32_t)(sc / ns)); }
                        delay_ms(200); h2 = hall(); all_off();
                        puts_("hold vector "); putn(hv); puts_(" -> hall "); putn((int32_t)h1); putc_('/'); putn((int32_t)h2); puts_("  vm_mV="); putn(counts_to_mv(vm)); puts_("  nFAULT low "); putn((int32_t)((nf * 1000ULL) / (nn ? nn : 1))); puts_(" per mille of the hold\r\n"); hv = (hv + 1) % 6; } break;
            case 'v': status(); break;
            case 'A': { /* PA4 voltage meter, outputs disabled throughout. */
                        uint32_t raw, sum = 0, lo = 4095, hi = 0;
                        stop(); drv_off(true);
                        for (uint32_t k = 0; k < 128; k++) {
                            DL_ADC_forceSequencer(ADC_B_INST, ADC_B_SEQ1); delay_ms(1);
                            raw = DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER2);
                            sum += raw; if (raw < lo) lo = raw; if (raw > hi) hi = raw;
                        }
                        puts_("PA4 ADC input (not an ILIMIT measurement until jumper connected): ");
                        putkv("mean_mV=", adc_mv(sum / 128)); putkv("min_mV=", adc_mv(lo)); putkv("max_mV=", adc_mv(hi));
                        puts_("DAC output disabled; DRVOFF high\r\n");
                    } break;
            case 'C': { /* Cross-check the new SOC against ISENA internally; no external pin drive. */
                        uint32_t ref = 0, meter = 0;
                        stop(); drv_off(true);
                        DL_ADC_selectSOCChannel(ADC_B_INST, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN14);
                        delay_ms(2);
                        for (uint32_t k = 0; k < 128; k++) {
                            DL_ADC_forceSequencer(ADC_B_INST, ADC_B_SEQ1); delay_ms(1);
                            ref += DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER0);
                            meter += DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER2);
                        }
                        DL_ADC_selectSOCChannel(ADC_B_INST, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN17);
                        delay_ms(2);
                        puts_("ADC crosscheck ISENA internal: ");
                        putkv("SOC0_mV=", adc_mv(ref / 128)); putkv("SOC2_mV=", adc_mv(meter / 128));
                        puts_("PA4 channel restored; DAC disabled; DRVOFF high\r\n");
                        /* PA4/AIN10 is also ADC2 channel18. Preserve phase-voltage SOC2. */
                        ref = meter = 0;
                        DL_ADC_selectSOCChannel(ADC_C_INST, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN18);
                        delay_ms(2);
                        for (uint32_t k = 0; k < 128; k++) {
                            DL_ADC_forceSequencer(ADC_B_INST, ADC_B_SEQ1);
                            DL_ADC_forceSequencer(ADC_C_INST, ADC_C_SEQ1); delay_ms(1);
                            ref += DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER2);
                            meter += DL_ADC_readResult(ADC_C_RESULT_INST, DL_ADC_SOC_NUMBER2);
                        }
                        DL_ADC_selectSOCChannel(ADC_C_INST, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN30);
                        delay_ms(2);
                        puts_("PA4 dual ADC crosscheck: "); putkv("ADC1_mV=", adc_mv(ref / 128));
                        putkv("ADC2_mV=", adc_mv(meter / 128)); puts_("ADC2 channel30 restored; DRVOFF high\r\n");
                    } break;
            case 'J': { /* Software-only acquisition sweep; no external pin is driven. */
                        static const uint32_t windows[] = {15U, 1472U};
                        uint32_t seq_b = ADC_B_INST->ADCSEQ1CONFIG, seq_c = ADC_C_INST->ADCSEQ1CONFIG;
                        uint32_t soc_b = ADC_B_INST->ADCSOC2CTL, soc_c = ADC_C_INST->ADCSOC2CTL;
                        stop(); drv_off(true);
                        DL_ADC_setupSequencer(ADC_B_INST, ADC_B_SEQ1, 15U, DL_ADC_TRIGGER_SOFTWARE, DL_ADC_SOC_NUMBER0);
                        DL_ADC_setupSequencer(ADC_C_INST, ADC_C_SEQ1, 15U, DL_ADC_TRIGGER_SOFTWARE, DL_ADC_SOC_NUMBER0);
                        delay_ms(2); /* Drain earlier timer-triggered conversions before channel changes. */
                        puts_("ADC isolated sweep: software triggers only; DAC disabled; DRVOFF high\r\n");
                        for (uint32_t w = 0; w < 2U; w++) {
                            DL_ADC_setupSequencer(ADC_B_INST, ADC_B_SEQ1, windows[w], DL_ADC_TRIGGER_SOFTWARE, DL_ADC_SOC_NUMBER0);
                            DL_ADC_setupSequencer(ADC_C_INST, ADC_C_SEQ1, windows[w], DL_ADC_TRIGGER_SOFTWARE, DL_ADC_SOC_NUMBER0);
                            for (uint32_t mode = 0; mode < 3U; mode++) {
                                uint32_t sum = 0, lo = 4095, hi = 0, ref = 0, other = 0;
                                DL_ADC_selectSOCChannel(ADC_B_INST, DL_ADC_SOC_NUMBER2,
                                    mode == 0U ? DL_ADC_CH_ADCIN14 : DL_ADC_CH_ADCIN17);
                                DL_ADC_selectSOCChannel(ADC_C_INST, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN18);
                                for (uint32_t k = 0; k < 80U; k++) {
                                    DL_ADC_forceSequencer(ADC_B_INST, ADC_B_SEQ1);
                                    if (mode == 2U) DL_ADC_forceSequencer(ADC_C_INST, ADC_C_SEQ1);
                                    delay_ms(1);
                                    uint32_t raw = DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER2);
                                    if (k < 16U) continue; /* Discard initial settling acquisitions. */
                                    sum += raw; if (raw < lo) lo = raw; if (raw > hi) hi = raw;
                                    ref += DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER0);
                                    if (mode == 2U) other += DL_ADC_readResult(ADC_C_RESULT_INST, DL_ADC_SOC_NUMBER2);
                                }
                                putkv("window_cycles=", windows[w]); putkv("mode=", mode);
                                puts_(mode == 0U ? "ISENA " : (mode == 1U ? "PA4_single " : "PA4_dual "));
                                putkv("mean_mV=", adc_mv(sum / 64U)); putkv("min_mV=", adc_mv(lo));
                                putkv("max_mV=", adc_mv(hi)); putkv("ISENA_ref_mV=", adc_mv(ref / 64U));
                                if (mode == 2U) putkv("ADC2_mV=", adc_mv(other / 64U));
                                puts_("\r\n");
                            }
                        }
                        ADC_B_INST->ADCSOC2CTL = soc_b; ADC_C_INST->ADCSOC2CTL = soc_c;
                        ADC_B_INST->ADCSEQ1CONFIG = seq_b; ADC_C_INST->ADCSEQ1CONFIG = seq_c;
                        puts_("ADC configuration restored; motor remains disabled\r\n");
                    } break;
            case 'B': { /* Check whether the fault input is driven or merely floating. */
                        uint32_t old = IOMUX->SECCFG.PINCM[IOMUX_PINCM_PB4];
                        stop(); drv_off(true);
                        puts_("PB4 weak-pull check, bridge disabled: ");
                        for (uint32_t k = 0; k < 3; k++) {
                            uint32_t pull = k == 0 ? IOMUX_PINCM_PIPD_ENABLE : (k == 1 ? IOMUX_PINCM_PIPU_ENABLE : 0U);
                            IOMUX->SECCFG.PINCM[IOMUX_PINCM_PB4] = (old & ~(IOMUX_PINCM_PIPD_MASK | IOMUX_PINCM_PIPU_MASK)) | pull;
                            delay_ms(2);
                            puts_(k == 0 ? "down=" : (k == 1 ? "up=" : "none="));
                            putn(DL_GPIO_readPins(DRVB_PORT, DRVB_NFAULT_ALT_PIN) != 0U); putc_(' ');
                        }
                        IOMUX->SECCFG.PINCM[IOMUX_PINCM_PB4] = old;
                        puts_("\r\n");
                    } break;
            case 'F': puts_("fault inputs raw (0=low): PA22 ");
                      putn(DL_GPIO_readPins(DRVA_PORT, DRVA_NFAULT_PIN) != 0U);
                      puts_(" PB4 "); putn(DL_GPIO_readPins(DRVB_PORT, DRVB_NFAULT_ALT_PIN) != 0U);
                      puts_("; PB4 drives fault shutdown\r\n");
                      { uint32_t old = IOMUX->SECCFG.PINCM[IOMUX_PINCM_PB18];
                        DL_GPIO_setInputEnableBit(IOMUX_PINCM_PB18, DL_GPIO_INPUT_ENABLE);
                        putkv("DRVOFF commanded ", (DRVB_PORT->DOUT31_0 & DRVB_DRVOFF_PIN) != 0U);
                        putkv("pad ", DL_GPIO_readPins(DRVB_PORT, DRVB_DRVOFF_PIN) != 0U);
                        IOMUX->SECCFG.PINCM[IOMUX_PINCM_PB18] = old; puts_("\r\n"); }
                      break;
            case 'U': { /* Bounded 1% pulse train: compare unloaded high side and line vectors. */
                        uint32_t t0, elapsed, ia, ib, ic;
                        stop();
                        for (int v = 0; v < 9; v++) {
                            drv_off(true); all_off(); drv_sleep(true); delay_ms(1);
                            drv_sleep(false); delay_ms(6);
                            putkv("pulse test ", v); putkv("reset_fault ", drv_fault());
                            if (drv_fault()) { puts_("skipped\r\n"); break; }
                            drv_off(false); delay_ms(1);
                            if (drv_fault()) { drv_off(true); puts_("idle fault; skipped\r\n"); break; }
                            t0 = cyc();
                            if (v < 3) phase_pwm(v, 1); else vector(v - 3, 1);
                            while (!drv_fault() && cyc() - t0 < CPU_HZ / 1000U) {}
                            elapsed = cyc() - t0; int f = drv_fault();
                            isen_read(&ia, &ib, &ic); all_off(); drv_off(true);
                            putkv("fault ", f); putkv("elapsed_cycles ", elapsed);
                            putkv("sense_A_mV ", adc_mv(ia)); putkv("B ", adc_mv(ib)); putkv("C ", adc_mv(ic)); puts_("\r\n");
                        }
                        g_fault = drv_fault();
                    } break;
            case 'N': { /* A single low-side switch; no high-side drive. */
                        uint32_t t0, elapsed, ia, ib, ic;
                        stop();
                        for (int p = 0; p < 3; p++) {
                            drv_off(true); all_off(); drv_sleep(true); delay_ms(1);
                            drv_sleep(false); delay_ms(6);
                            putkv("single low phase ", p); putkv("reset_fault ", drv_fault());
                            if (drv_fault()) { puts_("skipped\r\n"); break; }
                            drv_off(false); delay_ms(1);
                            putkv("idle_fault ", drv_fault());
                            if (drv_fault()) { drv_off(true); puts_("skipped\r\n"); break; }
                            t0 = cyc(); phase_low(p);
                            while (!drv_fault() && cyc() - t0 < CPU_HZ / 1000U) {}
                            elapsed = cyc() - t0; int f = drv_fault();
                            isen_read(&ia, &ib, &ic); all_off(); drv_off(true);
                            putkv("fault ", f); putkv("elapsed_cycles ", elapsed);
                            putkv("sense_A_mV ", adc_mv(ia)); putkv("B ", adc_mv(ib)); putkv("C ", adc_mv(ic)); puts_("\r\n");
                        }
                        g_fault = drv_fault();
                    } break;
            case 'P': { /* Observe MCU pads with bridge disabled throughout. */
                        static const uint32_t pins[6] = {IOMUX_PINCM_PB3, IOMUX_PINCM_PA7,
                            IOMUX_PINCM_PB1, IOMUX_PINCM_PA2, IOMUX_PINCM_PB2, IOMUX_PINCM_PA25};
                        static const uint32_t duties[3] = {1, 10, 40};
                        uint32_t saved[6], counts[6], n, t0, t, b, a, start, longest, was_high;
                        stop(); drv_off(true);
                        for (i = 0; i < 6; i++) { saved[i] = IOMUX->SECCFG.PINCM[pins[i]];
                            DL_GPIO_setInputEnableBit(pins[i], DL_GPIO_INPUT_ENABLE); }
                        puts_("PWM pad check: DRVOFF high; AH/AL/BH/BL/CH/CL high per mille\r\n");
                        for (uint32_t p = 0; p < 3; p++) for (uint32_t k = 0; k < 3; k++) {
                            all_off(); delay_ms(1);
                            for (i = 0; i < 6; i++) counts[i] = 0;
                            n = longest = start = was_high = 0; t0 = cyc();
                            vector(2 * p, duties[k]);
                            do {
                                t = cyc() - t0; b = DRVB_PORT->DIN31_0; a = DRVA_PORT->DIN31_0;
                                uint32_t high = (b >> (p == 0 ? 3 : (p == 1 ? 1 : 2))) & 1U;
                                counts[0] += (b >> 3) & 1U; counts[1] += (a >> 7) & 1U;
                                counts[2] += (b >> 1) & 1U; counts[3] += (a >> 2) & 1U;
                                counts[4] += (b >> 2) & 1U; counts[5] += (a >> 25) & 1U;
                                if (high && !was_high) start = t;
                                if (!high && was_high && t - start > longest) longest = t - start;
                                was_high = high; n++;
                            } while (t < CPU_HZ / 50U);
                            all_off(); putkv("phase ", p); putkv("duty ", duties[k]);
                            for (i = 0; i < 6; i++) { putn((counts[i] * 1000U) / n); putc_('/'); }
                            putkv(" longest_PWM_high_cycles ", longest); puts_("\r\n");
                        }
                        all_off();
                        for (i = 0; i < 6; i++) IOMUX->SECCFG.PINCM[pins[i]] = saved[i];
                    } break;
            case 'D': { /* Short fixed-vector duty sweep in the live 3x encoding. */
                        static const uint32_t duty[4] = {10U, 20U, 30U, 35U};
                        uint32_t old_mode = g_mode3x, ia, ib, ic, k;
                        stop();
                        if (drv_fault()) { puts_("duty check skipped: driver fault\r\n"); break; }
                        g_mode3x = 1; drv_off(false);
                        for (k = 0; k < 4U; k++) {
                            int64_t sum = 0;
                            vector(0, duty[k]); delay_ms(100);
                            for (i = 0; i < 500; i++) {
                                isen_read(&ia, &ib, &ic);
                                sum += (int32_t)adc_mv(ia) - adc_mv(ib);
                                DEVICE_DELAY_US(50);
                            }
                            all_off();
                            puts_("duty "); putn((int32_t)duty[k]); puts_("% ISENA-ISENB mV ");
                            putn((int32_t)(sum / 500)); puts_(" nFAULT low "); putn(drv_fault()); puts_("\r\n");
                            if (drv_fault()) break;
                            delay_ms(75);
                        }
                        all_off(); g_mode3x = old_mode;
                    } break;
            case 'e': { /* Driver-enable pin check with all PWM outputs floating. */
                        stop(); drv_off(true); drv_sleep(true); delay_ms(1);
                        drv_sleep(false); delay_ms(6); drv_off(false); delay_ms(1);
                        puts_("enable pins (output/read/enable): nSLEEP ");
                        putn((DRVA_PORT->DOUT31_0 & DRVA_NSLEEP_PIN) != 0U); putc_('/');
                        putn(DL_GPIO_readPins(DRVA_PORT, DRVA_NSLEEP_PIN) != 0U); putc_('/');
                        putn((DRVA_PORT->DOE31_0 & DRVA_NSLEEP_PIN) != 0U);
                        puts_(" DRVOFF ");
                        putn((DRVB_PORT->DOUT31_0 & DRVB_DRVOFF_PIN) != 0U); putc_('/');
                        putn(DL_GPIO_readPins(DRVB_PORT, DRVB_DRVOFF_PIN) != 0U); putc_('/');
                        putn((DRVB_PORT->DOE31_0 & DRVB_DRVOFF_PIN) != 0U);
                        puts_(" nFAULT low "); putn(drv_fault()); puts_("\r\n"); } break;
            case 'T': { static const uint8_t T[8] = {0,3,2,5,4,0,1,0}; for (i = 1; i < 7; i++) { g_fwd[i] = T[i]; g_rev[i] = (uint8_t)((T[i] + 3) % 6); } learned = true; puts_("built-in table loaded (the one that spun on 2026-10-15)\r\n"); } break;
            case 'W': { /* pulse bursts on A with B low. Pulses under the 1.8 us current-limit blanking cannot be cut by the limit;
                           longer ones can. Motor open: every burst reads zero. Limit at zero amps: short bursts carry current, long ones hardly any. */
                        static const uint32_t ON[4] = {1, 2, 5, 20}; uint32_t ia, ib, ic, j, k, n; int64_t sa, sb, pk;
                        stop(); drv_off(false); drv_sleep(false); delay_ms(5);
                        puts_("burst on/off us : mean ISEN B mV from rest (peak), mean ISEN A, samples\r\n");
                        for (k = 0; k < 4; k++) {
                            phase_off(PH_C); phase_low(PH_B); phase_low(PH_A); delay_ms(5); phase_off(PH_A); sa = sb = pk = 0; n = 0;   /* A low first so its bootstrap is charged */
                            for (j = 0; j < 300U / (2U * ON[k]); j++) {
                                sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); DEVICE_DELAY_US(ON[k]); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
                                if ((j & 3U) == 3U) { int32_t v; isen_force(&ia, &ib, &ic); v = adc_mv(ib) - g_rest[1]; sb += v; if (v > pk) pk = v; sa += adc_mv(ia) - g_rest[0]; n++; }
                                else DEVICE_DELAY_US(ON[k]);
                            }
                            all_off();
                            putn((int32_t)ON[k]); puts_(" : "); putn((int32_t)(n ? sb / n : 0)); puts_(" ("); putn((int32_t)pk); puts_(") "); putn((int32_t)(n ? sa / n : 0)); putc_(' '); putn((int32_t)n); puts_(" nFAULT low "); putn(drv_fault() ? 1 : 0); puts_("\r\n");
                            delay_ms(50);
                        } } break;
            case 'Q': { /* driver A/B, hands free: does the high side stay on with no current path, and with one? */
                        uint32_t a, vm, ia, ib, ic, j; int32_t pins; stop(); drv_off(false); drv_sleep(false); delay_ms(5);
                        pins = (int32_t)((DL_GPIO_readPins(DRVA_PORT, DRVA_NSLEEP_PIN) ? 1 : 0) | (DL_GPIO_readPins(DRVB_PORT, DRVB_DRVOFF_PIN) ? 2 : 0) | (drv_fault() ? 4 : 0));
                        puts_("after wake: nSLEEP "); putn(pins & 1); puts_(" DRVOFF "); putn((pins >> 1) & 1); puts_(" nFAULT low "); putn((pins >> 2) & 1); puts_("\r\n");
                        /* 1: A high, B and C floating: no current can flow */
                        phase_off(PH_B); phase_off(PH_C); phase_low(PH_A); delay_ms(5); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
                        puts_("A high, B C open. VSENA mV at 20us 100us 1ms 10ms: ");
                        DEVICE_DELAY_US(20); adc_read(&a, &vm); putn(counts_to_mv(a)); putc_(' ');
                        DEVICE_DELAY_US(80); adc_read(&a, &vm); putn(counts_to_mv(a)); putc_(' ');
                        delay_ms(1); adc_read(&a, &vm); putn(counts_to_mv(a)); putc_(' ');
                        delay_ms(9); adc_read(&a, &vm); putn(counts_to_mv(a)); puts_(" nFAULT low "); putn(drv_fault() ? 1 : 0); puts_("\r\n");
                        all_off(); delay_ms(50);
                        /* 2: A high, B low: current through the winding, 300 us only (about 1.5 A if nothing limits it) */
                        phase_off(PH_C); phase_low(PH_A); phase_low(PH_B); delay_ms(5); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
                        puts_("A high, B low. VSENA mV / ISEN A B mV from rest, at 20us 100us 300us: ");
                        for (j = 0; j < 3; j++) { DEVICE_DELAY_US(j == 0 ? 20 : (j == 1 ? 60 : 180)); adc_read(&a, &vm); isen_read(&ia, &ib, &ic);
                          putn(counts_to_mv(a)); putc_('/'); putn(adc_mv(ia) - g_rest[0]); putc_('/'); putn(adc_mv(ib) - g_rest[1]); puts_("  "); }
                        puts_(" nFAULT low "); putn(drv_fault() ? 1 : 0); puts_("\r\n"); all_off();
                        delay_ms(2); puts_("after: nFAULT low "); putn(drv_fault() ? 1 : 0); puts_(" VM "); putn(counts_to_mv(vm)); puts_(" mV\r\n"); } break;
            case 'x': { static const uint32_t P[5] = {10, 30, 50, 80, 100}; uint32_t a, vm, j; stop();
                        for (j = 0; j < 5; j++) { phase_off(PH_B); phase_off(PH_C); phase_low(PH_A); delay_ms(5); phase_pwm(PH_A, P[j]); delay_ms(30); adc_read(&a, &vm);
                          puts_("A at "); putn((int32_t)P[j]); puts_("%: VSENA="); putn(counts_to_mv(a)); puts_(" mV of VM="); putn(counts_to_mv(vm)); puts_(" mV; CMPA now "); putn((int32_t)DL_MCPWM_getCounterCompareActiveValue(MOTOR_PWM_INST, DL_MCPWM_COUNTER_COMPARE_1A)); puts_("\r\n"); }
                        all_off(); } break;
            case 'y': { uint32_t a, vm, j; static const uint32_t W[5] = {50, 100, 150, 200, 500}; stop();
                        puts_("DRVOFF pin "); putn((int32_t)(DL_GPIO_readPins(DRVB_PORT, DRVB_DRVOFF_PIN) ? 1 : 0)); puts_(" nSLEEP pin "); putn((int32_t)(DL_GPIO_readPins(DRVA_PORT, DRVA_NSLEEP_PIN) ? 1 : 0)); puts_(" nFAULT "); putn((int32_t)(drv_fault() ? 0 : 1)); puts_("\r\n");
                        drv_off(false); drv_sleep(false); delay_ms(5);
                        puts_("TBCTR "); putn((int32_t)DL_MCPWM_getTimeBaseCounterValue(MOTOR_PWM_INST)); putc_(' '); putn((int32_t)DL_MCPWM_getTimeBaseCounterValue(MOTOR_PWM_INST)); putc_(' '); putn((int32_t)DL_MCPWM_getTimeBaseCounterValue(MOTOR_PWM_INST)); puts_("\r\n");
                        phase_off(PH_B); phase_off(PH_C); phase_low(PH_A); delay_ms(500); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
                        puts_("A forced high, VSENA mV at 50,150,300,500,1000 ms: ");
                        for (j = 0; j < 5; j++) { delay_ms(W[j]); adc_read(&a, &vm); putn(counts_to_mv(a)); putc_(' '); } puts_("\r\n");
                        phase_low(PH_A); delay_ms(500); phase_pwm(PH_A, 50);
                        puts_("A at 50% PWM, VSENA mV at 50,150,300,500,1000 ms: ");
                        for (j = 0; j < 5; j++) { delay_ms(W[j]); adc_read(&a, &vm); putn(counts_to_mv(a)); putc_(' '); } puts_("\r\n");
                        all_off(); } break;
            case 'z': { uint32_t a, vm; stop(); drv_off(false); drv_sleep(false); delay_ms(5);
                        puts_("order swapped: ");
                        sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400);
                        sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400); adc_read(&a, &vm); puts_("3x code first="); putn(counts_to_mv(a));
                        sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400);
                        sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW);  sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400); adc_read(&a, &vm); puts_("  6x code second="); putn(counts_to_mv(a));
                        sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_LOW); sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400);
                        sw(INL[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); sw(INH[PH_A], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH); delay_ms(400); adc_read(&a, &vm); puts_("  3x code again="); putn(counts_to_mv(a));
                        puts_(" mV\r\n"); all_off(); } break;
            case 'M': g_mode3x = g_mode3x ? 0 : 1; putkv("mode3x ", (int32_t)g_mode3x); puts_("\r\n"); break;
            case 'j': {  /* Hall sector widths at steady speed: time spent in each state over 50 electrical turns, as percent of a turn (ideal 16.7 each) */
                        uint32_t t0, h, last, tl, n = 0, sum[8] = {0}, cnt[8] = {0}, tot = 0;
                        if (!running) { puts_("run the motor first (g)\r\n"); break; }
                        last = hall(); tl = cyc(); t0 = tl;
                        while (n < 300U && cyc() - t0 < 5U * CPU_HZ) {
                            h = hall(); commutate(h, (uint32_t)g_act);
                            if (h != last) { uint32_t now = cyc(); if (n >= 6U) { sum[last] += now - tl; cnt[last]++; tot += now - tl; } tl = now; last = h; n++; }
                        }
                        puts_("hall sector widths, percent of an electrical turn (ideal 16.7): ");
                        for (i = 1; i < 7; i++) { putn(i); putc_(':'); putn(tot ? (int32_t)((sum[i] * 1000ULL) / tot / 10) : 0); putc_('.'); putn(tot ? (int32_t)((sum[i] * 1000ULL) / tot % 10) : 0); putc_(' '); }
                        puts_(" edges "); putn((int32_t)n); puts_("\r\n"); } break;
            case 'S': {  /* startup statistics: 20 starts from standstill at g_duty, time to 24 Hall edges (one shaft turn), 3 s limit each */
                        uint32_t k, ok = 0, tsum = 0, tmax = 0, tmin = 0xFFFFFFFFU; stop(); if (!learned) { puts_("learn first (l)\r\n"); break; }
                        if (g_duty == 0) g_duty = 10;
                        puts_("start test: 20 starts at "); putn(g_duty); puts_("%\r\n");
                        for (k = 0; k < 20U; k++) {
                            uint32_t t0 = cyc(), h, sectors = 0; int32_t act = 0; uint32_t tr = t0, done = 0; int sector_dir = 0;
                            drv_off(false); g_hall = hall();
                            while (cyc() - t0 < 3U * CPU_HZ) {
                                h = hall(); if (h != g_hall) { sectors = track_hall(g_hall, h, &sector_dir, sectors); g_hall = h; }
                                if (cyc() - tr > (CPU_HZ / 1000U) * RAMP_MS && act < g_duty) { act++; tr = cyc(); }
                                commutate(h, (uint32_t)act);
                                if (sectors >= 24U) { done = (cyc() - t0) / (CPU_HZ / 1000U); break; }
                            }
                            all_off();
                            if (done) { ok++; tsum += done; if (done > tmax) tmax = done; if (done < tmin) tmin = done; }
                            delay_ms(700);   /* let it coast to a stop */
                            while (1) { uint32_t e1 = g_edges, hh; delay_ms(300); hh = hall(); if (hh != g_hall) { g_hall = hh; g_edges++; } if (g_edges == e1) break; }
                        }
                        puts_("starts ok "); putn((int32_t)ok); puts_(" of 20; one turn in "); if (ok) { putn((int32_t)(tsum / ok)); puts_(" ms mean, "); putn((int32_t)tmin); puts_(" to "); putn((int32_t)tmax); }
                        puts_("\r\n"); } break;
            case 'O': {  /* open-loop start, no sensor: align on vector 0, then step the vector round at a rising rate up to g_ol_hz over g_ol_ms, at g_duty; the Halls only judge */
                        uint32_t t0, tstep, k = 0, h, last, ok, sectors = 0; int v = 0, sector_dir = 0; stop();
                        if (g_duty == 0) g_duty = 15;
                        puts_("open-loop start: align, ramp to "); putn((int32_t)g_ol_hz); puts_(" Hz electrical over "); putn((int32_t)g_ol_ms); puts_(" ms at "); putn(g_duty); puts_("%\r\n");
                        drv_off(false); vector(0, (uint32_t)g_duty); delay_ms(400);
                        last = hall(); t0 = cyc(); tstep = t0;
                        while (1) {
                            uint32_t el = (cyc() - t0) / (CPU_HZ / 1000U); uint32_t f = el >= g_ol_ms ? g_ol_hz : (g_ol_hz * el) / g_ol_ms;   /* Hz, electrical */
                            uint32_t step_cyc = f ? CPU_HZ / (6U * f) : CPU_HZ; uint32_t dd = (uint32_t)g_duty + (g_vf * f) / 100U; if (dd > (uint32_t)MAX_DUTY) dd = MAX_DUTY;
                            if (cyc() - tstep >= step_cyc) { tstep += step_cyc; v = (v + 1) % 6; vector(v, dd); k++; }
                            h = hall(); if (h != last) { sectors = track_hall(last, h, &sector_dir, sectors); last = h; }
                            if (el >= g_ol_ms + 1000U) break;
                        }
                        /* the last second at the target: Hall edges per second against the commanded 6 x f */
                        { uint32_t t1 = cyc(), dd = (uint32_t)g_duty + (g_vf * g_ol_hz) / 100U; if (dd > (uint32_t)MAX_DUTY) dd = MAX_DUTY;
                          sectors = 0; sector_dir = 0;
                          while (cyc() - t1 < CPU_HZ) { h = hall(); if (h != last) { sectors = track_hall(last, h, &sector_dir, sectors); last = h; } if (cyc() - tstep >= CPU_HZ / (6U * g_ol_hz)) { tstep += CPU_HZ / (6U * g_ol_hz); v = (v + 1) % 6; vector(v, dd); } }
                          ok = sectors;
                          /* hand over to Hall commutation at the same duty for one second, if the table is learned */
                          if (learned && ok >= (6U * g_ol_hz * 9U) / 10U && ok <= (6U * g_ol_hz * 11U) / 10U) {
                            uint32_t t2 = cyc(); sectors = 0; sector_dir = 0;
                            while (cyc() - t2 < CPU_HZ) { h = hall(); if (h != last) { sectors = track_hall(last, h, &sector_dir, sectors); last = h; } commutate(h, dd); }
                            puts_("  handed over to Halls at "); putn((int32_t)dd); puts_("%: "); putn((int32_t)((sectors * 60U) / 24U)); puts_(" rpm\r\n"); } }
                        all_off();
                        puts_("  commanded "); putn((int32_t)(6U * g_ol_hz)); puts_(" edges/s, Halls saw "); putn((int32_t)ok); puts_(" edges/s in the last second ("); putn((int32_t)((ok * 60U) / 24U)); puts_(" rpm): "); puts_(ok >= (6U * g_ol_hz * 9U) / 10U && ok <= (6U * g_ol_hz * 11U) / 10U ? "FOLLOWED" : "LOST"); puts_("\r\n"); } break;
            case 'V': g_vf = g_vf >= 40U ? 0U : g_vf + 10U; putkv("V/f: extra duty percent per 100 Hz ", (int32_t)g_vf); puts_("\r\n"); break;
            case 'H': g_ol_hz = g_ol_hz >= 200U ? 25U : g_ol_hz + 25U; putkv("open-loop target Hz ", (int32_t)g_ol_hz); puts_("\r\n"); break;
            case 'R': g_ol_ms = g_ol_ms >= 4000U ? 500U : g_ol_ms + 500U; putkv("open-loop ramp ms ", (int32_t)g_ol_ms); puts_("\r\n"); break;
            case 'm': stop(); sine_mode = true;  puts_("sine modulation (Hall-interpolated angle)\r\n"); break;
            case 'n': stop(); sine_mode = false; puts_("six-step\r\n"); break;
            case 'p': speed_mode = true;  sp_i = 0; puts_("speed mode: 1 to 4 = 500 to 2000 rpm target, then g; w prints the step log\r\n"); break;
            case 'u': speed_mode = false; sp_i = 0; puts_("duty mode\r\n"); break;
            case 'k': sp_kp = sp_kp >= 8 ? 1 : sp_kp + 1; putkv("Kp ", sp_kp); puts_("\r\n"); break;
            case 'K': sp_ki = sp_ki >= 8 ? 1 : sp_ki + 1; putkv("Ki ", sp_ki); puts_("\r\n"); break;
            case 'E': { /* Timestamped trace dump only with drive disabled. */
                        if (running) { puts_("stop before trace dump\r\n"); break; }
                        drv_off(true);
                        putkv("trace target=", g_trace_target); puts_(g_trace_sine ? "mode=sine " : "mode=6step ");
                        puts_(g_trace_dir > 0 ? "dir=f " : "dir=r ");
                        puts_("count="); putn((int32_t)g_tracen); puts_("\r\n");
                        for (uint32_t k = 0; k < g_tracen; k++) {
                            putkv("sample us=", (int32_t)g_trace[k].us); putkv("rpm=", g_trace[k].rpm);
                            putkv("duty=", g_trace[k].duty); puts_("edges="); putn((int32_t)g_trace[k].edges); puts_("\r\n");
                        }
                        puts_("trace end\r\n");
                    } break;
            case 'w': { if (running) { puts_("stop before trace dump\r\n"); break; } uint32_t n; puts_("rpm every 10 ms: "); for (n = 0; n < g_logn; n++) { putn(g_log[n]); putc_(' '); } puts_("\r\n"); } break;
            case 'i': {  /* currents: 1 s of samples at the PWM rate, mean and rms about the rest level, per phase, in mA at 1 V/A */
                        uint32_t t0 = cyc(), n = 0, a, b, c; int64_t sa = 0, sb = 0, sc = 0, qa = 0, qb = 0, qc = 0; int32_t ma, mb, mc;
                        if (!running) {   /* at rest: the zero-current level, which is VREF/2 */
                            delay_ms(20); while (cyc() - t0 < CPU_HZ / 5U) { isen_read(&a, &b, &c); sa += adc_mv(a); sb += adc_mv(b); sc += adc_mv(c); n++; delay_ms(1); }
                            g_rest[0] = (int32_t)(sa / n); g_rest[1] = (int32_t)(sb / n); g_rest[2] = (int32_t)(sc / n);
                            puts_("rest level mV (VREF/2): A "); putn(g_rest[0]); puts_(" B "); putn(g_rest[1]); puts_(" C "); putn(g_rest[2]); puts_("  -> VREF about "); putn(g_rest[1] * 2); puts_(" mV\r\n"); break; }
                        while (cyc() - t0 < CPU_HZ) { h = hall(); g_hall = h; commutate(h, (uint32_t)g_act);
                            isen_read(&a, &b, &c); ma = adc_mv(a) - g_rest[0]; mb = adc_mv(b) - g_rest[1]; mc = adc_mv(c) - g_rest[2];
                            sa += ma; sb += mb; sc += mc; qa += (int64_t)ma * ma; qb += (int64_t)mb * mb; qc += (int64_t)mc * mc; n++; DEVICE_DELAY_US(30); }
                        { int32_t ra, rb, rc; uint32_t r; 
                          /* integer square roots */
                          for (r = 0; (int64_t)r * r < qa / (int64_t)n; r++) {} ra = (int32_t)r; for (r = 0; (int64_t)r * r < qb / (int64_t)n; r++) {} rb = (int32_t)r; for (r = 0; (int64_t)r * r < qc / (int64_t)n; r++) {} rc = (int32_t)r;
                          puts_("phase current mA (1 V/A), mean/rms: A "); putn((int32_t)(sa / n)); putc_('/'); putn(ra); puts_("  B "); putn((int32_t)(sb / n)); putc_('/'); putn(rb); puts_("  C "); putn((int32_t)(sc / n)); putc_('/'); putn(rc);
                          puts_("  samples "); putn((int32_t)n); puts_("  rpm "); putn((int32_t)g_rpm); puts_("\r\n"); } } break;
            case 'q': { uint32_t n = g_hn < 16U ? g_hn : 16U; puts_("last hall states: "); for (i = 0; i < (int)n; i++) { putn((int32_t)g_hist[(g_hn - n + (uint32_t)i) % 16U]); putc_(' '); } puts_("\r\n"); } break;
            case '?': help(); status(); break;
            default: break;
        } }
    }
}
