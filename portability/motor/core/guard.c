/* guard.c: the background half of the guard (init, limit writes, start refusals, tick checks, reaction table). */
#include "guard.h"

void frl_guard_init(frl_guard_t *g, const frl_guard_cfg_t *cfg)
{
	g->cfg = cfg;
	g->ilimit = cfg->ilimit_def; g->cap_ms = cfg->cap_def_ms; g->hb_ms = cfg->hb_def_ms; g->heartbeat = 0;
	g->run_ilimit = cfg->ilimit_def; g->run_cap_ms = cfg->cap_def_ms; g->run_hb_ms = cfg->hb_def_ms;
	g->state = FRL_G_IDLE; g->reason = FRL_R_NONE;
	g->isr_count = 0; g->tick_isr_snap = 0; g->last_cycles = 0; g->ms_acc = 0; g->run_ms = 0;
	g->hb_seen = 0; g->hb_since_ms = 0; g->win_ms = 0; g->win_isr_snap = 0;
}

/* above the ceiling: the ceiling; zero or below: the default */
void frl_guard_set_ilimit(frl_guard_t *g, frl_real v)
{
	g->ilimit = v <= 0 ? g->cfg->ilimit_def : (v > g->cfg->ilimit_ceil ? g->cfg->ilimit_ceil : v);
}
void frl_guard_set_cap_ms(frl_guard_t *g, uint32_t ms)
{
	g->cap_ms = ms == 0 ? g->cfg->cap_def_ms : (ms > g->cfg->cap_ceil_ms ? g->cfg->cap_ceil_ms : ms);
}
void frl_guard_set_hb_ms(frl_guard_t *g, uint32_t ms)
{
	g->hb_ms = ms == 0 ? g->cfg->hb_def_ms : (ms > g->cfg->hb_ceil_ms ? g->cfg->hb_ceil_ms : ms);
}

frl_reason_t frl_guard_start(frl_guard_t *g, const frl_guard_start_t *in, uint32_t cycles)
{
	const frl_guard_cfg_t *c = g->cfg;
	frl_reason_t r = FRL_R_NONE;
	if (in->drv_fault) r = FRL_R_REFUSED_FAULT;
	else if (in->vbus < c->vbus_lo || in->vbus > c->vbus_hi) r = FRL_R_REFUSED_BUS;
	else if (!in->cal_ok) r = FRL_R_REFUSED_UNCAL;
	else if (FRL_ABS(in->i[0]) > c->offset_max || FRL_ABS(in->i[1]) > c->offset_max || FRL_ABS(in->i[2]) > c->offset_max)
		r = FRL_R_REFUSED_OFFSET;
	else if (!in->cfg_ok) r = FRL_R_REFUSED_CFG;
	if (r != FRL_R_NONE) { g->reason = r; return r; }
	g->run_ilimit = g->ilimit; g->run_cap_ms = g->cap_ms; g->run_hb_ms = g->hb_ms;   /* latched */
	g->isr_count = 0; g->tick_isr_snap = 0;
	g->last_cycles = cycles; g->ms_acc = 0; g->run_ms = 0;
	g->hb_seen = g->heartbeat; g->hb_since_ms = 0; g->win_ms = 0; g->win_isr_snap = 0;
	g->reason = FRL_R_NONE; g->state = FRL_G_RUN;
	return FRL_R_NONE;
}

frl_reason_t frl_guard_tick(frl_guard_t *g, uint32_t cycles)
{
	const frl_guard_cfg_t *c = g->cfg;
	frl_reason_t r = FRL_R_NONE;
	if (g->state != FRL_G_RUN) return g->state == FRL_G_TRIPPED ? g->reason : FRL_R_NONE;
	uint32_t dt = cycles - g->last_cycles;                   /* wraps correctly while ticks are under 2^32 cycles apart */
	g->last_cycles = cycles;
	g->ms_acc += dt;
	while (g->ms_acc >= c->cycles_per_ms) { g->ms_acc -= c->cycles_per_ms; g->run_ms++; }
	g->tick_isr_snap = g->isr_count;                          /* the background loop is alive */
	if (g->run_ms >= g->run_cap_ms) r = FRL_R_TRIP_CAP;
	if (g->heartbeat != g->hb_seen) { g->hb_seen = g->heartbeat; g->hb_since_ms = g->run_ms; }
	else if (r == FRL_R_NONE && g->run_ms - g->hb_since_ms > g->run_hb_ms) r = FRL_R_TRIP_HEARTBEAT;
	if (r == FRL_R_NONE && g->run_ms - g->win_ms >= c->clock_win_ms) {   /* clock against ISR count */
		uint32_t span = g->run_ms - g->win_ms, got = g->isr_count - g->win_isr_snap;
		uint32_t want = span * (c->isr_hz / 1000u);
		uint32_t diff = got > want ? got - want : want - got;
		if ((uint64_t)diff * 100u > (uint64_t)want * c->clock_tol_pct) r = FRL_R_TRIP_CLOCK;
		g->win_ms = g->run_ms; g->win_isr_snap = g->isr_count;
	}
	if (r != FRL_R_NONE) { g->state = FRL_G_TRIPPED; g->reason = r; }
	return r;
}

void frl_guard_stop(frl_guard_t *g) { g->state = FRL_G_IDLE; }

/* Default reaction table (a board may replace it): bit 0 = low-side brake instead of coast, bit 1 = the host may retry.
   Faults where the power stage or the sensing may be wrong (current, driver, bus, non-finite, clock, stall) coast and
   latch; a run cap or host silence is a clean condition: bounded brake, retry allowed. */
uint_least8_t frl_guard_reaction(frl_reason_t r)
{
	static const uint_least8_t t[FRL_R_COUNT] = {
		0,                    /* none */
		0, 0, 0, 0, 0,        /* refusals: nothing was running */
		0,                    /* trip: current limit */
		0,                    /* trip: non-finite */
		3,                    /* trip: run cap */
		3,                    /* trip: host silence */
		0, 0, 0, 0 };         /* trip: stall, clock, driver, bus */
	return r < FRL_R_COUNT ? t[r] : 0u;
}
