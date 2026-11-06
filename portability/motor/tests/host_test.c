/* Host test for portability/motor/core. Build and run: ./host_test.sh (prints OK or the failing check).

   WHAT IT CHECKS
   1. The kernel pieces (sincos, Clarke, Park, inverse Park, PI, min-max modulation, ramp, sqrt) in the number format of
      this build, against double precision.
   2. A current step and a Hall-tracked speed run, the controller in this build's number format closed around a minimal
      PMSM test plant, against a textbook double-precision controller closed around its own copy of the plant. The plant
      only lets the two number formats be compared with each other; it predicts nothing about a motor, and engineering
      predictions come from the trusted solvers, not from this file.
   3. The Hall edge tracker on scripted Hall sequences (direction, standstill, bounce, invalid states, overdue edge).
   4. The guard (limits, refusals, every trip) and the cost stamps.

   PLANT VALUES (bench values, the BLY171S-class motor on the AM13 bench; the test needs a plausible machine, not a
   claim about that motor): R 0.75 ohm, L 1.05 mH, flux 4.74e-3 Wb, 4 pole pairs, J 2.4e-6 kg m2, 20 V bus, 20 kHz ISR.

   EXPECTED BOUNDS, written before the first run (2026-10-19). Why each is what it is:
     sincos, 1,000 angles, max |error| against double:
       float + libm 2e-6 (float rounding of sinf and the angle); float + table 1.2e-4 and IQ24 + table 1.2e-4 (linear
       interpolation in 256 steps: (2 pi/256)^2/8 = 7.5e-5, plus rounding); Q15 + table 2.5e-4 (the same plus 3e-5 steps).
     Clarke/Park/inverse Park round trip, unit-amplitude input, max |error|: float + libm 1e-5; IQ24 + libm 5e-5;
       table builds 2.5e-4 (sin^2 + cos^2 is up to 1.5e-4 low with the table, applied twice); Q15 4e-4.
       [The first draft of this line gave the libm figures for the table builds too; the first run showed 1.4e-4 for
       float + table, and the line was corrected to this one.]
     Current step (iq 0 to 2 A at 1 ms, rotor driven at 200 Hz electrical, 10 ms), max |iq - iq_ref| and |id - id_ref|,
       the double controller being the reference:
         float + libm 1e-3 A; float + table 4e-3 A; IQ24 + libm 2e-3 A; IQ24 + table 4e-3 A; Q15 + table 3e-2 A.
       The step must also settle: |iq - 2 A| < 2 % of the step over the last 3 ms (all formats, Q15 too: 0.04 A).
     Hall run (standstill to 100 Hz electrical, 20 ms ramp, speed loop at 2 kHz, 200 ms):
       tracker, steady window 120 to 200 ms: electrical angle error rms <= 2.0 degrees, max <= 4.0 degrees (edge timing
         jitter of +/- 0.5 ISR is +/- 0.9 degrees at 100 Hz; the speed estimate's +/- 3 % per-edge jitter adds up to 1.8
         degrees across a sector);
       per-edge speed estimate error against the true speed: signed mean within 0.5 %, worst magnitude <= 4.5 %;
       sign of the speed never negative after the second edge; the speed is within 2 % of 100 Hz from 80 ms on, on average;
       mean estimated speed in the window within 1 Hz of 100 Hz, and the true mean speed within 1.0 Hz (float), 1.5 Hz
         (IQ24) or 2.0 Hz (Q15) of the double controller's.
     Guard and tracker scripts: exact.
   A negative control (host_test.sh builds one with -DFRL_TEST_BREAK, a halved current-loop gain) must FAIL a bound. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "kernel.h"
#include "angle_hall.h"
#include "guard.h"
static uint32_t fake_cycles;
#define FRL_CYCLES() fake_cycles
#include "cost.h"

#define TWO_PI 6.283185307179586
static int g_fail = 0;
static void check(int ok, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void check(int ok, const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	printf("  %-5s ", ok ? "ok" : "WRONG"); vprintf(fmt, ap); printf("\n");
	va_end(ap);
	if (!ok) g_fail++;
}

/* ---- per-build bounds ------------------------------------------------------------------------------------------ */
#if FRL_NUM == FRL_NUM_FLOAT && FRL_TRIG == FRL_TRIG_LIBM
#define B_NAME "float+libm"
#define B_SINCOS 2e-6
#define B_RT 1e-5
#define B_STEP 1e-3
#define B_SPEED_MEAN 1.0
#elif FRL_NUM == FRL_NUM_FLOAT
#define B_NAME "float+table"
#define B_SINCOS 1.2e-4
#define B_RT 2.5e-4
#define B_STEP 4e-3
#define B_SPEED_MEAN 1.0
#elif FRL_NUM == FRL_NUM_IQ24 && FRL_TRIG == FRL_TRIG_LIBM
#define B_NAME "iq24+libm"
#define B_SINCOS 2e-6
#define B_RT 5e-5
#define B_STEP 2e-3
#define B_SPEED_MEAN 1.5
#elif FRL_NUM == FRL_NUM_IQ24
#define B_NAME "iq24+table"
#define B_SINCOS 1.2e-4
#define B_RT 2.5e-4
#define B_STEP 4e-3
#define B_SPEED_MEAN 1.5
#else
#define B_NAME "q15+table"
#define B_SINCOS 2.5e-4
#define B_RT 4e-4
#define B_STEP 3e-2
#define B_SPEED_MEAN 2.0
#endif
#if FRL_NUM == FRL_NUM_Q15 && FRL_TRIG != FRL_TRIG_TABLE
#error "the Q15 build is tested with the table backend"
#endif

