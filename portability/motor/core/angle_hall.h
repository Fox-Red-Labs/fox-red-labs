/* angle_hall.h: Hall decode, six-step pattern, and the edge tracker.
   Angles here are 32-bit electrical turns (2^32 = one turn, unsigned, wraps for free); speed is turns per ISR as int32_t.
   frl_hall_angle() gives the angle the kernel reads (FRL_ANG_FROM_T32 converts it to frl_real).

   Tracker (the logic of the guarded AM13 image's hall_pll() at pole 0, written without a vendor include):
     predict   every ISR th += w. The travel since the last edge is bounded at 1.2 sectors; past it the rotor is slower
               than w, so |w| = that travel / the time since the edge and th holds (the one division between edges).
     edge      the step in the forward order 3, 2, 6, 4, 5, 1 gives the direction; the edge angle is the table's (forward
               or backward entry) plus the mounting offset, moved on by w x the debounce delay (D - 0.5 ISRs for D agreeing
               samples).
               first edge after standstill: th = edge, w = 0;
               second edge the same way: w = the angle between the two edges / the time between them;
               a reversal: w = 0 (never the old sign), th = edge, second-edge rule again; if the next edge goes back within
               a quarter sector at the speed held before, it was a bounce and that speed comes back;
               tracking: th snaps to the edge (clamped to one sector), w += error / time since the last edge.
     standstill no edge for still_isrs ISRs: th = the sector middle, w = 0.
   Between edges the cost is one add, one compare and the debounce counters; no division. */
#ifndef FRL_ANGLE_HALL_H
#define FRL_ANGLE_HALL_H
#include <stdint.h>
#include "frl_cfg.h"

#define FRL_T32_SECT   715827883UL     /* 2^32 / 6: 60 electrical degrees */
#define FRL_T32_LIM    858993459UL     /* 1.2 sectors */
#define FRL_T32_BOUNCE 178956971UL     /* 0.25 sector */
#define FRL_HALL_NONE  0xFFFFFFFFUL    /* te: no edge seen since standstill */
#ifndef FRL_HALL_DEBOUNCE
#define FRL_HALL_DEBOUNCE 3            /* agreeing samples before a state change is accepted */
#endif
#define FRL_HALL_DELAY_X2 (2 * FRL_HALL_DEBOUNCE - 1)   /* mean ISRs from pin edge to acceptance, doubled */

/* Hall bits: A = 1, B = 2, C = 4. Position of each state in the forward order 3, 2, 6, 4, 5, 1; -1 = invalid. */
static const int_least8_t frl_hall_idx[8] = { -1, 5, 1, 0, 3, 4, 2, -1 };

typedef struct { uint32_t fwd[8], rev[8]; } frl_hall_tab_t;   /* angle on entering a state going forward / backward */

/* Nominal table: entering sector k forward is at k/6 turn, backward at (k+1)/6. A bench table replaces it. */
static inline void frl_hall_tab_nominal(frl_hall_tab_t *t)
{
	for (int s = 0; s < 8; s++) {
		int k = frl_hall_idx[s] < 0 ? 0 : frl_hall_idx[s];
		t->fwd[s] = (uint32_t)k * (uint32_t)FRL_T32_SECT;
		t->rev[s] = (uint32_t)(k + 1) * (uint32_t)FRL_T32_SECT;
	}
}
/* The same table as a constant initializer, for a const object in flash. */
#define FRL_H_F(k) ((uint32_t)((k) * (uint32_t)FRL_T32_SECT))
#define FRL_HALL_TAB_NOMINAL { { 0, FRL_H_F(5), FRL_H_F(1), FRL_H_F(0), FRL_H_F(3), FRL_H_F(4), FRL_H_F(2), 0 }, \
                               { 0, FRL_H_F(6), FRL_H_F(2), FRL_H_F(1), FRL_H_F(4), FRL_H_F(5), FRL_H_F(3), 0 } }
/* Angle of the middle of a state's sector (the standstill angle, and the angle of an untracked Hall FOC). */
static inline uint32_t frl_hall_mid(const frl_hall_tab_t *t, unsigned s, uint32_t offset)
{
	return t->fwd[s & 7] + (uint32_t)((int32_t)(t->rev[s & 7] - t->fwd[s & 7]) >> 1) + offset;
}

/* Six-step pattern: for a Hall state and direction (+1 or -1), the high-side phase and the low-side phase (0 = A, 1 = B,
   2 = C); the third phase floats. Generic 120-degree pattern, 6 entries per direction; the sensor-to-phase wiring is a
   board constant, so a board corrects it by replacing the table. */
static const uint_least8_t frl_sixstep_fwd[8][2] = { {0,0},{0,2},{1,0},{1,2},{2,1},{0,1},{2,0},{0,0} };   /* hi, lo by state */
static const uint_least8_t frl_sixstep_rev[8][2] = { {0,0},{2,1},{0,2},{0,1},{1,0},{2,0},{1,2},{0,0} };
/* returns 0 for an invalid state, else 1 and sets *hi, *lo */
static inline int frl_sixstep_hall(unsigned s, int dir, unsigned *hi, unsigned *lo)
{
	if (s > 7 || frl_hall_idx[s] < 0) return 0;
	const uint_least8_t *e = dir >= 0 ? frl_sixstep_fwd[s] : frl_sixstep_rev[s];
	*hi = e[0]; *lo = e[1];
	return 1;
}

