/* fom_c28.h: the parts of the rate sweep and load pass (fom.h) for the C28x programs on the LAUNCHXL-F2800137
   (c2000-launchpad/bench/power-loop and c2000-launchpad/bench/ctrl). The same checks, poll and run order as
   fom_tb.h on the Arm boards; no serial port here: the debugger reads fom[0] .. fom[47] after the program halts
   in bench_done(). Method: docs/methodology.md, "Control-loop figures of merit". Header only; include it once
   per program, after the program's cyc() (CPU Timer 1 at SYSCLK, counting up).

   Both programs: ePWM1 counts up and starts ADCA SOC0 at zero; the ADC end-of-conversion interrupt INT_ADCA1
   (PIE group 1, bit 0) runs the loop. The program supplies:
     fom_isr()          the loop work with the stamps c_in, t_in, c_wr, c_end; its last line is fom_isr_end()
     fom_pwm_stop()     stop the trigger timer (and the PWM timer if it is another one)
     fom_pwm_set(p)     period p counts (register p - 1), counters 0, compare p / 2, controller state reset
     fom_pwm_start()    start them
     fom_select(k)      pick the math for block k
   C28x: int is 16 bits and uint32_t is unsigned long, so every 32-bit expression here is written as one. */
#ifndef FOM_C28_H
#define FOM_C28_H
#include "fom.h"

#define FOM_GUARD_CYC  (2UL * DEVICE_SYSCLK_FREQ)    /* 2 s, the trial's guard; CPU Timer 1 wraps after 35.8 s */
#define FOM_P_MAX      65535UL                       /* ePWM TBCTR is 16 bits: period register 65,534 */

__interrupt void fom_isr(void);
static void fom_pwm_stop(void);
static void fom_pwm_set(uint32_t p);
static void fom_pwm_start(void);
static void fom_select(uint16_t k);

/* State the interrupt fills during one trial. */
typedef struct {
	uint32_t period, late_lim, load;             /* set before the trial starts */
	uint32_t irqs, late, wr_late, ovf, c_prev;
	uint32_t work_sum, work_max, body_sum, body_max, body_min;
} fom_state_t;
static volatile fom_state_t fom_st;

/* What the main-loop poll saw in one trial. */
typedef struct { uint32_t gaps, gap_sum, gap_max, it_min, it_max, elapsed; } fom_poll_t;
static fom_poll_t fom_pg;

volatile uint32_t fom[FOM_WORDS];

/* ---- in the interrupt ---- */

/* The last line of fom_isr(): the checks after the loop work. Late: more than 1.5 periods since the previous
   interrupt's first line. Late write: duty written after the period ended. Overflow: the ADC raised ADCINT1
   again before its flag was cleared. After interrupt 1,000 the line is switched off (PIEIER1 bit 0; INTM is
   set inside the interrupt), so a loop slower than its period cannot keep the CPU. Load pass only: first line
   to the end of the loop work, then c_exit, then first line to c_exit. The sums after c_exit are not timed:
   tools/results/fom_post_cycles.py --arch c28 counts them from the disassembly, and fom_decode.py --post takes
   them off. The load pass's rate comes from fom_load_period() (fom.h): 100 kHz when the highest rate is 200 kHz
   or more, else half the highest rate. */
#pragma FUNC_ALWAYS_INLINE(fom_isr_end)
static inline void fom_isr_end(uint32_t c_in, uint32_t t_in, uint32_t c_wr, uint32_t c_end)
{
	uint32_t n = fom_st.irqs;
	if (n != 0UL && c_in - fom_st.c_prev > fom_st.late_lim) fom_st.late++;
	fom_st.c_prev = c_in;
	if (t_in + (c_wr - c_in) >= fom_st.period) fom_st.wr_late++;
	if ((HWREGH(ADCA_BASE + ADC_O_INTOVF) & 1U) != 0U) {
		fom_st.ovf++;
		HWREGH(ADCA_BASE + ADC_O_INTOVFCLR) = 1U;
	}
	n++;
	fom_st.irqs = n;
	if (n >= FOM_N_IRQ) HWREGH(PIECTRL_BASE + PIE_O_IER1) &= ~(uint16_t)1U;
	if (fom_st.load != 0UL) {
		uint32_t w = c_end - c_in, b;
		fom_st.work_sum += w;
		if (w > fom_st.work_max) fom_st.work_max = w;
		b = cyc() - c_in;                    /* c_exit: the last line of the interrupt's code */
		fom_st.body_sum += b;
		if (b > fom_st.body_max) fom_st.body_max = b;
		if (b < fom_st.body_min) fom_st.body_min = b;
	}
}

/* ---- in the main loop ---- */

/* The poll, the same as fom_tb.h: read the counter, take the difference from the previous read, compare it.
   A difference above thr is one time the CPU was taken away (entry, the whole handler, the return, plus one
   iteration). Stops at FOM_N_IRQ interrupts, after iters iterations (0: no limit) or after limit cycles.
   Not inlined: the calibration and the load pass run the same machine code. */