/* ---- 1. kernel pieces ------------------------------------------------------------------------------------------ */
static void test_kernel(void)
{
	printf("kernel (%s)\n", B_NAME);
	double worst = 0;
	for (int k = 0; k < 1000; k++) {
		double a = (k + 0.37) / 1000.0; frl_real s, c;
		FRL_SINCOS(FRL_R(a), &s, &c);
		double e1 = fabs((double)FRL_TOF(s) - sin(TWO_PI * a)), e2 = fabs((double)FRL_TOF(c) - cos(TWO_PI * a));
		if (e1 > worst) worst = e1;
		if (e2 > worst) worst = e2;
	}
	check(worst <= B_SINCOS, "sincos max error %.2e (bound %.1e)", worst, B_SINCOS);

	worst = 0;   /* clarke -> park -> ipark -> alpha/beta back */
	for (int k = 0; k < 360; k += 7) {
		double th = k / 360.0, ph = 0.31;
		double ia = cos(TWO_PI * ph), ib = cos(TWO_PI * (ph - 1.0 / 3.0));
		frl_real s, c, al, be, d, q, al2, be2;
		FRL_SINCOS(FRL_R(th), &s, &c);
		frl_clarke(FRL_R(ia), FRL_R(ib), &al, &be);
		frl_park(al, be, s, c, &d, &q);
		frl_ipark(d, q, s, c, &al2, &be2);
		double e = fabs((double)FRL_TOF(al2) - ia);
		double eb = fabs((double)FRL_TOF(be2) - (ia + 2 * ib) / sqrt(3.0));
		if (e > worst) worst = e;
		if (eb > worst) worst = eb;
	}
	check(worst <= B_RT, "clarke/park/ipark round trip max error %.2e (bound %.1e)", worst, B_RT);

	frl_pi_t p = { FRL_R(1.0), FRL_R(0.1), 0, FRL_R(-2.0), FRL_R(2.0) };   /* conditional integration */
	for (int k = 0; k < 200; k++) (void)frl_pi(&p, FRL_R(1.0));
	check(FRL_TOF(p.integ) >= 0.99f && FRL_TOF(p.integ) <= 1.11f, "PI integrator frozen once the output saturates (%.3f, want 1.0 to 1.1)", (double)FRL_TOF(p.integ));
	for (int k = 0; k < 40; k++) (void)frl_pi(&p, FRL_R(-1.0));
	check(FRL_TOF(p.integ) < 1.9f, "PI unwinds when the error reverses (%.3f)", (double)FRL_TOF(p.integ));
	frl_real o = frl_pi(&p, FRL_R(-20.0));
	check(o == p.lo, "PI output clamps to the lower limit");

	worst = 0;   /* modulation: line-to-line volts reproduced, duties in 0..1 */
	int inrange = 1;
	for (int k = 0; k < 360; k += 5) {
		double th = TWO_PI * k / 360.0, V = 9.0, inv = 1.0 / 20.0;
		double va = V * cos(th), vb = V * cos(th - TWO_PI / 3), vc = V * cos(th + TWO_PI / 3);
		frl_real da, db, dc;
		frl_modulate(FRL_R(va), FRL_R(V * sin(th)), FRL_R(inv), &da, &db, &dc);
		double A = FRL_TOF(da), B = FRL_TOF(db), C = FRL_TOF(dc);
		if (A < 0 || A > 1 || B < 0 || B > 1 || C < 0 || C > 1) inrange = 0;
		double e = fabs((A - B) * 20.0 - (va - vb)) + fabs((B - C) * 20.0 - (vb - vc));
		if (e > worst) worst = e;
	}
	check(inrange && worst < 0.05, "modulation duties in 0..1, line-to-line volts error %.3f V (bound 0.05)", worst);

	frl_ramp_t r = { 0, FRL_R(0.5) };
	frl_real v = 0; for (int k = 0; k < 3; k++) v = frl_ramp(&r, FRL_R(10.0));
	check(fabs((double)FRL_TOF(v) - 1.5) < 1e-3, "ramp moves by its step (%.3f)", (double)FRL_TOF(v));
#if FRL_NUM != FRL_NUM_FLOAT
	{ double se = 0; for (int k = 1; k < 100; k++) { double x = k * 0.9; double e = fabs((double)FRL_TOF(FRL_SQRT(FRL_R(x))) - sqrt(x)); if (e > se) se = e; }
	  check(se < 2e-4, "integer sqrt max error %.2e (bound 2e-4)", se); }
#endif
}

