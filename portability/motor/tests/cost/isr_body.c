/* isr_body.c: a tiny control ISR per tier, for the cost matrix. The "registers" are volatile words, so the loads and
   stores are the ones a board's HAL produces; the HAL's own setup code is not here. Variants are chosen by -D flags
   (see build.sh): FRL_TIER, FRL_F_GUARD, FRL_F_HALL_TRACK, FRL_ANGLE_FROM_HALL_SECTOR. */
#include <stdint.h>
extern volatile uint32_t HW_ADC[4], HW_PWM[4], HW_HALL, HW_CYC, HW_DRV;
#define FRL_CYCLES() HW_CYC
#include "kernel.h"
#include "angle_hall.h"
#include "guard.h"
#include "cost.h"

typedef struct {
	frl_cost_t cost;
	int32_t    off[3];
	frl_real   ks, kv, id_ref, iq_ref, inv_vbus;
	uint32_t   period, hoff;
	int_least8_t     dir;
	frl_real   ia, ib, vbus;
#if FRL_F_FOC
	frl_pi_t   pd, pq;
#endif
#if FRL_F_HALL_TRACK
	frl_hall_t hall;
#endif
} frl_state_t;
frl_state_t S;

#if FRL_F_GUARD
frl_guard_t G;
static const frl_guard_cfg_t GCFG = { FRL_R(3.0), FRL_R(1.25), FRL_R(8.0), FRL_R(24.0), FRL_R(0.2), 60000, 10000, 5000, 1000, 40, 20000, 160000, 100, 5 };
#endif
#if FRL_F_FOC
static const frl_hall_tab_t TAB = FRL_HALL_TAB_NOMINAL;
#endif

void frl_isr_init(void)
{
	S.ks = FRL_R(0.0006); S.kv = FRL_R(0.01); S.inv_vbus = FRL_R(0.05); S.period = 4000; S.dir = 1;
#if FRL_F_FOC
	{ frl_pi_t t = { FRL_R(6.6), FRL_R(0.2356), 0, FRL_R(-10.97), FRL_R(10.97) }; S.pd = t; S.pq = t; }
#endif
#if FRL_F_HALL_TRACK
	frl_hall_init(&S.hall, &TAB, 0, 2000);
#endif
#if FRL_F_GUARD
	frl_guard_init(&G, &GCFG);
#endif
}

void frl_isr(void)
{
	FRL_COST_BEGIN(S.cost);
	frl_real ia = (frl_real)((int32_t)HW_ADC[0] - S.off[0]) * S.ks;
	frl_real ib = (frl_real)((int32_t)HW_ADC[1] - S.off[1]) * S.ks;
	frl_real vbus = (frl_real)HW_ADC[3] * S.kv;
	S.ia = ia; S.ib = ib; S.vbus = vbus;
#if FRL_F_GUARD
	{
		frl_guard_in_t gi;
		gi.i[0] = ia; gi.i[1] = ib; gi.i[2] = -(ia + ib); gi.vbus = vbus;
		gi.drv_fault = (uint_least8_t)(HW_DRV & 1u); gi.nonfinite = (uint_least8_t)FRL_NONFINITE(ia + ib);
		if (frl_guard_isr(&G, &gi)) { HW_PWM[3] = 7u; FRL_COST_END(S.cost); return; }   /* all three phases off */
	}
#endif
#if FRL_F_SIXSTEP_HALL
	{
		unsigned hi, lo;
		if (frl_sixstep_hall(HW_HALL & 7u, S.dir, &hi, &lo)) {
			HW_PWM[hi] = S.period >> 1; HW_PWM[lo] = 0; HW_PWM[3] = 1u << (3u - hi - lo);
		} else HW_PWM[3] = 7u;
	}
#elif FRL_F_FOC
	{
		uint32_t th32;
		frl_real s, c, al, be, id, iq, vd, vq, val, vbe, da, db, dc;
#if FRL_F_HALL_TRACK
		frl_hall_step(&S.hall, HW_HALL & 7u);
		th32 = frl_hall_angle(&S.hall);
#else
		th32 = frl_hall_mid(&TAB, HW_HALL & 7u, S.hoff);
#endif
		FRL_SINCOS(FRL_ANG_FROM_T32(th32), &s, &c);
		frl_clarke(ia, ib, &al, &be);
		frl_park(al, be, s, c, &id, &iq);
		vd = frl_pi(&S.pd, S.id_ref - id);
		vq = frl_pi(&S.pq, S.iq_ref - iq);
		frl_ipark(vd, vq, s, c, &val, &vbe);
		frl_modulate(val, vbe, S.inv_vbus, &da, &db, &dc);
		HW_PWM[0] = FRL_DUTY_COUNTS(da, S.period); HW_PWM[1] = FRL_DUTY_COUNTS(db, S.period); HW_PWM[2] = FRL_DUTY_COUNTS(dc, S.period);
	}
#else
	HW_PWM[0] = S.period >> 2; HW_PWM[1] = S.period >> 2; HW_PWM[2] = S.period >> 2;   /* shell: a fixed duty */
#endif
	FRL_COST_END(S.cost);
}