typedef struct {
	const frl_hall_tab_t *tab;
	uint32_t offset;                 /* mounting offset, turn32 */
	uint32_t still_isrs;             /* no edge for this many ISRs: standstill (about 1.7 Hz electrical at 20 kHz: 2000) */
	uint32_t th, adv, te, lastea;    /* angle; travel since the edge; ISRs since the edge; last edge angle */
	int32_t  w, wsave;               /* speed (turns per ISR, signed); speed held before a reversal */
	uint_least8_t  cand, cnt, cur, prev, n, pend, fault;
	int_least8_t   dir, dirsave;
} frl_hall_t;

static inline void frl_hall_init(frl_hall_t *h, const frl_hall_tab_t *tab, uint32_t offset, uint32_t still_isrs)
{
	h->tab = tab; h->offset = offset; h->still_isrs = still_isrs;
	h->th = 0; h->adv = 0; h->te = FRL_HALL_NONE; h->lastea = 0; h->w = 0; h->wsave = 0;
	h->cand = 0; h->cnt = 0; h->cur = 0; h->prev = 0; h->n = 0; h->pend = 0; h->fault = 0; h->dir = 1; h->dirsave = 1;
}

/* one call per control ISR with the three raw Hall bits */
static inline void frl_hall_step(frl_hall_t *h, unsigned bits)
{
	uint32_t th = h->th;
	int32_t  w = h->w;
	int edge = 0;

	if ((bits & 7u) == h->cand) { if (h->cnt < 255) h->cnt++; } else { h->cand = (uint_least8_t)(bits & 7u); h->cnt = 1; }
	if (h->cnt >= FRL_HALL_DEBOUNCE && h->cand != h->cur) {
		if (frl_hall_idx[h->cand] >= 0) { h->prev = h->cur; h->cur = h->cand; h->fault = 0; edge = 1; }
		else h->fault = 1;
	}

	if (h->te != FRL_HALL_NONE && h->te < FRL_HALL_NONE - 1) h->te++;
	th += (uint32_t)w;
	h->adv += (uint32_t)(w < 0 ? -w : w);
	if (h->adv > FRL_T32_LIM) {      /* an edge is overdue: hold the angle; the rotor is at most this fast */
		uint32_t back = h->adv - FRL_T32_LIM;
		if (w > 0) th -= back; else th += back;
		h->adv = FRL_T32_LIM;
		if (h->te != FRL_HALL_NONE && h->te > 0) {
			int32_t m = (int32_t)(FRL_T32_LIM / h->te);
			w = w > 0 ? m : -m;
		}
	}

	if (edge && h->prev >= 1 && h->prev <= 6) {
		unsigned sn = h->cur, sp = h->prev;
		int step = frl_hall_idx[sn] - frl_hall_idx[sp];
		if (step < 0) step += 6;
		if (step == 3) {             /* opposite sector: direction unknown; start again from the sector middle */
			h->n = 0; h->pend = 0;
			th = frl_hall_mid(h->tab, sn, h->offset);
		} else {
			int_least8_t sgn = (step <= 2) ? 1 : -1;
			uint32_t ea = (sgn > 0 ? h->tab->fwd[sn] : h->tab->rev[sn]) + h->offset + (uint32_t)((w >> 1) * FRL_HALL_DELAY_X2);
			int32_t e = (int32_t)(ea - th);
			if (h->n == 0) {                       /* first edge after standstill: direction and angle, no speed yet */
				th = ea; w = 0; h->n = 1; h->dir = sgn; h->pend = 0;
			} else if (sgn != h->dir) {            /* a reversal */
				uint32_t aw = (uint32_t)(h->wsave < 0 ? -h->wsave : h->wsave);
				if (h->pend && sgn == h->dirsave && (uint64_t)h->te * aw < FRL_T32_BOUNCE) {
					w = h->wsave; th = ea; h->n = 2; h->dir = sgn; h->pend = 0;   /* back within a quarter sector: a bounce */
				} else {
					h->wsave = w; h->dirsave = h->dir; h->pend = 1;
					th = ea; w = 0; h->n = 1; h->dir = sgn;
				}
			} else {
				h->pend = 0;
				uint32_t te = h->te < 1 ? 1 : h->te;
				if (h->n == 1) {                   /* second edge the same way: the speed from the two edges */
					int32_t span = (int32_t)(ea - h->lastea) * sgn;
					if (span < (int32_t)(FRL_T32_SECT / 2)) span = (int32_t)(FRL_T32_SECT / 2);
					if (span > (int32_t)(FRL_T32_SECT + FRL_T32_SECT / 2)) span = (int32_t)(FRL_T32_SECT + FRL_T32_SECT / 2);
					w = (int32_t)sgn * (int32_t)((uint32_t)span / te);
					th = ea; h->n = 2;
				} else {                           /* tracking at pole 0: the angle snaps to the edge, w gains error / interval */
					int32_t ec = e > (int32_t)FRL_T32_SECT ? (int32_t)FRL_T32_SECT : (e < -(int32_t)FRL_T32_SECT ? -(int32_t)FRL_T32_SECT : e);
					th += (uint32_t)ec;
					w += ec / (int32_t)te;
				}
			}
			h->lastea = ea;
		}
		h->te = 0; h->adv = 0;
	}
	if (h->cur >= 1 && h->cur <= 6 && h->te >= h->still_isrs) {   /* standstill */
		th = frl_hall_mid(h->tab, h->cur, h->offset);
		w = 0; h->n = 0; h->pend = 0;
	}
	h->th = th; h->w = w;
}
static inline uint32_t frl_hall_angle(const frl_hall_t *h) { return h->th; }
static inline int32_t  frl_hall_speed(const frl_hall_t *h) { return h->w; }

#endif