/* ---- 2. plant and loops ----------------------------------------------------------------------------------------- */
static const double P_R = 0.75, P_L = 1.05e-3, P_LAM = 4.74e-3, P_PP = 4, P_J = 2.4e-6, VBUS = 20.0, TS = 50e-6;
typedef struct { double id, iq, th, wm; } plant_t;   /* th: electrical turns, wm: mechanical rad/s */
static void plant_step(plant_t *p, double va_, double vb_, int mech, double fe_hz)
{
	const int N = 20; const double h = TS / N;
	for (int k = 0; k < N; k++) {
		double a = TWO_PI * p->th, c = cos(a), s = sin(a);
		double vd = va_ * c + vb_ * s, vq = -va_ * s + vb_ * c;
		double we = mech ? P_PP * p->wm : TWO_PI * fe_hz;
		double did = (vd - P_R * p->id + we * P_L * p->iq) / P_L;
		double diq = (vq - P_R * p->iq - we * P_L * p->id - we * P_LAM) / P_L;
		p->id += h * did; p->iq += h * diq;
		if (mech) p->wm += h * (1.5 * P_PP * P_LAM * p->iq) / P_J;
		p->th += h * we / TWO_PI; p->th -= floor(p->th);
	}
}
static void plant_sample(const plant_t *p, double *ia, double *ib)
{
	double a = TWO_PI * p->th, ial = p->id * cos(a) - p->iq * sin(a), ibe = p->id * sin(a) + p->iq * cos(a);
	*ia = ial; *ib = -0.5 * ial + 0.8660254037844386 * ibe;
}
static void duty_to_ab(double A, double B, double C, double *va, double *vb)
{
	double m = (A + B + C) / 3.0, pa = (A - m) * VBUS, pb = (B - m) * VBUS, pc = (C - m) * VBUS;
	*va = pa; *vb = (pb - pc) / 1.7320508075688772;
}

/* gains: 1 kHz current-loop crossover from R and L; speed loop 20 Hz crossover from Kt and J (same numbers both builds) */
#define WC (TWO_PI * 1000.0)
static const double VMAX = 10.97, KP_I = WC * 1.05e-3, KI_I = WC * 0.75 * 50e-6;
#ifdef FRL_TEST_BREAK
#define KP_DUT (KP_I * 0.5)
#else
#define KP_DUT KP_I
#endif
typedef struct { frl_pi_t pd, pq; } dut_t;
typedef struct { double kp, ki, in, lo, hi; } dpi_t;
static double dpi(dpi_t *p, double e)
{
	double u = p->kp * e + p->in, o = u > p->hi ? p->hi : (u < p->lo ? p->lo : u);
	if (o == u || (u > p->hi && e < 0) || (u < p->lo && e > 0)) { p->in += p->ki * e; p->in = p->in > p->hi ? p->hi : (p->in < p->lo ? p->lo : p->in); }
	return o;
}
static void dut_init(dut_t *d)
{
	frl_pi_t t = { FRL_R(KP_DUT), FRL_R(KI_I), 0, FRL_R(-VMAX), FRL_R(VMAX) };
	d->pd = t; d->pq = t;
}
static void dut_loop(dut_t *d, double ia, double ib, frl_real th, double idr, double iqr, double *va, double *vb)
{
	frl_real s, c, al, be, id, iq, vd, vq, val, vbe, da, db, dc;
	FRL_SINCOS(th, &s, &c);
	frl_clarke(FRL_R(ia), FRL_R(ib), &al, &be);
	frl_park(al, be, s, c, &id, &iq);
	vd = frl_pi(&d->pd, FRL_R(idr) - id);
	vq = frl_pi(&d->pq, FRL_R(iqr) - iq);
	frl_ipark(vd, vq, s, c, &val, &vbe);
	frl_modulate(val, vbe, FRL_R(1.0 / VBUS), &da, &db, &dc);
	duty_to_ab((double)FRL_TOF(da), (double)FRL_TOF(db), (double)FRL_TOF(dc), va, vb);
}
typedef struct { dpi_t pd, pq; } ref_t;
static void ref_init(ref_t *r)
{
	dpi_t t = { KP_I, KI_I, 0, -VMAX, VMAX };
	r->pd = t; r->pq = t;
}
static void ref_loop(ref_t *r, double ia, double ib, double th, double idr, double iqr, double *va, double *vb)
{
	double s = sin(TWO_PI * th), c = cos(TWO_PI * th), al = ia, be = (ia + 2 * ib) / sqrt(3.0);
	double id = al * c + be * s, iq = be * c - al * s;
	double vd = dpi(&r->pd, idr - id), vq = dpi(&r->pq, iqr - iq);
	double val = vd * c - vq * s, vbe = vd * s + vq * c;
	double vA = val, vB = -0.5 * val + 0.8660254037844386 * vbe, vC = -0.5 * val - 0.8660254037844386 * vbe;
	double mx = fmax(vA, fmax(vB, vC)), mn = fmin(vA, fmin(vB, vC)), off = 0.5 * (mx + mn);
	double dA = 0.5 + (vA - off) / VBUS, dB = 0.5 + (vB - off) / VBUS, dC = 0.5 + (vC - off) / VBUS;
	dA = fmin(1, fmax(0, dA)); dB = fmin(1, fmax(0, dB)); dC = fmin(1, fmax(0, dC));
	duty_to_ab(dA, dB, dC, va, vb);
}

