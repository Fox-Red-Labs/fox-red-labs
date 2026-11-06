/* fom.h: the rate sweep and the result layout for a hardware-triggered control loop, the same on every board.
   Method: docs/methodology.md, "Control-loop figures of merit".
   Header only, plain C (TI Arm Clang, TI C2000 compiler, the Mac's cc for host_test.c). No board code here.

   The board program supplies one function and fills the load-pass words itself:
     int trial(uint32_t period, uint32_t *why)
       Run FOM_N_IRQ (1,000) trigger periods of `period` trigger-timer counts with the program's own loop
       (trigger, ADC, control step, duty write) and return 1 when no period was missed, else 0 with the
       reasons in *why (FOM_WHY_* bits). A period is missed when its interrupt did not run (FOM_WHY_LATE:
       an interrupt's first line came more than 1.5 periods after the previous one's), when its duty was
       written after the period had ended (FOM_WHY_WRITE), when the ADC overwrote an unread result
       (FOM_WHY_OVF), or when 1,000 interrupts did not arrive before the guard time (FOM_WHY_SHORT).

   fom_sweep() finds the shortest period with no miss: from p_start (10 kHz) it doubles the period until a
   trial passes (only if p_start fails), halves it while trials pass, then searches between the last
   failing and the last passing period to one count, then confirms the result with two more trials,
   stepping up 1 percent (at least one count) after a failed confirmation, at most 8 times.
   Highest rate = timer clock / p_pass.

   Result: an array of FOM_WORDS 32-bit words, read by the debugger (MSPM0, C2000) or printed as
   I,fom lines in the OpenOCD mdw format (CC23xx, CC13xx). tools/results/fom_decode.py turns it into rows. */
#ifndef FOM_H
#define FOM_H
#include <stdint.h>

#define FOM_MAGIC      0xF0E50001UL     /* word 0, written last: the program finished */
#define FOM_WORDS      48
#define FOM_N_IRQ      1000UL

/* header words */
#define FOM_MAGIC_I    0
#define FOM_CPU_HZ     1                /* CPU clock, Hz */
#define FOM_TIMER_HZ   2                /* trigger-timer clock, Hz: the unit of every period below */
#define FOM_P100K      3                /* trigger-timer counts in a 100 kHz period */
#define FOM_LOOP_MIN   4                /* main-loop poll iteration, cycles, shortest of 1,000 with interrupts off */
#define FOM_LOOP_MAX   5                /* the same, longest */
#define FOM_N_MATH     6                /* blocks filled: 1 or 2 */
#define FOM_LAYOUT     7                /* 1: this layout */

/* block k (one per math type or build) starts at word FOM_BLK(k), k = 0 or 1 */
#define FOM_BLK(k)     (8 + 20 * (k))
#define FOM_MATH       0                /* power loop: 0 float, 1 Q15; control loop: 16 + the program's variant number */
#define FOM_P_PASS     1                /* sweep: shortest period with no miss in 1,000 (0: none passed) */
#define FOM_P_FAIL     2                /* sweep: longest period that missed (0: none missed down to p_min) */
#define FOM_TRIALS     3                /* sweep: trials run */
#define FOM_CONFIRM    4                /* sweep: confirmation trials at p_pass that passed, of 2 */
#define FOM_WHY_FAIL   5                /* sweep: FOM_WHY_* bits of the trial at p_fail */
#define FOM_LOAD_PER   6                /* load pass: period used (FOM_P100K, or p_pass when the chip cannot hold 100 kHz) */
#define FOM_IRQS       7                /* load pass: interrupts counted (1,000) */
#define FOM_LATE       8                /* load pass: interrupts more than 1.5 periods after the one before */
#define FOM_WR_LATE    9                /* load pass: duty written after its period ended */
#define FOM_OVF        10               /* load pass: ADC overflow seen (count or flag) */
#define FOM_GAPS       11               /* load pass: times the main loop lost the CPU (expect 1,000) */
#define FOM_GAP_SUM    12               /* load pass: cycles away from the main loop, summed: interrupt entry, body and return */
#define FOM_GAP_MAX    13
#define FOM_BODY_SUM   14               /* load pass: cycles from the interrupt's first line to its last line, summed */
#define FOM_BODY_MAX   15
#define FOM_BODY_MIN   16
#define FOM_WORK_SUM   17               /* load pass: first line to the end of the loop work (flags cleared), before our checks */
#define FOM_WORK_MAX   18
#define FOM_STEPS      19               /* sweep: 1 percent steps the confirmation needed (0 = confirmed at once) */

#define FOM_WHY_SHORT  1UL
#define FOM_WHY_LATE   2UL
#define FOM_WHY_WRITE  4UL
#define FOM_WHY_OVF    8UL

typedef int (*fom_trial_fn)(uint32_t period, uint32_t *why);

static inline void fom_sweep(volatile uint32_t *blk, fom_trial_fn trial, uint32_t p_start, uint32_t p_min, uint32_t p_max)
{
	uint32_t pass = 0, fail = 0, p = p_start, why = 0, why_fail = 0, n = 0, conf = 0, steps = 0;

	/* 1. go slower from p_start until a trial passes */
	for (;;) {
		n++;
		why = 0;
		if (trial(p, &why)) { pass = p; break; }
		fail = p; why_fail = why;
		if (p > p_max / 2UL) break;
		p *= 2UL;
	}
	if (pass) {
		/* 2. go faster by halving while trials pass (only when p_start itself passed) */
		if (!fail) {
			while (pass / 2UL >= p_min) {
				p = pass / 2UL; n++; why = 0;
				if (trial(p, &why)) { pass = p; } else { fail = p; why_fail = why; break; }
			}
		}
		/* 3. binary search to one count between the failing and the passing period */
		while (fail && pass - fail > 1UL) {
			p = fail + (pass - fail) / 2UL; n++; why = 0;
			if (trial(p, &why)) { pass = p; } else { fail = p; why_fail = why; }
		}
		/* 4. confirm: two more passes at the result, else one percent slower and again */
		for (;;) {
			conf = 0;
			why = 0; n++; if (trial(pass, &why)) conf++;
			why = 0; n++; if (trial(pass, &why)) conf++;
			if (conf == 2UL || steps >= 8UL) break;
			pass += (pass / 100UL) ? pass / 100UL : 1UL;
			steps++;
			if (pass > p_max) { pass = 0; break; }
		}
	}
	blk[FOM_P_PASS] = pass;
	blk[FOM_P_FAIL] = fail;
	blk[FOM_TRIALS] = n;
	blk[FOM_CONFIRM] = conf;
	blk[FOM_WHY_FAIL] = why_fail;
	blk[FOM_STEPS] = steps;
}

/* The load pass runs at 100 kHz when the chip holds 100 kHz with room to spare (its highest rate is at least
   200 kHz), else at half the highest rate it held (twice the period), capped at the 16-bit timers' longest
   period. At the highest rate itself the handler with the load counting can leave the main loop too little
   time to see each interrupt (CC2340R5, 2026-10-08). The rate used is in FOM_LOAD_PER and in every report. */
#define FOM_LOAD_P_MAX 65535UL
static inline uint32_t fom_load_period(volatile uint32_t *blk, uint32_t p100k)
{
	uint32_t p = blk[FOM_P_PASS];
	if (p == 0UL) return 0UL;
	p = (p > FOM_LOAD_P_MAX / 2UL) ? FOM_LOAD_P_MAX : 2UL * p;
	return p > p100k ? p : p100k;
}

#endif
