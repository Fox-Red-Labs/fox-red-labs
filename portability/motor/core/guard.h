/* guard.h / guard.c: the portable safety checks. Plain structs in, a reason code out; no hardware here. The caller
   (the board's ISR) switches the driver off when frl_guard_isr() returns nonzero; the fault reaction table says coast
   or brake. Concept proven on the AM13 bench image; the checks:

   refusals at a start   driver fault, bus outside its window, uncalibrated angle source, standstill currents off
                         zero, a configuration the host may not run
   stops during a run    phase current over the latched limit, non-finite value, run cap, host silence (a changing
                         heartbeat, not any byte), background stall (N ISRs without a tick), clock disagreement
                         (cycle counter against ISR count), driver fault, bus excursion
   limits                host-writable, clamped on write to a ceiling the firmware owns, latched at a start; an
                         out-of-range write reverts to the default
   Times come from a cycle counter read in the background tick, not from a tick that is assumed to be a millisecond. */
#ifndef FRL_GUARD_H
#define FRL_GUARD_H
#include <stdint.h>
#include "frl_num.h"

typedef enum {
	FRL_R_NONE = 0,
	FRL_R_REFUSED_FAULT, FRL_R_REFUSED_BUS, FRL_R_REFUSED_UNCAL, FRL_R_REFUSED_OFFSET, FRL_R_REFUSED_CFG,
	FRL_R_TRIP_ILIMIT, FRL_R_TRIP_NONFINITE, FRL_R_TRIP_CAP, FRL_R_TRIP_HEARTBEAT, FRL_R_TRIP_STALL,
	FRL_R_TRIP_CLOCK, FRL_R_TRIP_DRIVER, FRL_R_TRIP_BUS,
	FRL_R_COUNT
} frl_reason_t;
typedef enum { FRL_G_IDLE = 0, FRL_G_RUN, FRL_G_TRIPPED } frl_gstate_t;

/* what the firmware owns: ceilings, defaults, windows, clocks */
typedef struct {
	frl_real  ilimit_ceil, ilimit_def;        /* A: phase current trip */
	frl_real  vbus_lo, vbus_hi;               /* V: bus window */
	frl_real  offset_max;                     /* A: standstill phase current allowed at a start */
	uint32_t  cap_ceil_ms, cap_def_ms;        /* run cap */
	uint32_t  hb_ceil_ms, hb_def_ms;          /* host silence */
	uint32_t  stall_isrs;                     /* ISRs allowed between background ticks */
	uint32_t  isr_hz;                         /* control ISR rate, a multiple of 1000 */
	uint32_t  cycles_per_ms;                  /* cycle counter ticks per millisecond */
	uint32_t  clock_win_ms, clock_tol_pct;    /* clock cross-check window and tolerance */
} frl_guard_cfg_t;

typedef struct {
	const frl_guard_cfg_t *cfg;
	/* host-writable (through the set functions), latched at a start */
	frl_real  ilimit;  uint32_t cap_ms, hb_ms;  uint32_t heartbeat;
	frl_real  run_ilimit;  uint32_t run_cap_ms, run_hb_ms;
	/* state */
	frl_gstate_t state;  frl_reason_t reason;
	uint32_t isr_count;                        /* ISRs since the start (the ISR increments it) */
	uint32_t tick_isr_snap;                    /* isr_count at the last tick */
	uint32_t last_cycles, ms_acc, run_ms;      /* background time base */
	uint32_t hb_seen, hb_since_ms;
	uint32_t win_ms, win_isr_snap;
} frl_guard_t;

/* per-ISR input: the ISR fills it from its own samples */
typedef struct { frl_real i[3]; frl_real vbus; uint_least8_t drv_fault, nonfinite; } frl_guard_in_t;
/* start input */
typedef struct { frl_real i[3]; frl_real vbus; uint_least8_t drv_fault, cal_ok, cfg_ok; } frl_guard_start_t;

void         frl_guard_init(frl_guard_t *g, const frl_guard_cfg_t *cfg);
void         frl_guard_set_ilimit(frl_guard_t *g, frl_real v);
void         frl_guard_set_cap_ms(frl_guard_t *g, uint32_t ms);
void         frl_guard_set_hb_ms(frl_guard_t *g, uint32_t ms);
frl_reason_t frl_guard_start(frl_guard_t *g, const frl_guard_start_t *in, uint32_t cycles);
frl_reason_t frl_guard_tick(frl_guard_t *g, uint32_t cycles);      /* background, any rate faster than the heartbeat */
void         frl_guard_stop(frl_guard_t *g);                       /* a normal stop: back to idle */
/* 0 = coast, 1 = low-side brake; bit 1 set = the host may retry */
uint_least8_t      frl_guard_reaction(frl_reason_t r);

/* The ISR check, static inline. Returns the reason of a trip (0 = none) and latches the first one. */
static inline frl_reason_t frl_guard_isr(frl_guard_t *g, const frl_guard_in_t *in)
{
	frl_reason_t r = FRL_R_NONE;
	if (g->state != FRL_G_RUN) return FRL_R_NONE;
	g->isr_count++;
	if (in->nonfinite) r = FRL_R_TRIP_NONFINITE;
	else if (in->drv_fault) r = FRL_R_TRIP_DRIVER;
	else {
		frl_real lim = g->run_ilimit;
		/* fail closed: a NaN fails every comparison, so each check passes only when the value is provably inside its
		 * range. Build without -ffinite-math-only (or set in->nonfinite from a bit test) so the compiler keeps these. */
		if (!(FRL_ABS(in->i[0]) <= lim && FRL_ABS(in->i[1]) <= lim && FRL_ABS(in->i[2]) <= lim)) r = FRL_R_TRIP_ILIMIT;
		else if (!(in->vbus >= g->cfg->vbus_lo && in->vbus <= g->cfg->vbus_hi)) r = FRL_R_TRIP_BUS;
		else if (g->isr_count - g->tick_isr_snap > g->cfg->stall_isrs) r = FRL_R_TRIP_STALL;
	}
	if (r != FRL_R_NONE) { g->state = FRL_G_TRIPPED; g->reason = r; }
	return r;
}

#endif