static void test_current_step(void)
{
	printf("current step, rotor driven at 200 Hz electrical (%s)\n", B_NAME);
	plant_t pd = { 0, 0, 0.1, 0 }, pr = pd;
	dut_t d; dut_init(&d); ref_t r; ref_init(&r);
	double vad = 0, vbd = 0, var = 0, vbr = 0, worst_q = 0, worst_d = 0, tail = 0;
	double t90d = -1, t90r = -1;
	for (int k = 0; k < 200; k++) {
		double iqr = k >= 20 ? 2.0 : 0.0, ia, ib;
		plant_step(&pd, vad, vbd, 0, 200.0);      /* the voltage computed last sample acts during this one */
		plant_step(&pr, var, vbr, 0, 200.0);
		plant_sample(&pd, &ia, &ib); dut_loop(&d, ia, ib, FRL_R(pd.th), 0.0, iqr, &vad, &vbd);
		plant_sample(&pr, &ia, &ib); ref_loop(&r, ia, ib, pr.th, 0.0, iqr, &var, &vbr);
		double eq = fabs(pd.iq - pr.iq), ed = fabs(pd.id - pr.id);
		if (eq > worst_q) worst_q = eq;
		if (ed > worst_d) worst_d = ed;
		if (t90d < 0 && pd.iq > 1.8) t90d = (k - 20) * 0.05;
		if (t90r < 0 && pr.iq > 1.8) t90r = (k - 20) * 0.05;
		if (k >= 140 && fabs(pd.iq - 2.0) > tail) tail = fabs(pd.iq - 2.0);
	}
	check(worst_q <= B_STEP, "max |iq - iq_ref| %.2e A (bound %.1e)", worst_q, B_STEP);
	check(worst_d <= B_STEP, "max |id - id_ref| %.2e A (bound %.1e)", worst_d, B_STEP);
	check(tail < 0.04, "settled: last 3 ms within %.4f A of 2 A (bound 0.04)", tail);
	check(fabs(t90d - t90r) <= 0.051, "90 %% rise at %.2f ms against %.2f ms for the double controller (within one sample)", t90d, t90r);
}

/* Hall bits for an electrical angle in turns: sector k = floor(6 th) in the order 3, 2, 6, 4, 5, 1 */
static unsigned hall_bits(double th) { static const unsigned o[6] = {3, 2, 6, 4, 5, 1}; int k = (int)(th * 6.0); return o[k < 0 ? 0 : (k > 5 ? 5 : k)]; }
#define HZ_PER_W (20000.0 / 4294967296.0)

