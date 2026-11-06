/* frl_num.h: one number format per build. frl_real is float, or an int32_t with 15 (Q15) or 24 (IQ24) fraction bits.
   Angles are per-unit turns, 0 to 1 (FRL_WRAP keeps them there; integer builds wrap with a mask). Currents, voltages
   and times are in SI units; an integer build must keep |x| below 2^(31 - fraction bits): 128 for IQ24, 65,536 for Q15.
   FRL_SINCOS picks one backend per build: libm, a 256-entry interpolated table, or a hook for a trig unit
   (FRL_TRIG_TMU) or a math accelerator (FRL_TRIG_MATHACL); the board header defines FRL_UNIT_SINCOS(a, sp, cp). */
#ifndef FRL_NUM_H
#define FRL_NUM_H
#include <stdint.h>
#include "frl_cfg.h"
#include <math.h>

#define FRL_TRIG_LIBM    0
#define FRL_TRIG_TABLE   1
#define FRL_TRIG_TMU     2
#define FRL_TRIG_MATHACL 3
#ifndef FRL_TRIG
#if FRL_NUM == FRL_NUM_FLOAT
#define FRL_TRIG FRL_TRIG_LIBM
#else
#define FRL_TRIG FRL_TRIG_TABLE
#endif
#endif
#if (FRL_TRIG == FRL_TRIG_TMU || FRL_TRIG == FRL_TRIG_MATHACL) && !defined(FRL_UNIT_SINCOS)
#error "FRL_TRIG_TMU and FRL_TRIG_MATHACL need the board to define FRL_UNIT_SINCOS(a, sp, cp)"
#endif

#if FRL_NUM == FRL_NUM_FLOAT
typedef float frl_real;
#define FRL_FRAC        0
#define FRL_R(x)        ((float)(x))
#define FRL_MUL(a, b)   ((a) * (b))
#define FRL_DIV(a, b)   ((a) / (b))
#define FRL_HALF(a)     ((a) * 0.5f)
#define FRL_TOF(a)      (a)
#define FRL_SQRT(a)     sqrtf(a)
#define FRL_NONFINITE(x) (!((x) - (x) == 0.0f))    /* NaN and infinity both fail x - x == 0 */
static inline frl_real frl_wrap(frl_real a) { if (a >= 1.0f) a -= 1.0f; else if (a < 0.0f) a += 1.0f;
	if (a >= 1.0f) a -= 1.0f; else if (a < 0.0f) a += 1.0f; return a; }
#define FRL_ANG_FROM_T32(x) ((float)((uint32_t)(x) >> 8) * (1.0f / 16777216.0f))
#else
typedef int32_t frl_real;
#if FRL_NUM == FRL_NUM_Q15
#define FRL_FRAC 15
#else
#define FRL_FRAC 24
#endif
#define FRL_R(x)        ((frl_real)((x) * (double)(1L << FRL_FRAC) + ((x) >= 0 ? 0.5 : -0.5)))
#define FRL_MUL(a, b)   ((frl_real)(((int64_t)(a) * (int64_t)(b)) >> FRL_FRAC))
#define FRL_DIV(a, b)   ((frl_real)((((int64_t)(a)) << FRL_FRAC) / (int64_t)(b)))
#define FRL_HALF(a)     ((a) >> 1)
#define FRL_TOF(a)      ((float)(a) * (1.0f / (float)(1L << FRL_FRAC)))
#define FRL_NONFINITE(x) (0)
static inline frl_real frl_wrap(frl_real a) { return a & (frl_real)((1L << FRL_FRAC) - 1); }
#define FRL_ANG_FROM_T32(x) ((frl_real)((uint32_t)(x) >> (32 - FRL_FRAC)))
static inline frl_real FRL_SQRT(frl_real a)   /* square root of a >= 0, same format: bitwise on a 64-bit radicand */
{
	uint64_t n = (uint64_t)(a < 0 ? 0 : a) << FRL_FRAC, r = 0, b = (uint64_t)1 << 62;
	while (b > n) b >>= 2;
	while (b) { if (n >= r + b) { n -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
	return (frl_real)r;
}
#endif

#define FRL_ABS(a)      ((a) < 0 ? -(a) : (a))
#define FRL_HALF_ONE    FRL_HALF(FRL_R(1.0))
#define FRL_WRAP(a)     frl_wrap(a)

/* sin and cos of a per-unit angle a (0 to 1) */
#if FRL_TRIG == FRL_TRIG_TABLE
#include "frl_sintab.h"
static inline frl_real frl_sin_tab(frl_real a)
{
#if FRL_NUM == FRL_NUM_FLOAT
	if (a >= 1.0f) a -= 1.0f;               /* an angle that rounded up to 1.0f is angle 0 */
	if (a < 0.0f) a = 0.0f;
	float x = a * 256.0f; int32_t i = (int32_t)x; float fr = x - (float)i;
	float t0 = (float)frl_sintab_q24[i], t1 = (float)frl_sintab_q24[i + 1];
	return (t0 + (t1 - t0) * fr) * (1.0f / 16777216.0f);
#else
	uint32_t u = (uint32_t)a & ((1UL << FRL_FRAC) - 1UL);
	uint32_t i = u >> (FRL_FRAC - 8), fr = u & ((1UL << (FRL_FRAC - 8)) - 1UL);
	int32_t w = (FRL_FRAC >= 20) ? (int32_t)(fr >> (FRL_FRAC - 20)) : (int32_t)(fr << (20 - FRL_FRAC));   /* 12-bit weight */
	int32_t t0 = frl_sintab_q24[i], d = frl_sintab_q24[i + 1] - t0;
	int32_t v = t0 + ((d * w) >> 12);                       /* d < 2^19, w < 2^12: fits int32 */
	return (FRL_FRAC >= 24) ? (frl_real)v : (frl_real)(v >> (24 - FRL_FRAC));
#endif
}
static inline void frl_sincos(frl_real a, frl_real *s, frl_real *c)
{
	*s = frl_sin_tab(a);
	*c = frl_sin_tab(FRL_WRAP(a + FRL_R(0.25)));
}
#elif FRL_TRIG == FRL_TRIG_LIBM
static inline void frl_sincos(frl_real a, frl_real *s, frl_real *c)
{
	float x = 6.28318530718f * FRL_TOF(a), sv = sinf(x), cv = cosf(x);
	*s = FRL_R(sv); *c = FRL_R(cv);
}
#else
static inline void frl_sincos(frl_real a, frl_real *s, frl_real *c) { FRL_UNIT_SINCOS(a, s, c); }
#endif
#define FRL_SINCOS(a, sp, cp) frl_sincos((a), (sp), (cp))

#endif
