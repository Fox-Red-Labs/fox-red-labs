/* fom_tb.h: the parts of the rate sweep and load pass (fom.h) that are the same on every board that reports
   over the serial port with cc2340r5/bench/lib/tb.h (LP-EM-CC2340R5, LP-CC1354P10-1):
   the interrupt's checks, the main-loop poll, the run order, and the I,fom and R lines.
   Method: docs/methodology.md, "Control-loop figures of merit". Header only; include it once per program.

   Before including it, the program defines its CPU cycle counter:
     FOM_NOW()            one read of the counter
     FOM_SINCE(a, b)      cycles from read a to the later read b (the counter's width and direction)
   SysTick (the CMSIS core header) must be included first. After the include, the program supplies,
   for each block, a trial function (fom.h) whose interrupt calls fom_isr_check(),
   fom_isr_work() and fom_isr_body() below and counts ADC overflows into fom_st.ovf.

   Lines go to the report (tb_info, tb_result) and, at the same moment, straight to the serial port (tb_say),
   so a program that stops later still leaves them in the capture's raw text. tb_result and tb_info cut a
   line at 199 and 159 characters; fom_line() puts the rest of a long R line on an I,<test>_more line. */
#ifndef FOM_TB_H
#define FOM_TB_H
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "fom.h"
#include "tb.h"

/* CPU cycles per trigger-timer count. 1 on every board whose timer runs on the CPU clock; a program whose
   timer is slower defines it first (the CC2755P10: CPU 96 MHz, LGPT 48 MHz, so 2). SysTick differences
   (cycles) and the timer's counts are both used below, so each is converted before they meet. */
#ifndef FOM_TIMER_DIV
#define FOM_TIMER_DIV 1u
#endif

/* State the interrupt fills during one trial. */
typedef struct {
	uint32_t period, late_lim;                   /* set by fom_state_clear() before a trial */
	uint32_t load;                               /* 1 in the load pass: the interrupt sums work and body */
	uint32_t irqs, late, wr_late, ovf, c_prev;
	uint32_t work_sum, work_max, body_sum, body_max, body_min;
} fom_state_t;
/* Own sections: at -O3 TI Arm Clang merges a file's static variables into one block, and these would move
   the program's own variables and change the code of its existing handler. */
static volatile fom_state_t fom_st __attribute__((section(".bss.fom_st")));

/* What the main-loop poll saw in one trial. */
typedef struct { uint32_t gaps, gap_sum, gap_max, it_min, it_max, elapsed; } fom_poll_t;
static fom_poll_t fom_pg __attribute__((section(".bss.fom_pg")));

volatile uint32_t fom[FOM_WORDS];

/* ---- in the interrupt ---- */

/* The checks after the loop work: late (more than 1.5 periods since the previous interrupt's first line) and
   late write (duty written after the period ended). Returns the interrupt count including this one. */
static inline uint32_t fom_isr_check(uint32_t c_in, uint32_t t_in, uint32_t c_wr)
{
	uint32_t n = fom_st.irqs;
	if (n != 0u && FOM_SINCE(fom_st.c_prev, c_in) > fom_st.late_lim) fom_st.late++;
	fom_st.c_prev = c_in;
	if (t_in * FOM_TIMER_DIV + FOM_SINCE(c_in, c_wr) >= fom_st.period * FOM_TIMER_DIV) fom_st.wr_late++;
	return n + 1u;
}

/* Load pass only: first line to the end of the loop work. */
static inline void fom_isr_work(uint32_t c_in, uint32_t c_end)
{
	uint32_t w = FOM_SINCE(c_in, c_end);
	fom_st.work_sum += w;
	if (w > fom_st.work_max) fom_st.work_max = w;
}

/* Load pass only: first line to the last line (c_exit is read just before this call). */
static inline void fom_isr_body(uint32_t c_in, uint32_t c_exit)
{
	uint32_t b = FOM_SINCE(c_in, c_exit);
	fom_st.body_sum += b;
	if (b > fom_st.body_max) fom_st.body_max = b;
	if (b < fom_st.body_min) fom_st.body_min = b;
}

/* Before a trial starts. */
static inline void fom_state_clear(uint32_t period)
{
	fom_st.period = period; fom_st.late_lim = (period + period / 2u) * FOM_TIMER_DIV;
	fom_st.irqs = fom_st.late = fom_st.wr_late = fom_st.ovf = fom_st.c_prev = 0;
	fom_st.work_sum = fom_st.work_max = fom_st.body_sum = fom_st.body_max = 0;
	fom_st.body_min = 0xFFFFFFFFu;
}