static void test_hall_run(void)
{
	printf("Hall-tracked speed run, standstill to 100 Hz electrical (%s)\n", B_NAME);
	static frl_hall_tab_t tab;
	frl_hall_tab_nominal(&tab);
	frl_hall_t hd, hr; frl_hall_init(&hd, &tab, 0, 2000); frl_hall_init(&hr, &tab, 0, 2000);
	plant_t pd = { 0, 0, 0.30, 0 }, pr = pd;
	dut_t d; dut_init(&d); ref_t r; ref_init(&r);
	/* speed loop at 2 kHz: Kp = wc/G, zero at wc/5, G = Kt/J x pp in Hz/s per A */
	const double G = 1.5 * P_PP * P_LAM / P_J * P_PP / TWO_PI, WS = TWO_PI * 20.0;
	const double kps = WS / G, kis = kps * (WS / 5.0) * 500e-6, IQMAX = 3.0;
	frl_pi_t sd = { FRL_R(kps), FRL_R(kis), 0, FRL_R(-IQMAX), FRL_R(IQMAX) };
	dpi_t sr = { kps, kis, 0, -IQMAX, IQMAX };
	frl_ramp_t rd = { 0, FRL_R(2.5) };
	double rr = 0, iqd = 0, iqr = 0, vad = 0, vbd = 0, var = 0, vbr = 0;
	double se2 = 0, semax = 0, ee_sum = 0, eemax = 0, neg = 0, sum_est = 0, sum_td = 0, sum_tr = 0, sum_iq = 0, late = 0, lateN = 0;
	int nwin = 0, nedge = 0;
	for (int k = 0; k < 4000; k++) {
		double ia, ib;
		plant_step(&pd, vad, vbd, 1, 0); plant_step(&pr, var, vbr, 1, 0);
		uint8_t prev_d = hd.cur; uint32_t te_prev = hd.te;
		frl_hall_step(&hd, hall_bits(pd.th)); frl_hall_step(&hr, hall_bits(pr.th));
		double wd = hd.w * HZ_PER_W, wr = hr.w * HZ_PER_W;
		if (k % 10 == 9) {                                  /* 2 kHz speed loop */
			double ref = k * TS >= 0 ? 100.0 : 0.0;
			frl_real rdv = frl_ramp(&rd, FRL_R(ref));
			iqd = (double)FRL_TOF(frl_pi(&sd, rdv - FRL_R(wd)));
			rr += 2.5; if (rr > ref) rr = ref;
			iqr = dpi(&sr, rr - wr);
		}
		plant_sample(&pd, &ia, &ib); dut_loop(&d, ia, ib, FRL_ANG_FROM_T32(hd.th), 0.0, iqd, &vad, &vbd);
		plant_sample(&pr, &ia, &ib); ref_loop(&r, ia, ib, hr.th / 4294967296.0, 0.0, iqr, &var, &vbr);
		double tms = k * TS * 1000.0, true_hz = P_PP * pd.wm / TWO_PI;
		if (hd.n >= 2 && wd < 0) neg += 1;
		if (tms >= 120.0) {                                 /* steady window */
			double ea = (double)(int32_t)(hd.th - (uint32_t)(pd.th * 4294967296.0)) / 4294967296.0 * 360.0;
			se2 += ea * ea; if (fabs(ea) > semax) semax = fabs(ea);
			sum_est += wd; sum_td += true_hz; sum_tr += P_PP * pr.wm / TWO_PI; sum_iq += pd.iq; nwin++;
		}
		if (tms >= 80.0) { late += fabs(wd - 100.0); lateN++; }
		if (tms >= 120.0 && hd.cur != prev_d && hd.te == 0 && te_prev != 0xFFFFFFFFUL) {
			double ee = (wd - true_hz) / true_hz * 100.0;
			ee_sum += ee; if (fabs(ee) > eemax) eemax = fabs(ee); nedge++;
		}
	}
	(void)nedge;
	check(neg == 0, "speed estimate never negative after the second edge");
	check(sqrt(se2 / nwin) <= 2.0, "tracker angle error rms %.2f deg (bound 2.0)", sqrt(se2 / nwin));
	check(semax <= 4.0, "tracker angle error max %.2f deg (bound 4.0)", semax);
	check(nedge > 20, "%d edges in the window", nedge);
	check(fabs(ee_sum / nedge) <= 0.5, "per-edge speed estimate error, signed mean %.2f %% (bound 0.5)", ee_sum / nedge);
	check(eemax <= 4.5, "per-edge speed estimate error worst %.2f %% (bound 4.5)", eemax);
	check(late / lateN <= 2.0, "from 80 ms the estimate is within %.2f Hz of 100 Hz on average (bound 2 %% = 2 Hz)", late / lateN);
	check(fabs(sum_est / nwin - 100.0) <= 1.0, "mean estimated speed %.2f Hz (bound 100 +/- 1)", sum_est / nwin);
	check(fabs(sum_td / nwin - sum_tr / nwin) <= B_SPEED_MEAN, "true mean speed %.2f Hz against the double controller's %.2f Hz (bound %.1f)",
	      sum_td / nwin, sum_tr / nwin, B_SPEED_MEAN);
	check(fabs(sum_iq / nwin) <= 0.1, "mean iq %.3f A at constant speed with no load (bound 0.1)", sum_iq / nwin);
}

