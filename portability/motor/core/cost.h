/* cost.h: cycle stamps for the control ISR. FRL_COST_BEGIN at the first statement, FRL_COST_END at the last.
   The board supplies FRL_CYCLES() (a free-running counter: DWT on M33 and M4, a 16-bit timer on M0+, the CPU timer on
   C28x) and, for a counter narrower than 32 bits, FRL_CYCLES_MASK (0xFFFF for a 16-bit timer). Interrupt entry is not
   inside the stamps. The mean is an IIR average kept x256 (time constant 32 ISRs), the maximum is exact; no division. */
#ifndef FRL_COST_H
#define FRL_COST_H
#include <stdint.h>
#include "frl_cfg.h"

#ifndef FRL_CYCLES_MASK
#define FRL_CYCLES_MASK 0xFFFFFFFFUL
#endif
typedef struct { uint32_t t0, last, max, mean256, n; } frl_cost_t;

#if FRL_F_COST
#define FRL_COST_BEGIN(c) ((c).t0 = (uint32_t)FRL_CYCLES())
#define FRL_COST_END(c) do { uint32_t d_ = ((uint32_t)FRL_CYCLES() - (c).t0) & FRL_CYCLES_MASK; (c).last = d_; \
	if (d_ > (c).max) (c).max = d_; \
	if ((c).n == 0) (c).mean256 = d_ << 8; else (c).mean256 = (uint32_t)((int32_t)(c).mean256 + (((int32_t)(d_ << 8) - (int32_t)(c).mean256) >> 5)); \
	(c).n++; } while (0)
#else
#define FRL_COST_BEGIN(c) ((void)0)
#define FRL_COST_END(c) ((void)0)
#endif
static inline uint32_t frl_cost_mean(const frl_cost_t *c) { return c->mean256 >> 8; }
static inline void frl_cost_reset(frl_cost_t *c) { c->t0 = 0; c->last = 0; c->max = 0; c->mean256 = 0; c->n = 0; }

#endif