/* ---- in the main loop ---- */

/* The poll: read the counter, take the difference from the previous read, compare it. A difference above
   `thr` is one time the CPU was taken away (entry, the whole handler, the return, plus one iteration).
   Stops at FOM_N_IRQ interrupts, after `iters` iterations (0: no limit) or after `limit` cycles (the guard,
   counted from the differences, so it keeps counting across the wraps of a 24-bit SysTick).
   noinline: the calibration and the load pass run the same machine code. */
static __attribute__((noinline)) void fom_poll(uint32_t iters, uint32_t limit, uint32_t thr)
{
	uint32_t prev = FOM_NOW(), el = 0, k = 0, n = 0, sum = 0, mx = 0, imin = 0xFFFFFFFFu, imax = 0;
	for (;;) {
		uint32_t now = FOM_NOW();
		uint32_t d = FOM_SINCE(prev, now);
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

/* 1,000 poll iterations with interrupts off: the shortest and longest iteration. */
static void fom_calibrate(void)
{
	uint32_t pm;
	__asm volatile("mrs %0, primask" : "=r"(pm));
	__asm volatile("cpsid i" ::: "memory");
	fom_st.irqs = 0;
	fom_poll(1000u, 0xFFFFFFFFu, 0xFFFFFFFFu);
	if (!(pm & 1u)) __asm volatile("cpsie i" ::: "memory");
	fom[FOM_LOOP_MIN] = fom_pg.it_min;
	fom[FOM_LOOP_MAX] = fom_pg.it_max;
}

/* The poll threshold for the load pass. */
static inline uint32_t fom_gap_thr(void) { return 2u * fom[FOM_LOOP_MAX] + 8u; }

/* ---- lines ---- */

/* Wait `cycles` CPU cycles on SysTick (24 bits, counting down, always running on these boards), so the
   serial driver has sent its last bytes and its interrupt is quiet before the next timed part. */
#define FOM_QUIET 480000u   /* 10 ms at 48 MHz: a 32-byte UART FIFO empties in 2.8 ms at 115200 */
static void fom_quiet(uint32_t cycles)
{
	uint32_t prev = SysTick->VAL, el = 0;
	while (el < cycles) {
		uint32_t now = SysTick->VAL;
		el += (prev - now) & 0xFFFFFFu;
		prev = now;
	}
}

/* An I line in the report and on the port now (cut at 159 characters, as tb_info cuts it). */
static void fom_info(const char *key, const char *fmt, ...)
{
	char b[160];
	int k = snprintf(b, sizeof b, "I,%s,", key);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b + k, sizeof b - (size_t)k, fmt, ap);
	va_end(ap);
	tb_say(b);
	tb_info(key, "%s", b + k);
}

/* tb_result, and the same line on the port now. */
static void fom_result_now(const char *suite, const char *test, uint64_t cycles, int ok, const char *note)
{
	char b[200];
	if (cycles >= 1000000000ULL)
		snprintf(b, sizeof b, "R,%s,%s,%lu%09lu,%s,%s", suite, test, (unsigned long)(cycles / 1000000000ULL),
			 (unsigned long)(cycles % 1000000000ULL), ok ? "ok" : "FAIL", note ? note : "");
	else
		snprintf(b, sizeof b, "R,%s,%s,%lu,%s,%s", suite, test, (unsigned long)cycles, ok ? "ok" : "FAIL", note ? note : "");
	tb_say(b);
	tb_result(suite, test, cycles, ok, note);
}

/* An R line in the report and on the port now; a note too long for tb_result goes on as I,<test>_more. */
static void fom_line(const char *suite, const char *test, uint32_t value, int ok, const char *note)
{
	char b[400];
	snprintf(b, sizeof b, "R,%s,%s,%lu,%s,%s", suite, test, (unsigned long)value, ok ? "ok" : "FAIL", note);
	tb_say(b);
	size_t head = strlen(b) - strlen(note), room = 199u - head;
	if (strlen(note) <= room) { tb_result(suite, test, value, ok, note); return; }
	/* cut at the last "; " that fits */
	size_t cut = room;
	while (cut > 2u && !(note[cut - 2u] == ';' && note[cut - 1u] == ' ')) cut--;
	if (cut <= 2u) cut = room;
	char h[200];
	memcpy(h, note, cut); h[cut] = 0;
	if (cut >= 2u && h[cut - 2u] == ';') h[cut - 2u] = 0;
	tb_result(suite, test, value, ok, h);
	char t[48];
	snprintf(t, sizeof t, "%s_more", test);
	tb_info(t, "%s", note + cut);
}

/* "123.4" from a sum and a count */
static char *fom_mean(char *b, uint32_t sum, uint32_t n)
{
	if (n == 0u) n = 1u;
	uint32_t t = (uint32_t)(((uint64_t)sum * 10u + n / 2u) / n);
	sprintf(b, "%lu.%lu", (unsigned long)(t / 10u), (unsigned long)(t % 10u));
	return b;
}

/* worst_<pass>: the existing pass's worst trigger-to-write, as a percent of a 100 kHz period */
static void fom_worst_line(const char *suite, const char *pass, uint32_t worst, uint32_t best, int ran, uint32_t p100k)
{
	char n[160];
	uint32_t pct10 = (uint32_t)(((uint64_t)worst * 1000u + p100k / 2u) / p100k);
	snprintf(n, sizeof n, "%lu.%lu percent of a 100 kHz period; best %lu; jitter %lu cycles",
		 (unsigned long)(pct10 / 10u), (unsigned long)(pct10 % 10u), (unsigned long)best,
		 (unsigned long)(worst >= best ? worst - best : 0u));
	char t[48];
	snprintf(t, sizeof t, "worst_%s", pass);
	fom_line(suite, t, worst, ran, n);
}

/* ---- one block: sweep, then the load pass ----
   The load pass's rate comes from fom_load_period() (fom.h): 100 kHz when the highest rate is 200 kHz or more,
   else half the highest rate. The cpu_load line names the rate used. The few instructions after c_exit (the
   body sums) are not timed here; tools/results/fom_post_cycles.py counts them from the disassembly and
   tools/results/fom_decode.py --post takes them off. */

static void fom_block(unsigned k, uint32_t math, fom_trial_fn trial, uint32_t p_start, uint32_t p_min, uint32_t p_max,
		      const char *suite, const char *mname)
{
	volatile uint32_t *b = &fom[FOM_BLK(k)];
	b[FOM_MATH] = math;
	fom_st.load = 0;
	fom_quiet(FOM_QUIET);
	fom_sweep(b, trial, p_start, p_min, p_max);
	fom_info("fom_progress", "sweep %s done; p_pass %lu; p_fail %lu; trials %lu; confirm %lu", mname,
		 (unsigned long)b[FOM_P_PASS], (unsigned long)b[FOM_P_FAIL], (unsigned long)b[FOM_TRIALS],
		 (unsigned long)b[FOM_CONFIRM]);
	uint32_t per = fom_load_period(b, fom[FOM_P100K]);
	b[FOM_LOAD_PER] = per;
	if (per == 0u) {
		fom_info("fom_progress", "load %s skipped; no rate passed", mname);
		return;
	}
	uint32_t why = 0;
	fom_quiet(FOM_QUIET);
	fom_st.load = 1;
	(void)trial(per, &why);
	fom_st.load = 0;
	b[FOM_IRQS] = fom_st.irqs; b[FOM_LATE] = fom_st.late; b[FOM_WR_LATE] = fom_st.wr_late;
	b[FOM_OVF] = fom_st.ovf + ((why & FOM_WHY_OVF) && fom_st.ovf == 0u ? 1u : 0u);
	b[FOM_GAPS] = fom_pg.gaps; b[FOM_GAP_SUM] = fom_pg.gap_sum; b[FOM_GAP_MAX] = fom_pg.gap_max;
	b[FOM_BODY_SUM] = fom_st.body_sum; b[FOM_BODY_MAX] = fom_st.body_max;
	b[FOM_BODY_MIN] = fom_st.body_min == 0xFFFFFFFFu ? 0u : fom_st.body_min;
	b[FOM_WORK_SUM] = fom_st.work_sum; b[FOM_WORK_MAX] = fom_st.work_max;
	fom_info("fom_progress", "load %s done; period %lu; irqs %lu; gaps %lu", mname, (unsigned long)per,
		 (unsigned long)b[FOM_IRQS], (unsigned long)b[FOM_GAPS]);
}

/* ---- the report: I,fom words and the R lines for each block ---- */

static void fom_report(const char *suite, const char *const *mnames)
{
	char n[400], a[24], c[24], e[24];
	for (unsigned i = 0; i < FOM_WORDS; i += 8u) {
		int o = snprintf(n, sizeof n, "I,fom,0x%08x:", i * 4u);
		for (unsigned j = i; j < i + 8u; j++) o += snprintf(n + o, sizeof n - (size_t)o, " %08lx", (unsigned long)fom[j]);
		tb_say(n);
		tb_info("fom", "%s", n + 6);
	}
	for (unsigned k = 0; k < fom[FOM_N_MATH]; k++) {
		volatile uint32_t *b = &fom[FOM_BLK(k)];
		uint32_t pp = b[FOM_P_PASS], hz = pp ? (uint32_t)(((uint64_t)fom[FOM_TIMER_HZ] + pp / 2u) / pp) : 0u;
		char t[48];
		snprintf(n, sizeof n, "p_pass %lu counts; p_fail %lu; trials %lu; confirm %lu of 2; steps %lu; why_fail %lu",
			 (unsigned long)pp, (unsigned long)b[FOM_P_FAIL], (unsigned long)b[FOM_TRIALS],
			 (unsigned long)b[FOM_CONFIRM], (unsigned long)b[FOM_STEPS], (unsigned long)b[FOM_WHY_FAIL]);
		snprintf(t, sizeof t, "max_rate_%s", mnames[k]);
		fom_line(suite, t, hz, pp != 0u && b[FOM_CONFIRM] == 2u, n);

		uint32_t per = b[FOM_LOAD_PER], irqs = b[FOM_IRQS], gaps = b[FOM_GAPS];
		uint32_t load10 = 0;
		if (per != 0u && gaps != 0u && irqs != 0u) {
			/* (gap mean - poll min - (body mean - work mean)) / cycles in a 100 kHz period, in tenths of a percent */
			int64_t x = (int64_t)((uint64_t)b[FOM_GAP_SUM] * 1000u / gaps) - (int64_t)fom[FOM_LOOP_MIN] * 1000
				  - (int64_t)(((uint64_t)b[FOM_BODY_SUM] - b[FOM_WORK_SUM]) * 1000u / irqs);
			uint32_t c100 = fom[FOM_CPU_HZ] / 100000u;
			load10 = x > 0 ? (uint32_t)((x + c100 / 2) / c100) : 0u;
		}
		int lok = per != 0u && irqs == FOM_N_IRQ && gaps >= 990u && gaps <= 1000u && b[FOM_LATE] == 0u &&
			  b[FOM_WR_LATE] == 0u && b[FOM_OVF] == 0u;
		snprintf(n, sizeof n, "at %lu Hz; away from main mean %s worst %lu cycles incl. entry and return; handler first to last line mean %s worst %lu; loop work mean %s worst %lu; poll %lu to %lu; gaps %lu of 1000; late %lu; late writes %lu; overflow %lu",
			 (unsigned long)(per ? (fom[FOM_TIMER_HZ] + per / 2u) / per : 0u), fom_mean(a, b[FOM_GAP_SUM], gaps),
			 (unsigned long)b[FOM_GAP_MAX], fom_mean(c, b[FOM_BODY_SUM], irqs), (unsigned long)b[FOM_BODY_MAX],
			 fom_mean(e, b[FOM_WORK_SUM], irqs), (unsigned long)b[FOM_WORK_MAX], (unsigned long)fom[FOM_LOOP_MIN],
			 (unsigned long)fom[FOM_LOOP_MAX], (unsigned long)gaps, (unsigned long)b[FOM_LATE],
			 (unsigned long)b[FOM_WR_LATE], (unsigned long)b[FOM_OVF]);
		snprintf(t, sizeof t, "cpu_load_%s", mnames[k]);
		fom_line(suite, t, load10, lok, n);
	}
}

/* Header words; call before the first block. */
static void fom_header(uint32_t cpu_hz, uint32_t timer_hz, uint32_t n_math)
{
	for (unsigned i = 0; i < FOM_WORDS; i++) fom[i] = 0;
	fom[FOM_CPU_HZ] = cpu_hz;
	fom[FOM_TIMER_HZ] = timer_hz;
	fom[FOM_P100K] = timer_hz / 100000u;
	fom[FOM_N_MATH] = n_math;
	fom[FOM_LAYOUT] = 1u;
}

#endif