/* ---- 3. Hall tracker scripts ----------------------------------------------------------------------------------- */
static void feed(frl_hall_t *h, unsigned s, int isrs) { for (int i = 0; i < isrs; i++) frl_hall_step(h, s); }
static void test_hall_scripts(void)
{
	printf("Hall tracker scripts\n");
	static frl_hall_tab_t tab; frl_hall_tab_nominal(&tab);
	static const unsigned fw[6] = {3, 2, 6, 4, 5, 1};
	{ static const frl_hall_tab_t konst = FRL_HALL_TAB_NOMINAL;
	  int same = 1; for (int i = 1; i <= 6; i++) if (konst.fwd[i] != tab.fwd[i] || konst.rev[i] != tab.rev[i]) same = 0;
	  check(same, "the constant nominal table equals the computed one"); }
	frl_hall_t h; frl_hall_init(&h, &tab, 0, 2000);
	const int per = 30;                                   /* 30 ISRs per sector: 20000 / (30 x 6) = 111 Hz */
	double want = (double)FRL_T32_SECT / per;
	for (int k = 0; k < 4; k++) feed(&h, fw[k % 6], per);
	check(h.w > 0 && h.dir == 1, "forward: positive speed and direction +1 from the edge order after 4 edges");
	for (int k = 4; k < 14; k++) feed(&h, fw[k % 6], per);
	check(fabs(h.w - want) / want < 0.02, "forward: speed %.4f of a sector per ISR after 14 edges (want %.4f within 2 %%)", h.w / (double)FRL_T32_SECT, 1.0 / per);
	/* reversal: the sign flips at the first back edge with w = 0, then follows the second back edge. The second back edge
	   reads about 9 %% high: the edge angle carries the delay compensation of the old forward speed and the first
	   compensated edge is one step of 2.5 ISRs in 30 (kept as in the proven AM13 image); it settles in two more edges. */
	frl_hall_t b; frl_hall_init(&b, &tab, 0, 2000);
	for (int k = 0; k < 4; k++) feed(&b, fw[k], per);
	feed(&b, fw[2], per);                                 /* back one sector */
	check(b.w == 0 && b.dir == -1, "reversal: speed 0 and direction -1 at the first back edge (w=%d dir=%d)", (int)b.w, b.dir);
	feed(&b, fw[1], per);
	check(b.w < 0 && fabs(-b.w - want) / want < 0.12, "reversal: negative speed %.4f after the second back edge (within 12 %%)", b.w / (double)FRL_T32_SECT);
	for (int k = 0; k < 8; k++) feed(&b, fw[(12 - k) % 6], per);
	check(b.w < 0 && fabs(-b.w - want) / want < 0.02, "reversal: %.4f after 8 more back edges (within 2 %%)", b.w / (double)FRL_T32_SECT);
	/* bounce: forward, one edge back and forward again within a quarter sector at speed */
	frl_hall_t o; frl_hall_init(&o, &tab, 0, 2000);
	for (int k = 0; k < 4; k++) feed(&o, fw[k], per);
	int32_t wb = o.w;
	feed(&o, fw[2], 3);                                   /* back across the edge for 3 ISRs (accepted: debounce 3) */
	feed(&o, fw[3], 3);                                   /* and forward again at once */
	check(o.dir == 1 && o.w == wb, "bounce: the speed held before comes back (%d against %d)", (int)o.w, (int)wb);
	/* standstill */
	frl_hall_t s; frl_hall_init(&s, &tab, 0, 2000);
	for (int k = 0; k < 3; k++) feed(&s, fw[k], per);
	feed(&s, fw[3], 2500);
	check(s.w == 0 && s.th == frl_hall_mid(&tab, fw[3], 0) && s.n == 0, "standstill: speed 0, angle at the sector middle");
	/* invalid states are ignored and flagged */
	frl_hall_t v; frl_hall_init(&v, &tab, 0, 2000);
	for (int k = 0; k < 3; k++) feed(&v, fw[k], per);
	uint8_t cur = v.cur; feed(&v, 0, 5); int f0 = v.fault; feed(&v, 7, 5);
	check(f0 == 1 && v.fault == 1 && v.cur == cur, "invalid states 0 and 7: fault flagged, sector unchanged");
	feed(&v, fw[3], 4);
	check(v.fault == 0 && v.cur == fw[3], "a valid state clears the fault");
	/* a skipped sector still gives the direction (step 2) and an opposite sector restarts */
	frl_hall_t k2; frl_hall_init(&k2, &tab, 0, 2000);
	feed(&k2, 3, per); feed(&k2, 6, per);                 /* 3 -> 6 skips 2: forward */
	check(k2.dir == 1 && k2.n == 1, "skipped sector: forward from the edge order");
	feed(&k2, 5, per);                                    /* 6 -> 5: backward is 6 -> 4 -> 5? 6 is idx 2, 5 is idx 4: step 2 forward */
	/* overdue edge: after the last edge the angle may run 1.2 sectors at most and the speed falls */
	frl_hall_t od; frl_hall_init(&od, &tab, 0, 20000);
	for (int k = 0; k < 4; k++) feed(&od, fw[k], per);
	uint32_t edge_th = od.th; feed(&od, fw[3], 200);
	int32_t trav = (int32_t)(od.th - edge_th);
	check(trav > 0 && trav <= (int32_t)(FRL_T32_LIM + FRL_T32_SECT / 20), "overdue edge: angle ran %.2f sectors past the last edge (bound 1.2)", trav / (double)FRL_T32_SECT);
	check(od.w < (int32_t)(want * 0.5), "overdue edge: speed bound falls to %.1f %% of the last", 100.0 * od.w / want);
	/* six-step pattern: forward vector leads the sector middle by 60 degrees, reverse trails by 60 (nominal wiring) */
	int ok6 = 1;
	for (int st = 1; st <= 6; st++) {
		unsigned hi, lo; int idx = frl_hall_idx[st];
		double centre = (idx * 60.0 + 30.0);
		for (int dir = -1; dir <= 1; dir += 2) {
			if (!frl_sixstep_hall((unsigned)st, dir, &hi, &lo)) { ok6 = 0; continue; }
			double ang[3] = {0, 120, 240};
			double x = cos(ang[hi] * TWO_PI / 360) - cos(ang[lo] * TWO_PI / 360), y = sin(ang[hi] * TWO_PI / 360) - sin(ang[lo] * TWO_PI / 360);
			double dirv = atan2(y, x) * 180 / M_PI, diff = fmod(dirv - centre - 60.0 * dir + 720.0, 360.0);
			if (!(diff < 0.01 || diff > 359.99) || hi == lo) ok6 = 0;
		}
	}
	unsigned hi, lo;
	check(ok6 && !frl_sixstep_hall(0, 1, &hi, &lo) && !frl_sixstep_hall(7, 1, &hi, &lo), "six-step table: vector 60 degrees ahead (forward) or behind (reverse) the sector middle; 0 and 7 refused");
}

/* ---- 4. guard and cost ------------------------------------------------------------------------------------------ */
static const frl_guard_cfg_t GC = {
	FRL_R(3.0), FRL_R(1.25), FRL_R(8.0), FRL_R(24.0), FRL_R(0.2), 60000, 10000, 5000, 1000, 40, 20000, 1000, 100, 5 };
