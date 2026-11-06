/* frl_cfg.h: tier, feature flags and the checks that refuse impossible builds.
   A build defines FRL_TIER (0 to 5) and may define any FRL_F_* flag to 0 or 1 before including this file; an
   undefined flag takes the tier's default. Every flag is an #if, never a runtime branch in the ISR. */
#ifndef FRL_CFG_H
#define FRL_CFG_H

/* number format: one switch per build (frl_num.h) */
#define FRL_NUM_FLOAT 0
#define FRL_NUM_Q15   1     /* int32_t holding 15 fraction bits */
#define FRL_NUM_IQ24  2     /* int32_t holding 24 fraction bits */
#ifndef FRL_NUM
#define FRL_NUM FRL_NUM_FLOAT
#endif

#ifndef FRL_TIER
#define FRL_TIER 2
#endif
#if FRL_TIER < 0 || FRL_TIER > 5
#error "FRL_TIER must be 0 to 5"
#endif
#if FRL_NUM != FRL_NUM_FLOAT && FRL_NUM != FRL_NUM_Q15 && FRL_NUM != FRL_NUM_IQ24
#error "FRL_NUM must be FRL_NUM_FLOAT, FRL_NUM_Q15 or FRL_NUM_IQ24"
#endif

/* Defaults by tier (cumulative). A profile adds or removes flags by defining them first.
   T0 open loop (profile adds BRUSHED or STEPPER) | T1 six-step Hall | T2 Hall FOC with edge tracker
   T3 sensorless FOC with identification | T4 field weakening, MTPA, flying start | T5 NPU hook */
#ifndef FRL_F_GUARD
#define FRL_F_GUARD 1
#endif
#ifndef FRL_F_COST
#define FRL_F_COST 1
#endif
#ifndef FRL_F_SIXSTEP_HALL
#define FRL_F_SIXSTEP_HALL (FRL_TIER == 1)
#endif
#ifndef FRL_F_FOC
#define FRL_F_FOC (FRL_TIER >= 2)
#endif
#ifndef FRL_F_HALL_TRACK
#define FRL_F_HALL_TRACK (FRL_TIER >= 2)
#endif
#ifndef FRL_F_OBS_FLUX
#define FRL_F_OBS_FLUX (FRL_TIER >= 3)
#endif
#ifndef FRL_F_IDENT
#define FRL_F_IDENT (FRL_TIER >= 3)
#endif
#ifndef FRL_F_FW
#define FRL_F_FW (FRL_TIER >= 4)
#endif
#ifndef FRL_F_MTPA
#define FRL_F_MTPA (FRL_TIER >= 4)
#endif
#ifndef FRL_F_FLYING
#define FRL_F_FLYING (FRL_TIER >= 4)
#endif
#ifndef FRL_F_NPU
#define FRL_F_NPU (FRL_TIER >= 5)
#endif
/* off unless a profile asks */
#ifndef FRL_F_SIXSTEP_BEMF
#define FRL_F_SIXSTEP_BEMF 0
#endif
#ifndef FRL_F_BRUSHED
#define FRL_F_BRUSHED 0
#endif
#ifndef FRL_F_STEPPER
#define FRL_F_STEPPER 0
#endif
#ifndef FRL_F_HALL_TABLE24
#define FRL_F_HALL_TABLE24 0
#endif
#ifndef FRL_F_OBS_SMO
#define FRL_F_OBS_SMO 0
#endif
#ifndef FRL_F_FF_SPEED
#define FRL_F_FF_SPEED 0
#endif
#ifndef FRL_F_GAIN_SCHED
#define FRL_F_GAIN_SCHED 0
#endif
#ifndef FRL_F_IPD
#define FRL_F_IPD 0
#endif
#ifndef FRL_F_BRAKE
#define FRL_F_BRAKE 0
#endif
#ifndef FRL_F_DTC
#define FRL_F_DTC 0
#endif
#ifndef FRL_F_CAPTURE
#define FRL_F_CAPTURE 0
#endif
#ifndef FRL_F_TABLE
#define FRL_F_TABLE 0
#endif

/* Combinations that cannot work are refused here, at build time. Flags that name code this slice does not
   contain yet (observers, ident, FW, MTPA, flying start, NPU) are accepted and checked, not implemented. */
#define FRL_ANGLE_SRC (FRL_F_HALL_TRACK || FRL_F_OBS_SMO || FRL_F_OBS_FLUX)

#if FRL_F_FOC && !FRL_ANGLE_SRC && !FRL_F_SIXSTEP_HALL && !defined(FRL_ANGLE_FROM_HALL_SECTOR)
/* FOC without a tracker, an observer or a Hall angle has no rotor angle. The Hall-sector angle build (T2 without
   the edge tracker) defines FRL_ANGLE_FROM_HALL_SECTOR. */
#error "FRL_F_FOC needs an angle source: FRL_F_HALL_TRACK, an observer, or FRL_ANGLE_FROM_HALL_SECTOR"
#endif
#if (FRL_F_BRUSHED + FRL_F_STEPPER + FRL_F_FOC + FRL_F_SIXSTEP_HALL + FRL_F_SIXSTEP_BEMF) > 1
#if FRL_F_BRUSHED || FRL_F_STEPPER
#error "FRL_F_BRUSHED and FRL_F_STEPPER cannot combine with FOC or six-step"
#endif
#if FRL_F_FOC && (FRL_F_SIXSTEP_HALL || FRL_F_SIXSTEP_BEMF)
#error "FRL_F_FOC and six-step are separate control paths: pick one per build"
#endif
#endif
#if (FRL_F_OBS_SMO || FRL_F_OBS_FLUX || FRL_F_IDENT || FRL_F_FW || FRL_F_MTPA || FRL_F_FLYING || FRL_F_DTC || FRL_F_IPD || FRL_F_HALL_TRACK) && !FRL_F_FOC
#error "observers, identification, FW, MTPA, flying start, DTC, IPD and the Hall tracker in the FOC path need FRL_F_FOC"
#endif
#if FRL_F_FLYING && !(FRL_F_HALL_TRACK || FRL_F_OBS_SMO || FRL_F_OBS_FLUX)
#error "FRL_F_FLYING needs a speed source (Hall tracker or observer)"
#endif
#if FRL_F_IPD && !(FRL_F_OBS_SMO || FRL_F_OBS_FLUX)
#error "FRL_F_IPD (initial position detection) is for the sensorless path: it needs an observer"
#endif
#if FRL_F_HALL_TABLE24 && !(FRL_F_HALL_TRACK || FRL_F_SIXSTEP_HALL)
#error "FRL_F_HALL_TABLE24 needs a Hall consumer (FRL_F_HALL_TRACK or FRL_F_SIXSTEP_HALL)"
#endif
#if FRL_F_NPU && !FRL_F_FOC
#error "FRL_F_NPU hooks the slow loops of a FOC build"
#endif

/* Float on a core without an FPU is not offered (6,444 to 13,725 cycles for the FOC step on the boards on hand). */
#if FRL_NUM == FRL_NUM_FLOAT && !defined(FRL_ALLOW_SOFT_FLOAT)
#if defined(__ARM_ARCH) && !defined(__ARM_FP)
#error "float frl_real on an Arm core without an FPU is not offered: use FRL_NUM_IQ24 or FRL_NUM_Q15"
#endif
#if (defined(__TMS320C28XX__) || defined(__TMS320C28X__)) && !defined(__TMS320C28XX_FPU32__)
#error "float frl_real on a C28x without FPU32 is not offered: use FRL_NUM_IQ24 or FRL_NUM_Q15"
#endif
#endif

#endif