#pragma FUNC_CANNOT_INLINE(fom_poll)
static void fom_poll(uint32_t iters, uint32_t limit, uint32_t thr)
{
	uint32_t prev = cyc(), el = 0, k = 0, n = 0, sum = 0, mx = 0, imin = 0xFFFFFFFFUL, imax = 0;
	for (;;) {
		uint32_t now = cyc();
		uint32_t d = now - prev;
		prev = now;
		el += d;
		if (d > thr) {
			n++; sum += d;
			if (d > mx) mx = d;
		} else {
			if (d < imin) imin = d;
			if (d > imax) imax = d;
		}
		if (fom_st.irqs >= FOM_N_IRQ || el >= limit || ++k == iters) break;
	}
	fom_pg.gaps = n; fom_pg.gap_sum = sum; fom_pg.gap_max = mx;
	fom_pg.it_min = imin; fom_pg.it_max = imax; fom_pg.elapsed = el;
}

/* One trial at period p (fom.h). With load set, the main loop runs the poll instead of a plain wait. */
static uint32_t fom_ovf_flag;
static int fom_run(uint32_t p, uint32_t *why, uint32_t load)
{
	uint32_t t0;
	DINT;
	fom_pwm_stop();
	fom_pwm_set(p);
	fom_st.period = p; fom_st.late_lim = p + p / 2UL; fom_st.load = load;
	fom_st.irqs = fom_st.late = fom_st.wr_late = fom_st.ovf = fom_st.c_prev = 0;
	fom_st.work_sum = fom_st.work_max = fom_st.body_sum = fom_st.body_max = 0;
	fom_st.body_min = 0xFFFFFFFFUL;
	ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
	ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
	/* A flag left in the PIE from the last trial would enter at once with the counter still at 0. */
	HWREGH(PIECTRL_BASE + PIE_O_IFR1) &= ~(uint16_t)1U;
	Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
	Interrupt_enable(INT_ADCA1);
	EINT;
	fom_pwm_start();
	if (load != 0UL) {
		fom_poll(0UL, FOM_GUARD_CYC, 2UL * fom[FOM_LOOP_MAX] + 8UL);
	} else {
		t0 = cyc();
		while (fom_st.irqs < FOM_N_IRQ && cyc() - t0 < FOM_GUARD_CYC) { }
	}
	fom_pwm_stop();
	DINT;
	Interrupt_disable(INT_ADCA1);
	fom_ovf_flag = ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1) ? 1UL : 0UL;
	*why = 0;
	if (fom_st.irqs < FOM_N_IRQ) *why |= FOM_WHY_SHORT;
	if (fom_st.late != 0UL) *why |= FOM_WHY_LATE;
	if (fom_st.wr_late != 0UL) *why |= FOM_WHY_WRITE;
	if (fom_st.ovf != 0UL || fom_ovf_flag != 0UL) *why |= FOM_WHY_OVF;
	return *why == 0UL;
}

static int fom_trial(uint32_t p, uint32_t *why) { return fom_run(p, why, 0UL); }

/* After everything else the program does: header, poll calibration, then per block the sweep and the load
   pass; FOM_MAGIC last. math0 is FOM_MATH of block 0; block k gets math0 + k. */
static void fom_all(uint16_t n_math, uint32_t math0)
{
	uint16_t k, i;
	uint32_t hz = SysCtl_getClock(DEVICE_OSCSRC_FREQ), per, why;

	DINT;
	fom_pwm_stop();
	Interrupt_disable(INT_ADCA1);
	Interrupt_register(INT_ADCA1, &fom_isr);
	for (i = 0; i < FOM_WORDS; i++) fom[i] = 0;
	fom[FOM_CPU_HZ] = hz;
	fom[FOM_TIMER_HZ] = hz;                     /* ePWM time base at SYSCLK / 1 / 1; the program's ePWM check confirms it */
	fom[FOM_P100K] = hz / 100000UL;
	fom[FOM_N_MATH] = n_math;
	fom[FOM_LAYOUT] = 1UL;

	/* 1,000 poll iterations with interrupts off: the shortest and longest */
	fom_st.irqs = 0;
	fom_poll(FOM_N_IRQ, 0xFFFFFFFFUL, 0xFFFFFFFFUL);
	fom[FOM_LOOP_MIN] = fom_pg.it_min;
	fom[FOM_LOOP_MAX] = fom_pg.it_max;

	for (k = 0; k < n_math; k++) {
		volatile uint32_t *b = &fom[FOM_BLK(k)];
		b[FOM_MATH] = math0 + k;
		fom_select(k);
		fom_sweep(b, fom_trial, hz / 10000UL, 16UL, FOM_P_MAX);
		per = fom_load_period(b, fom[FOM_P100K]);
		b[FOM_LOAD_PER] = per;
		if (per == 0UL) continue;
		why = 0;
		(void)fom_run(per, &why, 1UL);
		b[FOM_IRQS] = fom_st.irqs; b[FOM_LATE] = fom_st.late; b[FOM_WR_LATE] = fom_st.wr_late;
		b[FOM_OVF] = fom_st.ovf + (fom_ovf_flag != 0UL && fom_st.ovf == 0UL ? 1UL : 0UL);
		b[FOM_GAPS] = fom_pg.gaps; b[FOM_GAP_SUM] = fom_pg.gap_sum; b[FOM_GAP_MAX] = fom_pg.gap_max;
		b[FOM_BODY_SUM] = fom_st.body_sum; b[FOM_BODY_MAX] = fom_st.body_max;
		b[FOM_BODY_MIN] = fom_st.body_min == 0xFFFFFFFFUL ? 0UL : fom_st.body_min;
		b[FOM_WORK_SUM] = fom_st.work_sum; b[FOM_WORK_MAX] = fom_st.work_max;
	}
	DINT;
	fom[FOM_MAGIC_I] = FOM_MAGIC;
}

#endif