static frl_guard_start_t good_start(void) { frl_guard_start_t s = { {0, 0, 0}, FRL_R(12.0), 0, 1, 1 }; return s; }
static frl_guard_in_t good_in(void) { frl_guard_in_t i = { {FRL_R(0.5), FRL_R(-0.5), 0}, FRL_R(12.0), 0, 0 }; return i; }
static void test_guard(void)
{
	printf("guard\n");
	frl_guard_t g; frl_guard_init(&g, &GC);
	frl_guard_set_ilimit(&g, FRL_R(9.0)); check(g.ilimit == GC.ilimit_ceil, "limit above the ceiling is clamped to it");
	frl_guard_set_ilimit(&g, FRL_R(-1.0)); check(g.ilimit == GC.ilimit_def, "limit at or below zero reverts to the default");
	frl_guard_set_ilimit(&g, FRL_R(2.0)); check(g.ilimit == FRL_R(2.0), "limit inside the ceiling is kept");
	frl_guard_set_cap_ms(&g, 999999); check(g.cap_ms == 60000, "cap clamped"); frl_guard_set_cap_ms(&g, 0); check(g.cap_ms == 10000, "cap zero reverts");
	frl_guard_set_hb_ms(&g, 99999); check(g.hb_ms == 5000, "heartbeat window clamped");

	frl_guard_start_t s;
	s = good_start(); s.drv_fault = 1; check(frl_guard_start(&g, &s, 0) == FRL_R_REFUSED_FAULT, "refused: driver fault");
	s = good_start(); s.vbus = FRL_R(30.0); check(frl_guard_start(&g, &s, 0) == FRL_R_REFUSED_BUS, "refused: bus high");
	s = good_start(); s.vbus = FRL_R(5.0); check(frl_guard_start(&g, &s, 0) == FRL_R_REFUSED_BUS, "refused: bus low");
	s = good_start(); s.cal_ok = 0; check(frl_guard_start(&g, &s, 0) == FRL_R_REFUSED_UNCAL, "refused: angle source uncalibrated");
	s = good_start(); s.i[1] = FRL_R(-0.5); check(frl_guard_start(&g, &s, 0) == FRL_R_REFUSED_OFFSET, "refused: standstill current off zero");
	s = good_start(); s.cfg_ok = 0; check(frl_guard_start(&g, &s, 0) == FRL_R_REFUSED_CFG, "refused: configuration");
	check(g.state == FRL_G_IDLE, "a refusal leaves the guard idle");

	frl_guard_set_ilimit(&g, FRL_R(2.0)); s = good_start();
	check(frl_guard_start(&g, &s, 1000) == FRL_R_NONE && g.state == FRL_G_RUN, "start accepted");
	frl_guard_set_ilimit(&g, FRL_R(0.5)); check(g.run_ilimit == FRL_R(2.0), "limit latched at the start: a later write waits for the next start");
	frl_guard_in_t in = good_in();
	check(frl_guard_isr(&g, &in) == FRL_R_NONE, "a normal ISR passes");
	for (int ph = 0; ph < 3; ph++) for (int sg = -1; sg <= 1; sg += 2) {
		frl_guard_start(&g, &s, 1000); in = good_in(); in.i[ph] = sg * FRL_R(2.5);
		check(frl_guard_isr(&g, &in) == FRL_R_TRIP_ILIMIT && g.state == FRL_G_TRIPPED, "current trip on phase %d, sign %d", ph, sg);
	}
	in = good_in(); in.i[0] = FRL_R(2.5); in.drv_fault = 0;
	check(frl_guard_isr(&g, &in) == FRL_R_NONE, "no checks while tripped (the caller already has the driver off)");
	frl_guard_start(&g, &s, 1000); in = good_in(); in.nonfinite = 1;
	check(frl_guard_isr(&g, &in) == FRL_R_TRIP_NONFINITE, "non-finite trip");
	frl_guard_start(&g, &s, 1000); in = good_in(); in.drv_fault = 1;
	check(frl_guard_isr(&g, &in) == FRL_R_TRIP_DRIVER, "driver fault trip");
	frl_guard_start(&g, &s, 1000); in = good_in(); in.vbus = FRL_R(26.0);
	check(frl_guard_isr(&g, &in) == FRL_R_TRIP_BUS, "bus excursion trip");
	/* first trip is kept */
	frl_guard_start(&g, &s, 1000); in = good_in(); in.i[0] = FRL_R(2.5); frl_guard_isr(&g, &in); in.nonfinite = 1; frl_guard_isr(&g, &in);
	check(g.reason == FRL_R_TRIP_ILIMIT, "the first trip's reason is the one kept");
	/* stall: more than stall_isrs (40) ISRs without a tick */
	frl_guard_start(&g, &s, 1000); in = good_in(); frl_reason_t r = FRL_R_NONE;
	for (int k = 0; k < 41 && r == FRL_R_NONE; k++) r = frl_guard_isr(&g, &in);
	check(r == FRL_R_TRIP_STALL, "background stall trip after 41 ISRs without a tick");
	frl_guard_start(&g, &s, 1000); r = FRL_R_NONE;
	for (int k = 0; k < 400 && r == FRL_R_NONE; k++) { r = frl_guard_isr(&g, &in); if (k % 20 == 19) frl_guard_tick(&g, 1000 + k * 10000); }
	check(r == FRL_R_NONE, "no stall while the tick runs every 20 ISRs");

	/* cap, heartbeat, clock: ISR at 20 kHz = 20 per ms; cycle counter 1000 cycles per ms in this config */
	uint32_t cyc = 0; r = FRL_R_NONE;
	frl_guard_start(&g, &s, cyc); g.run_cap_ms = 50; g.run_hb_ms = 5000;
	for (int ms = 1; ms <= 100 && r == FRL_R_NONE; ms++) {
		in = good_in(); for (int k = 0; k < 20; k++) frl_guard_isr(&g, &in);
		cyc += 1000; g.heartbeat++; r = frl_guard_tick(&g, cyc);
		if (r != FRL_R_NONE) check(r == FRL_R_TRIP_CAP && ms == 50, "run cap trip at %d ms (want 50)", ms);
	}
	check(r == FRL_R_TRIP_CAP, "run cap reached");
	r = FRL_R_NONE; cyc = 0; frl_guard_start(&g, &s, cyc); g.run_hb_ms = 20; int hbms = 0;
	for (int ms = 1; ms <= 100 && r == FRL_R_NONE; ms++) {
		in = good_in(); for (int k = 0; k < 20; k++) frl_guard_isr(&g, &in);
		cyc += 1000; if (ms <= 10) g.heartbeat++; r = frl_guard_tick(&g, cyc); hbms = ms;
	}
	check(r == FRL_R_TRIP_HEARTBEAT && hbms == 31, "heartbeat silence trip at %d ms (last change at 10 ms + 20 ms window = 31)", hbms);
	r = FRL_R_NONE; cyc = 0; frl_guard_start(&g, &s, cyc); g.run_hb_ms = 1000; int cms = 0;
	for (int ms = 1; ms <= 400 && r == FRL_R_NONE; ms++) {   /* the cycle counter runs 20 % fast: 1,200 cycles per real ms */
		in = good_in(); for (int k = 0; k < 20; k++) frl_guard_isr(&g, &in);
		cyc += 1200; g.heartbeat++; r = frl_guard_tick(&g, cyc); cms = ms;
	}
	check(r == FRL_R_TRIP_CLOCK, "clock disagreement trip (counter 20 %% fast) at %d ms", cms);
	r = FRL_R_NONE; cyc = 0; frl_guard_start(&g, &s, cyc); g.run_hb_ms = 1000;
	for (int ms = 1; ms <= 400 && r == FRL_R_NONE; ms++) {   /* 3 % off is inside the 5 % tolerance */
		in = good_in(); for (int k = 0; k < 20; k++) frl_guard_isr(&g, &in);
		cyc += 1030; g.heartbeat++; r = frl_guard_tick(&g, cyc);
		if (ms == 100) g.run_cap_ms = 60000;
	}
	check(r == FRL_R_NONE, "a 3 %% clock difference stays inside the 5 %% tolerance");
	check(frl_guard_reaction(FRL_R_TRIP_ILIMIT) == 0 && (frl_guard_reaction(FRL_R_TRIP_HEARTBEAT) & 1) && (frl_guard_reaction(FRL_R_TRIP_CAP) & 2),
	      "reaction table: current trip coasts, host silence brakes and may retry");
#if FRL_NUM == FRL_NUM_FLOAT
	{ volatile float z = 0.0f; float nanv = z / z, infv = 1.0f / z;
	  check(FRL_NONFINITE(nanv) && FRL_NONFINITE(infv) && !FRL_NONFINITE(1.5f), "FRL_NONFINITE sees NaN and infinity"); }
#endif
}

