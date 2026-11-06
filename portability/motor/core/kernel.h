/* kernel.h: the control-loop pieces, static inline, in frl_real. Clarke, Park, inverse Park, PI with conditional
   integration, min-max modulation and a ramp. No vendor include, no state outside the structs passed in. */
#ifndef FRL_KERNEL_H
#define FRL_KERNEL_H
#include "frl_num.h"

#define FRL_K_INV_SQRT3 FRL_R(0.57735026919)     /* 1 / sqrt(3) */
#define FRL_K_SQRT3_2   FRL_R(0.86602540378)     /* sqrt(3) / 2 */

/* Clarke: ia + ib + ic = 0 assumed; alpha = ia, beta = (ia + 2 ib) / sqrt(3). */
static inline void frl_clarke(frl_real ia, frl_real ib, frl_real *al, frl_real *be)
{
	*al = ia;
	*be = FRL_MUL(ia + ib + ib, FRL_K_INV_SQRT3);
}
/* Park: s, c are sin and cos of the rotor (d-axis) angle. */
static inline void frl_park(frl_real al, frl_real be, frl_real s, frl_real c, frl_real *d, frl_real *q)
{
	*d = FRL_MUL(al, c) + FRL_MUL(be, s);
	*q = FRL_MUL(be, c) - FRL_MUL(al, s);
}
static inline void frl_ipark(frl_real d, frl_real q, frl_real s, frl_real c, frl_real *al, frl_real *be)
{
	*al = FRL_MUL(d, c) - FRL_MUL(q, s);
	*be = FRL_MUL(d, s) + FRL_MUL(q, c);
}

/* PI with conditional integration: the integrator is frozen while the output is saturated and the error pushes
   further into the limit. kp in V/A (or the loop's units); ki is per sample (Ki x Ts). */
typedef struct { frl_real kp, ki, integ, lo, hi; } frl_pi_t;
static inline frl_real frl_pi(frl_pi_t *p, frl_real err)
{
	frl_real u = FRL_MUL(p->kp, err) + p->integ, out = u;
	if (u > p->hi) out = p->hi; else if (u < p->lo) out = p->lo;
	if (out == u || (u > p->hi && err < 0) || (u < p->lo && err > 0)) {
		p->integ += FRL_MUL(p->ki, err);
		if (p->integ > p->hi) p->integ = p->hi; else if (p->integ < p->lo) p->integ = p->lo;
	}
	return out;
}

/* Voltage limit for the q-axis from what the d-axis used: |vq| <= sqrt(vmax^2 - vd^2). Circle limit, one square root;
   the box limit (each axis +/- vmax) is the PI's own lo/hi and costs nothing. */
static inline frl_real frl_vq_limit(frl_real vmax, frl_real vd)
{
	frl_real r = FRL_MUL(vmax, vmax) - FRL_MUL(vd, vd);
	return r > 0 ? FRL_SQRT(r) : 0;
}

/* Min-max (third-harmonic) modulation: alpha-beta volts to three duties in 0..1, centre-aligned, clamped.
   inv_vbus is 1 / bus volts. */
static inline void frl_modulate(frl_real val, frl_real vbe, frl_real inv_vbus, frl_real *da, frl_real *db, frl_real *dc)
{
	frl_real h = FRL_HALF(val), t = FRL_MUL(vbe, FRL_K_SQRT3_2);
	frl_real va = val, vb = t - h, vc = -t - h;
	frl_real mx = va > vb ? va : vb, mn = va < vb ? va : vb;
	if (vc > mx) mx = vc; else if (vc < mn) mn = vc;
	frl_real off = FRL_HALF(mx + mn), a, b, c;
	a = FRL_HALF_ONE + FRL_MUL(va - off, inv_vbus);
	b = FRL_HALF_ONE + FRL_MUL(vb - off, inv_vbus);
	c = FRL_HALF_ONE + FRL_MUL(vc - off, inv_vbus);
	*da = a < 0 ? 0 : (a > FRL_R(1.0) ? FRL_R(1.0) : a);
	*db = b < 0 ? 0 : (b > FRL_R(1.0) ? FRL_R(1.0) : b);
	*dc = c < 0 ? 0 : (c > FRL_R(1.0) ? FRL_R(1.0) : c);
}

/* Ramp: moves cur toward target by at most step per call. */
typedef struct { frl_real cur, step; } frl_ramp_t;
static inline frl_real frl_ramp(frl_ramp_t *r, frl_real target)
{
	if (target > r->cur + r->step) r->cur += r->step;
	else if (target < r->cur - r->step) r->cur -= r->step;
	else r->cur = target;
	return r->cur;
}

/* duty 0..1 to timer counts for a period of n counts (n below 2^12 in integer builds) */
#if FRL_NUM == FRL_NUM_FLOAT
#define FRL_DUTY_COUNTS(d, n) ((uint32_t)((d) * (float)(n)))
#else
#define FRL_DUTY_COUNTS(d, n) ((uint32_t)((((d) >> (FRL_FRAC - 12)) * (int32_t)(n)) >> 12))
#endif

#endif