static void test_cost(void)
{
	printf("cost stamps\n");
	frl_cost_t c; frl_cost_reset(&c);
	for (int k = 0; k < 400; k++) {
		uint32_t d = (k == 200) ? 900 : 100 + (k & 1);   /* 100 or 101 cycles, one outlier of 900 */
		fake_cycles = 5000 + (uint32_t)k * 7919u;
		FRL_COST_BEGIN(c);
		fake_cycles += d;
		FRL_COST_END(c);
	}
	check(c.max == 900 && frl_cost_mean(&c) >= 100 && frl_cost_mean(&c) <= 101 && c.n == 400,
	      "cost: max %u, mean %u over %u stamps (want 900, 100 to 101, 400)", (unsigned)c.max, (unsigned)frl_cost_mean(&c), (unsigned)c.n);
	fake_cycles = 0xFFFFFF00u; FRL_COST_BEGIN(c); fake_cycles = 0x00000040u; FRL_COST_END(c);
	check(c.last == 0x140, "cost: the difference wraps correctly (%u)", (unsigned)c.last);
}

int main(void)
{
	printf("portability/motor host test, build %s\n", B_NAME);
	test_kernel();
	test_current_step();
	test_hall_run();
	test_hall_scripts();
	test_guard();
	test_cost();
	if (g_fail) { printf("FAILED: %d check(s)\n", g_fail); return 1; }
	printf("OK %s\n", B_NAME);
	return 0;
}
