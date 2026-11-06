/*
 * Copyright (c) 2025 Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file motor_params.h
 * \brief Motor constant table — per-motor nameplate and tuning parameters
 *
 * \defgroup MOTOR_PARAMS_MODULE Motor Parameters Module
 * \brief Runtime-selectable motor constant tables.
 *
 * Each supported motor is described by a \c Motor_ParamsType instance stored
 * in flash.  The active motor is selected by pointing \c motorParams_M1 at
 * the desired table — no recompilation required.
 *
 * Per-motor compile-time macros (e.g. \c MOTOR_TEKNIC_Ls_d_H) are defined
 * below and used to populate the const struct tables in \c motor_params.c.
 * This keeps all motor data in one place and avoids bare literals in the
 * struct initializers.
 *
 * The \c MOTOR_PH_Ls_d_H / \c MOTOR_PH_Ls_q_H aliases at the bottom of the
 * macro section feed the MTPA saturation look-up tables in \c mtpa.c, which
 * are static const arrays evaluated at compile time.  When switching the
 * active motor, update both the \c motorParams_M1 pointer in
 * \c motor_params.c AND the two MTPA alias lines below.
 *
 * To add a new motor:
 *  1. Add a macro block below (prefix \c MOTOR_<NAME>_)
 *  2. Add a \c const \c Motor_ParamsType entry in \c motor_params.c using
 *     those macros
 *  3. Update \c motorParams_M1 to point to the new entry
 *  4. Update the \c MOTOR_PH_Ls_d_H / \c MOTOR_PH_Ls_q_H aliases
 *
 * @{
 */

#ifndef MOTOR_PARAMS_H
#define MOTOR_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "math_types.h" /* float32_t */

/* =========================================================================
 * Motor constant macro blocks
 * =========================================================================
 * One block per supported motor.  Naming convention:
 *   MOTOR_<MOTORNAME>_<FIELDNAME>   (SCREAMING_SNAKE_CASE)
 * Unit suffixes follow AGENTS.md: _A, _Hz, _Ohm, _H, _sec, _Hzps, _dps,
 * _Kgm2, _V, _VpHz.
 * ========================================================================= */

/**
 * \anchor MOTOR_TEKNIC_PARAMS
 * \name Teknic ClearPath NEMA-23 motor constants
 * @{
 */
/* Nameplate */
#define MOTOR_TEKNIC_NUM_POLE_PAIRS             (4U)
#define MOTOR_TEKNIC_Rs_Ohm                     (0.393955578f)
#define MOTOR_TEKNIC_Ls_d_H                     (0.000190442806f)
#define MOTOR_TEKNIC_Ls_q_H                     (0.000190442806f)
#define MOTOR_TEKNIC_RATED_FLUX_VpHz            (0.0399353318f)
#define MOTOR_TEKNIC_RATED_CURRENT_A            (6.0f)
#define MOTOR_TEKNIC_INERTIA_Kgm2               (0.00000706154f)
#define MOTOR_TEKNIC_RATED_SPEED_Hz             (300.0f)
#define MOTOR_TEKNIC_RATED_VOLTAGE_V            (24.0f)
/* Closed-loop speed tuning */
#define MOTOR_TEKNIC_SPD_BW_RATIO               (12.0f)
#define MOTOR_TEKNIC_CL_ACCEL_RATE_Hzps         (20.0f)
/* Fault limits */
#define MOTOR_TEKNIC_OVER_CURRENT_LIM_A         (7.0f)
/* Open-loop startup */
#define MOTOR_TEKNIC_OL_SPEED_THR_Hz            (30.0f)
#define MOTOR_TEKNIC_OL_ACCEL_RATE_Hzps         (10.0f)
#define MOTOR_TEKNIC_OL_THETA_RAMP_RATE_dps     (1000.0f)
#define MOTOR_TEKNIC_OL_TORQUE_CURRENT_A        (1.0f)
/* Alignment */
#define MOTOR_TEKNIC_ALIGN_CURRENT_A            (1.0f)
/* Flying start */
#define MOTOR_TEKNIC_FLYING_START_MIN_SPEED_Hz  (10.0f)
/* IPD */
#define MOTOR_TEKNIC_IPD_PULSE_CURRENT_A        (2.0f)
#define MOTOR_TEKNIC_IPD_TIMEOUT_FAULT_sec      (5.0f)
#define MOTOR_TEKNIC_IPD_CYCLE_COUNT_MAX        (2U)
/* Control state timing */
#define MOTOR_TEKNIC_FLYING_START_DELAY_sec     (0.5f)
#define MOTOR_TEKNIC_ALIGN_DELAY_sec            (5.0f)
#define MOTOR_TEKNIC_FORCE_RUN_DELAY_sec        (1.0f)
#define MOTOR_TEKNIC_FWC_DELAY_sec              (2.0f)
/* ESMO observer gains */
#define MOTOR_TEKNIC_ESMO_KSLIDE_MAX            (0.5f)
#define MOTOR_TEKNIC_ESMO_KSLIDE_MIN            (0.1f)
#define MOTOR_TEKNIC_ESMO_PLL_KP_MAX            (10.0f)
#define MOTOR_TEKNIC_ESMO_PLL_KP_MIN            (1.50f)
#define MOTOR_TEKNIC_ESMO_PLL_KP_SF             (5.0f)
#define MOTOR_TEKNIC_ESMO_PLL_KI                (2.8125E-06f)
#define MOTOR_TEKNIC_ESMO_BEMF_THRESHOLD        (0.5f)
#define MOTOR_TEKNIC_ESMO_BEMF_KSLF_FC_SF       (0.5f)
#define MOTOR_TEKNIC_ESMO_THETA_OFFSET_SF       (1.0f)
#define MOTOR_TEKNIC_ESMO_SPEED_LPF_FC_Hz       (200.0f)
/* Flux Observer gains
 * InvKe_init = 1 / ratedFlux_Wb = 1 / (RATED_FLUX_VpHz / (2π))
 * Teknic: 1 / (0.0399353318 / 2π) = 2π / 0.0399353318 ≈ 157.4 (rad/s)/V */
#define MOTOR_TEKNIC_FLUX_OBS_INVKE_INIT        (157.4f)
#define MOTOR_TEKNIC_FLUX_OBS_INVKE_MAX_FACTOR  (4.0f)
#define MOTOR_TEKNIC_FLUX_OBS_INVKE_MIN_FACTOR  (0.25f)
/* bemfFiltConstFactor = 0.1  (1/10th of ωTs) */
#define MOTOR_TEKNIC_FLUX_OBS_BEMF_FILT_FACTOR  (0.1f)
/* speedFiltFactor = 0.1 for first-stage adaptive speed filter (1/10th of ωTs) */
#define MOTOR_TEKNIC_FLUX_OBS_SPEED_FILT_FACTOR (0.1f)
/* Second-stage LPF cutoff: ~1/10th of rated speed (300 Hz) → 30 Hz.
 * Produces filteredRefSpeed stable enough to drive bemfFiltConst. */
#define MOTOR_TEKNIC_FLUX_OBS_SPEED_LPF_FC_Hz   (30.0f)
#define MOTOR_TEKNIC_FLUX_OBS_TRAP_INT_DISABLE  (0U)
#define MOTOR_TEKNIC_FLUX_OBS_INDUCTOR_FILT_EN  (0U)
/** @} */

/**
 * \anchor MOTOR_PANASONIC_BLOWER_PARAMS
 * \name Panasonic blower motor constants
 * @{
 */
/* Nameplate */
#define MOTOR_PANASONIC_BLOWER_NUM_POLE_PAIRS                 (4U)
#define MOTOR_PANASONIC_BLOWER_Rs_Ohm                         (0.800f)
#define MOTOR_PANASONIC_BLOWER_Ls_d_H                         (0.0014423047f)
#define MOTOR_PANASONIC_BLOWER_Ls_q_H                         (0.0014423047f)
#define MOTOR_PANASONIC_BLOWER_RATED_FLUX_VpHz                (0.0384637527f)
#define MOTOR_PANASONIC_BLOWER_RATED_CURRENT_A                (2.2f)
#define MOTOR_PANASONIC_BLOWER_INERTIA_Kgm2                   (0.00000706154f)
#define MOTOR_PANASONIC_BLOWER_RATED_SPEED_Hz                 (100.0f)
#define MOTOR_PANASONIC_BLOWER_RATED_VOLTAGE_V                (24.0f)
/* Closed-loop speed tuning */
#define MOTOR_PANASONIC_BLOWER_SPD_BW_RATIO                   (12.0f)
#define MOTOR_PANASONIC_BLOWER_CL_ACCEL_RATE_Hzps             (20.0f)
/* Fault limits */
#define MOTOR_PANASONIC_BLOWER_OVER_CURRENT_LIM_A             (4.0f)
/* Open-loop startup */
#define MOTOR_PANASONIC_BLOWER_OL_SPEED_THR_Hz                (30.0f)
#define MOTOR_PANASONIC_BLOWER_OL_ACCEL_RATE_Hzps             (10.0f)
#define MOTOR_PANASONIC_BLOWER_OL_THETA_RAMP_RATE_dps         (360.0f)
#define MOTOR_PANASONIC_BLOWER_OL_TORQUE_CURRENT_A            (1.0f)
/* Alignment */
#define MOTOR_PANASONIC_BLOWER_ALIGN_CURRENT_A                (1.0f)
/* Flying start */
#define MOTOR_PANASONIC_BLOWER_FLYING_START_MIN_SPEED_Hz      (3.0f)
/* IPD */
#define MOTOR_PANASONIC_BLOWER_IPD_PULSE_CURRENT_A            (2.0f)
#define MOTOR_PANASONIC_BLOWER_IPD_TIMEOUT_FAULT_sec          (5.0f)
#define MOTOR_PANASONIC_BLOWER_IPD_CYCLE_COUNT_MAX            (2U)
/* Control state timing */
#define MOTOR_PANASONIC_BLOWER_FLYING_START_DELAY_sec         (0.5f)
#define MOTOR_PANASONIC_BLOWER_ALIGN_DELAY_sec                (5.0f)
#define MOTOR_PANASONIC_BLOWER_FORCE_RUN_DELAY_sec            (1.0f)
#define MOTOR_PANASONIC_BLOWER_FWC_DELAY_sec                  (2.0f)
/* ESMO observer gains */
#define MOTOR_PANASONIC_BLOWER_ESMO_KSLIDE_MAX                (0.5f)
#define MOTOR_PANASONIC_BLOWER_ESMO_KSLIDE_MIN                (0.1f)
#define MOTOR_PANASONIC_BLOWER_ESMO_PLL_KP_MAX                (10.0f)
#define MOTOR_PANASONIC_BLOWER_ESMO_PLL_KP_MIN                (1.50f)
#define MOTOR_PANASONIC_BLOWER_ESMO_PLL_KP_SF                 (5.0f)
#define MOTOR_PANASONIC_BLOWER_ESMO_PLL_KI                    (2.8125E-06f)
#define MOTOR_PANASONIC_BLOWER_ESMO_BEMF_THRESHOLD            (0.5f)
#define MOTOR_PANASONIC_BLOWER_ESMO_BEMF_KSLF_FC_SF           (0.5f)
#define MOTOR_PANASONIC_BLOWER_ESMO_THETA_OFFSET_SF           (1.0f)
#define MOTOR_PANASONIC_BLOWER_ESMO_SPEED_LPF_FC_Hz           (50.0f)
/* Flux Observer gains */
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_INVKE_INIT            (0.0f)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_INVKE_MAX_FACTOR      (4.0f)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_INVKE_MIN_FACTOR      (0.25f)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_BEMF_FILT_FACTOR      (0.1f)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_SPEED_FILT_FACTOR     (0.1f)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_SPEED_LPF_FC_Hz       (10.0f)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_TRAP_INT_DISABLE      (0U)
#define MOTOR_PANASONIC_BLOWER_FLUX_OBS_INDUCTOR_FILT_EN      (0U)
/** @} */
/**
 * \anchor MOTOR_ANAHEIM_BLY171S_PARAMS
 * \name Anaheim Automation BLY171S-24V-4000 constants (DRV8376EVM bench)
 * @{
 *
 * Sources: the motor label read 2026-10-15 (24 V, 4,000 rpm, 1.8 A, 1.5 ohm line to line, 2.45 V per 1,000 rpm,
 * eight poles) and Anaheim Automation product sheet L010228 / the BLY171S-24V-4000 web page (read 2026-10-16:
 * 2.1 mH line to line, torque constant 4.96 oz-in/A, rotor inertia 0.00034 oz-in-sec^2).
 *
 * TI's Rs and Ls are wye-equivalent phase values, half of the line-to-line figures whether the winding is wye or
 * delta. Surface magnets, so Ls_d = Ls_q.
 *
 * Flux convention: the label's 2.45 V per 1,000 rpm is taken as the phase-to-neutral PEAK back-EMF.
 *   2.45 V / (1,000 rpm x 4 pole pairs / 60) = 2.45 / 66.667 Hz = 0.03675 V/Hz.
 * Cross-check: torque constant 4.96 oz-in/A = 0.03503 N.m/A and back-EMF 2.45 V/krpm = 0.02340 V.s/rad give a
 * ratio of 1.497, and a sine-driven PMSM has Kt = 1.5 x Ke(phase peak). Reading the figure as line-to-line rms,
 * line-to-line peak or phase rms would give ratios 1.22, 0.87 and 2.12, so phase peak fits. If the flux turns out
 * wrong on the bench, the alternatives are 0.0300 (line-to-line rms) and 0.0212 (line-to-line peak).
 *
 * Bench currents: align, open-loop torque, IPD and Hall calibration currents are all capped at 0.4 A so the
 * 12 V, 0.5 A adapter never folds back during setup (SDK defaults are 1 to 2 A).
 */
#define MOTOR_BLY171S_NUM_POLE_PAIRS                 (4U)
#define MOTOR_BLY171S_Rs_Ohm                         (0.75f)
#define MOTOR_BLY171S_Ls_d_H                         (0.00105f)
#define MOTOR_BLY171S_Ls_q_H                         (0.00105f)
#define MOTOR_BLY171S_RATED_FLUX_VpHz                (0.03675f)
#define MOTOR_BLY171S_RATED_CURRENT_A                (1.8f)
#define MOTOR_BLY171S_INERTIA_Kgm2                   (0.0000024009f)  /* 0.00034 oz-in-sec^2 x 0.00706155 */
#define MOTOR_BLY171S_RATED_SPEED_Hz                 (266.7f)         /* 4,000 rpm x 4 / 60 */
#define MOTOR_BLY171S_RATED_VOLTAGE_V                (12.0f)          /* bench supply, not the 24 V label; feeds maxVsMag_V */
/* Speed tuning: Panasonic blower values (conservative, same pole pairs) until tuned on the bench */
#define MOTOR_BLY171S_SPD_BW_RATIO                   (12.0f)
#define MOTOR_BLY171S_CL_ACCEL_RATE_Hzps             (20.0f)
/* At nominal VREF=3.0 V and gain=1 V/A, the CSA linear range is +/-1.25 A.
 * Keep the trip inside that range and above the 0.4 A commissioning command limit.
 * Actual gain, current polarity and protection operation still require bench validation. */
#define MOTOR_BLY171S_OVER_CURRENT_LIM_A             (1.0f)
#define MOTOR_BLY171S_OL_SPEED_THR_Hz                (30.0f)
#define MOTOR_BLY171S_OL_ACCEL_RATE_Hzps             (10.0f)
#define MOTOR_BLY171S_OL_THETA_RAMP_RATE_dps         (360.0f)
#define MOTOR_BLY171S_OL_TORQUE_CURRENT_A            (0.4f)
#define MOTOR_BLY171S_ALIGN_CURRENT_A                (0.4f)
#define MOTOR_BLY171S_FLYING_START_MIN_SPEED_Hz      (3.0f)
#define MOTOR_BLY171S_IPD_PULSE_CURRENT_A            (0.4f)
#define MOTOR_BLY171S_IPD_TIMEOUT_FAULT_sec          (5.0f)
#define MOTOR_BLY171S_IPD_CYCLE_COUNT_MAX            (2U)
#define MOTOR_BLY171S_FLYING_START_DELAY_sec         (0.5f)
#define MOTOR_BLY171S_ALIGN_DELAY_sec                (5.0f)
#define MOTOR_BLY171S_FORCE_RUN_DELAY_sec            (1.0f)
#define MOTOR_BLY171S_FWC_DELAY_sec                  (2.0f)
/* ESMO and flux observer: Panasonic blower values; unused in the Hall-sensored project */
#define MOTOR_BLY171S_ESMO_KSLIDE_MAX                (0.5f)
#define MOTOR_BLY171S_ESMO_KSLIDE_MIN                (0.1f)
#define MOTOR_BLY171S_ESMO_PLL_KP_MAX                (10.0f)
#define MOTOR_BLY171S_ESMO_PLL_KP_MIN                (1.50f)
#define MOTOR_BLY171S_ESMO_PLL_KP_SF                 (5.0f)
#define MOTOR_BLY171S_ESMO_PLL_KI                    (2.8125E-06f)
#define MOTOR_BLY171S_ESMO_BEMF_THRESHOLD            (0.5f)
#define MOTOR_BLY171S_ESMO_BEMF_KSLF_FC_SF           (0.5f)
#define MOTOR_BLY171S_ESMO_THETA_OFFSET_SF           (1.0f)
#define MOTOR_BLY171S_ESMO_SPEED_LPF_FC_Hz           (50.0f)
#define MOTOR_BLY171S_FLUX_OBS_INVKE_INIT            (0.0f)
#define MOTOR_BLY171S_FLUX_OBS_INVKE_MAX_FACTOR      (4.0f)
#define MOTOR_BLY171S_FLUX_OBS_INVKE_MIN_FACTOR      (0.25f)
#define MOTOR_BLY171S_FLUX_OBS_BEMF_FILT_FACTOR      (0.1f)
#define MOTOR_BLY171S_FLUX_OBS_SPEED_FILT_FACTOR     (0.1f)
#define MOTOR_BLY171S_FLUX_OBS_SPEED_LPF_FC_Hz       (10.0f)
#define MOTOR_BLY171S_FLUX_OBS_TRAP_INT_DISABLE      (0U)
#define MOTOR_BLY171S_FLUX_OBS_INDUCTOR_FILT_EN      (0U)
/* Hall calibration (hall_cal.c): the SDK's 2 A sweep would pull about 0.6 A from the bus */
#define MOTOR_BLY171S_HALL_CAL_ALIGN_CURRENT_A       (0.4f)
#define MOTOR_BLY171S_HALL_CAL_SWEEP_CURRENT_A       (0.4f)
#define MOTOR_BLY171S_HALL_CAL_MAX_CURRENT_A         (0.4f)
#define MOTOR_BLY171S_HALL_CAL_MIN_CURRENT_A         (0.1f)
/** @} */
/**
 * \anchor MOTOR_PRO_RANGE_DRONE_2300_PARAMS
 * \name PRO_RANGE_DRONE constants
 * @{
 */
/* Nameplate */
#define MOTOR_PRO_RANGE_DRONE_2300_NUM_POLE_PAIRS             (4)
#define MOTOR_PRO_RANGE_DRONE_2300_Rs_Ohm                     (0.1)
#define MOTOR_PRO_RANGE_DRONE_2300_Ls_d_H                     (0.0000095)
#define MOTOR_PRO_RANGE_DRONE_2300_Ls_q_H                     (0.0000115)
#define MOTOR_PRO_RANGE_DRONE_2300_RATED_FLUX_VpHz            (0.00224)
#define MOTOR_PRO_RANGE_DRONE_2300_RATED_CURRENT_A            (4.0)
#define MOTOR_PRO_RANGE_DRONE_2300_INERTIA_Kgm2               (0.000000294f)
#define MOTOR_PRO_RANGE_DRONE_2300_RATED_SPEED_Hz             (4000)
#define MOTOR_PRO_RANGE_DRONE_2300_RATED_VOLTAGE_V            (16)
/* Speed Tuning */
#define MOTOR_PRO_RANGE_DRONE_2300_SPD_BW_RATIO               (12.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_CL_ACCEL_RATE_Hzps         (3500)
/* Fault Limits */
#define MOTOR_PRO_RANGE_DRONE_2300_OVER_CURRENT_LIM_A         (12)
/* Open-Loop Startup */
#define MOTOR_PRO_RANGE_DRONE_2300_OL_SPEED_THR_Hz            (400)
#define MOTOR_PRO_RANGE_DRONE_2300_OL_ACCEL_RATE_Hzps         (1500)
#define MOTOR_PRO_RANGE_DRONE_2300_OL_THETA_RAMP_RATE_dps     (360.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_OL_TORQUE_CURRENT_A        (4.0)
/* Alignment */
#define MOTOR_PRO_RANGE_DRONE_2300_ALIGN_CURRENT_A            (2.0)
/* Flying Start */
#define MOTOR_PRO_RANGE_DRONE_2300_FLYING_START_MIN_SPEED_Hz  (600)
/* IPD */
#define MOTOR_PRO_RANGE_DRONE_2300_IPD_PULSE_CURRENT_A        (3.0)
#define MOTOR_PRO_RANGE_DRONE_2300_IPD_TIMEOUT_FAULT_sec      (5.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_IPD_CYCLE_COUNT_MAX        (2U)
/* State Timing */
#define MOTOR_PRO_RANGE_DRONE_2300_FLYING_START_DELAY_sec     (0.5f)
#define MOTOR_PRO_RANGE_DRONE_2300_ALIGN_DELAY_sec            (5.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_FORCE_RUN_DELAY_sec        (1.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_FWC_DELAY_sec              (2.0f)
/* ESMO Observer Gains */
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_KSLIDE_MAX            (0.5f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_KSLIDE_MIN            (0.1f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KP_MAX            (10.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KP_MIN            (1.50f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KP_SF             (5.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KI                (2.8125E-06f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_BEMF_THRESHOLD        (0.5f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_BEMF_KSLF_FC_SF       (0.5f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_THETA_OFFSET_SF       (1.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_ESMO_SPEED_LPF_FC_Hz       (200.0)
/* Flux Observer Gains */
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INVKE_INIT        (2800.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INVKE_MAX_FACTOR  (4.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INVKE_MIN_FACTOR  (0.25f)
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_BEMF_FILT_FACTOR  (0.1f)
/* speedFiltFactor = 0.2 for drone — faster first-stage tracking at high speed */
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_SPEED_FILT_FACTOR (0.2f)
/* Second-stage LPF cutoff: ~1/10th of rated speed (3450 Hz) → 345 Hz.
 * Use 200 Hz as a conservative starting point for the drone motor. */
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_SPEED_LPF_FC_Hz   (200.0f)
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_TRAP_INT_DISABLE  (0U)
#define MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INDUCTOR_FILT_EN  (0U)
/** @} */
/** @} */

/** @} */
/**
 * \anchor MOTOR_KV_880_DRONE_PARAMS
 * \name KV_880_DRONE constants
 * @{
 */
/* Nameplate */
#define MOTOR_KV_880_DRONE_NUM_POLE_PAIRS             (4U)
#define MOTOR_KV_880_DRONE_Rs_Ohm                     (0.066)
#define MOTOR_KV_880_DRONE_Ls_d_H                     (0.000014)
#define MOTOR_KV_880_DRONE_Ls_q_H                     (0.000014)
#define MOTOR_KV_880_DRONE_RATED_FLUX_VpHz            (0.007)
#define MOTOR_KV_880_DRONE_RATED_CURRENT_A            (12U)
#define MOTOR_KV_880_DRONE_INERTIA_Kgm2               (0.00000706154f)
#define MOTOR_KV_880_DRONE_RATED_SPEED_Hz             (3450U)
#define MOTOR_KV_880_DRONE_RATED_VOLTAGE_V            (15U)
/* Speed Tuning */
#define MOTOR_KV_880_DRONE_SPD_BW_RATIO               (12.0f)
#define MOTOR_KV_880_DRONE_CL_ACCEL_RATE_Hzps         (50U)
/* Fault Limits */
#define MOTOR_KV_880_DRONE_OVER_CURRENT_LIM_A         (12U)
/* Open-Loop Startup */
#define MOTOR_KV_880_DRONE_OL_SPEED_THR_Hz            (250U)
#define MOTOR_KV_880_DRONE_OL_ACCEL_RATE_Hzps         (50U)
#define MOTOR_KV_880_DRONE_OL_THETA_RAMP_RATE_dps     (360.0f)
#define MOTOR_KV_880_DRONE_OL_TORQUE_CURRENT_A        (4.0)
/* Alignment */
#define MOTOR_KV_880_DRONE_ALIGN_CURRENT_A            (2.0)
/* Flying Start */
#define MOTOR_KV_880_DRONE_FLYING_START_MIN_SPEED_Hz  (600)
/* IPD */
#define MOTOR_KV_880_DRONE_IPD_PULSE_CURRENT_A        (3.0)
#define MOTOR_KV_880_DRONE_IPD_TIMEOUT_FAULT_sec      (5.0f)
#define MOTOR_KV_880_DRONE_IPD_CYCLE_COUNT_MAX        (2U)
/* State Timing */
#define MOTOR_KV_880_DRONE_FLYING_START_DELAY_sec     (0.5f)
#define MOTOR_KV_880_DRONE_ALIGN_DELAY_sec            (5.0f)
#define MOTOR_KV_880_DRONE_FORCE_RUN_DELAY_sec        (1.0f)
#define MOTOR_KV_880_DRONE_FWC_DELAY_sec              (2.0f)
/* ESMO Observer Gains */
#define MOTOR_KV_880_DRONE_ESMO_KSLIDE_MAX            (0.5f)
#define MOTOR_KV_880_DRONE_ESMO_KSLIDE_MIN            (0.1f)
#define MOTOR_KV_880_DRONE_ESMO_PLL_KP_MAX            (10.0f)
#define MOTOR_KV_880_DRONE_ESMO_PLL_KP_MIN            (1.50f)
#define MOTOR_KV_880_DRONE_ESMO_PLL_KP_SF             (5.0f)
#define MOTOR_KV_880_DRONE_ESMO_PLL_KI                (2.8125E-06f)
#define MOTOR_KV_880_DRONE_ESMO_BEMF_THRESHOLD        (0.5f)
#define MOTOR_KV_880_DRONE_ESMO_BEMF_KSLF_FC_SF       (0.5f)
#define MOTOR_KV_880_DRONE_ESMO_THETA_OFFSET_SF       (1.0f)
#define MOTOR_KV_880_DRONE_ESMO_SPEED_LPF_FC_Hz       (200.0)
/* Flux Observer Gains */
#define MOTOR_KV_880_DRONE_FLUX_OBS_INVKE_INIT        (0.0f)
#define MOTOR_KV_880_DRONE_FLUX_OBS_INVKE_MAX_FACTOR  (4.0f)
#define MOTOR_KV_880_DRONE_FLUX_OBS_INVKE_MIN_FACTOR  (0.25f)
#define MOTOR_KV_880_DRONE_FLUX_OBS_BEMF_FILT_FACTOR  (0.1f)
/* speedFiltFactor = 0.2 for drone — faster first-stage tracking at high speed */
#define MOTOR_KV_880_DRONE_FLUX_OBS_SPEED_FILT_FACTOR (0.2f)
/* Second-stage LPF cutoff: ~1/10th of rated speed (3450 Hz) → 345 Hz.
 * Use 200 Hz as a conservative starting point for the drone motor. */
#define MOTOR_KV_880_DRONE_FLUX_OBS_SPEED_LPF_FC_Hz   (200.0f)
#define MOTOR_KV_880_DRONE_FLUX_OBS_TRAP_INT_DISABLE  (0U)
#define MOTOR_KV_880_DRONE_FLUX_OBS_INDUCTOR_FILT_EN  (0U)
/** @} */
/** @} */
/** @} */
/**
 * \anchor MOTOR_DRONE_HP_PARAMS
 * \name MOTOR_DRONE_HP_DRONE constants
 * @{
 */
/* Nameplate */
#define MOTOR_DRONE_HP_POLE_PAIRS                     (6U)
#define MOTOR_DRONE_HP_Rs_Ohm                         (0.023)
#define MOTOR_DRONE_HP_Ls_d_H                         (0.00000946)
#define MOTOR_DRONE_HP_Ls_q_H                         (0.00000946)
#define MOTOR_DRONE_HP_RATED_FLUX_VpHz                (0.0184)
#define MOTOR_DRONE_HP_RATED_CURRENT_A                (8.0)
#define MOTOR_DRONE_HP_INERTIA_Kgm2                   (0.00000706154f)
#define MOTOR_DRONE_HP_RATED_SPEED_Hz                 (1350)
#define MOTOR_DRONE_HP_RATED_VOLTAGE_V                (15)
/* Speed Tuning */
#define MOTOR_DRONE_HP_SPD_BW_RATIO                   (50.0f)
#define MOTOR_DRONE_HP_CL_ACCEL_RATE_Hzps             (500)
/* Fault Limits */
#define MOTOR_DRONE_HP_OVER_CURRENT_LIM_A             (15.0)
/* Open-Loop Startup */
#define MOTOR_DRONE_HP_OL_SPEED_THR_Hz                (250)
#define MOTOR_DRONE_HP_OL_ACCEL_RATE_Hzps             (500)
#define MOTOR_DRONE_HP_OL_THETA_RAMP_RATE_dps         (360.0f)
#define MOTOR_DRONE_HP_OL_TORQUE_CURRENT_A            (4.0)
/* Alignment */
#define MOTOR_DRONE_HP_ALIGN_CURRENT_A                (2.0)
/* Flying Start */
#define MOTOR_DRONE_HP_FLYING_START_MIN_SPEED_Hz      (600)
/* IPD */
#define MOTOR_DRONE_HP_IPD_PULSE_CURRENT_A            (3.0)
#define MOTOR_DRONE_HP_IPD_TIMEOUT_FAULT_sec          (5.0f)
#define MOTOR_DRONE_HP_IPD_CYCLE_COUNT_MAX            (2U)
/* State Timing */
#define MOTOR_DRONE_HP_FLYING_START_DELAY_sec         (0.5f)
#define MOTOR_DRONE_HP_ALIGN_DELAY_sec                (5.0f)
#define MOTOR_DRONE_HP_FORCE_RUN_DELAY_sec            (1.0f)
#define MOTOR_DRONE_HP_FWC_DELAY_sec                  (2.0f)
/* ESMO Observer Gains */
#define MOTOR_DRONE_HP_ESMO_KSLIDE_MAX                (0.5f)
#define MOTOR_DRONE_HP_ESMO_KSLIDE_MIN                (0.1f)
#define MOTOR_DRONE_HP_ESMO_PLL_KP_MAX                (10.0f)
#define MOTOR_DRONE_HP_ESMO_PLL_KP_MIN                (1.50f)
#define MOTOR_DRONE_HP_ESMO_PLL_KP_SF                 (5.0f)
#define MOTOR_DRONE_HP_ESMO_PLL_KI                    (2.8125E-06f)
#define MOTOR_DRONE_HP_ESMO_BEMF_THRESHOLD            (0.5f)
#define MOTOR_DRONE_HP_ESMO_BEMF_KSLF_FC_SF           (0.5f)
#define MOTOR_DRONE_HP_ESMO_THETA_OFFSET_SF           (1.0f)
#define MOTOR_DRONE_HP_ESMO_SPEED_LPF_FC_Hz           (200.0)

/* Flux Observer gains
 * InvKe_init = 1 / ratedFlux_Wb = 1 / (RATED_FLUX_VpHz / (2π))*/
#define MOTOR_DRONE_HP_FLUX_OBS_INVKE_INIT        (350.4f)
#define MOTOR_DRONE_HP_FLUX_OBS_INVKE_MAX_FACTOR  (4.0f)
#define MOTOR_DRONE_HP_FLUX_OBS_INVKE_MIN_FACTOR  (0.25f)
/* (1/10th of ωTs) */
#define MOTOR_DRONE_HP_FLUX_OBS_BEMF_FILT_FACTOR  (0.1f)
/* speedFiltFactor = 0.1 for first-stage adaptive speed filter (1/10th of ωTs) */
#define MOTOR_DRONE_HP_FLUX_OBS_SPEED_FILT_FACTOR (0.1f)
/* Second-stage LPF cutoff: ~1/10th of rated speed (2000 Hz) → 200 Hz.
 * Produces filteredRefSpeed stable enough to drive bemfFiltConst. */
#define MOTOR_DRONE_HP_FLUX_OBS_SPEED_LPF_FC_Hz   (200.0f)
#define MOTOR_DRONE_HP_FLUX_OBS_TRAP_INT_DISABLE  (0U)
#define MOTOR_DRONE_HP_FLUX_OBS_INDUCTOR_FILT_EN  (0U)
/** @} */
/**
 * \anchor MOTOR_MTPA_ALIASES
 * \name MTPA compile-time inductance aliases
 * @{
 *
 * The MTPA saturation look-up tables in mtpa.c are static const arrays
 * evaluated at compile time.  They consume MOTOR_PH_Ls_d_H / MOTOR_PH_Ls_q_H
 * via the chain:
 *   mtpa.c -> mtr_config.h (USER_MOTOR_Ls_d_H = MOTOR_PH_Ls_d_H) -> here
 *
 * These two aliases MUST point to the same motor as motorParams_M1 in
 * motor_params.c.  When switching the active motor, update both the pointer
 * in motor_params.c AND these two lines.
 *
 * Do NOT modify mtr_config.h — the USER_MOTOR_Ls_d_H bridge there is fixed.
 */
#define MOTOR_PH_Ls_d_H                           MOTOR_BLY171S_Ls_d_H  //!< Active motor d-axis inductance for MTPA LUT (BLY171S)
#define MOTOR_PH_Ls_q_H                           MOTOR_BLY171S_Ls_q_H  //!< Active motor q-axis inductance for MTPA LUT (BLY171S)
/** @} */

/* =========================================================================
 * Motor Parameters Structure
 * ========================================================================= */

/**
 * \anchor MOTOR_PARAMS_STRUCT
 * \name Motor Parameters Structure
 * @{
 */

/**
 * \brief Per-motor constant table.
 *
 * All fields that differ between motors are collected here.
 * Board-level and algorithm constants remain in \c user_mtr1.h.
 */
typedef struct _Motor_ParamsType_
{
    /*=======================================================================
     * NAMEPLATE — electrical and mechanical motor data
     *=======================================================================*/

    uint32_t  NumPolePairs;   /*!< Number of pole pairs */
    float32_t Rs_Ohm;         /*!< Phase resistance (Ohm) */
    float32_t Ls_d_H;         /*!< D-axis inductance (H) */
    float32_t Ls_q_H;         /*!< Q-axis inductance (H) */
    float32_t RatedFlux_VpHz; /*!< Rated flux linkage (V/Hz) */
    float32_t RatedCurrent_A; /*!< Rated phase current (A) */
    float32_t Inertia_Kgm2;   /*!< Rotor inertia (kg·m²) */
    float32_t RatedSpeed_Hz;  /*!< Rated electrical speed (Hz) */
    float32_t RatedVoltage_V; /*!< Rated phase voltage (V) */

    /*=======================================================================
     * CLOSED-LOOP SPEED TUNING
     *=======================================================================*/

    float32_t SpdBwRatio;       /*!< Speed loop bandwidth ratio (dimensionless) */
    float32_t ClAccelRate_Hzps; /*!< Closed-loop acceleration rate (Hz/s) */

    /*=======================================================================
     * FAULT LIMITS
     *=======================================================================*/

    float32_t OverCurrentLim_A; /*!< Hardware over-current fault limit (A) */

    /*=======================================================================
     * OPEN-LOOP STARTUP
     *=======================================================================*/

    float32_t OlSpeedThr_Hz;       /*!< OL->CL transition speed threshold (Hz) */
    float32_t OlAccelRate_Hzps;    /*!< Open-loop acceleration rate (Hz/s) */
    float32_t OlThetaRampRate_dps; /*!< Theta ramp rate during OL->CL blend (deg/s) */
    float32_t OlTorqueCurrent_A;   /*!< Open-loop torque current limit (A) */

    /*=======================================================================
     * ALIGNMENT
     *=======================================================================*/

    float32_t AlignCurrent_A; /*!< Rotor alignment current limit (A) */

    /*=======================================================================
     * FLYING START
     *=======================================================================*/

    float32_t FlyingStartMinSpeed_Hz; /*!< Minimum speed for flying-start CL transition (Hz) */

    /*=======================================================================
     * INITIAL POSITION DETECTION (IPD)
     *=======================================================================*/

    float32_t IpdPulseCurrent_A;   /*!< IPD pulse current limit (A) */
    float32_t IpdTimeoutFault_sec; /*!< IPD timeout fault duration (s) */
    uint32_t  IpdCycleCountMax;    /*!< IPD injection cycle count */

    /*=======================================================================
     * CONTROL STATE TIMING
     *=======================================================================*/

    float32_t FlyingStartDelay_sec; /*!< Flying-start rotor-speed settle time (s) */
    float32_t AlignDelay_sec;       /*!< Rotor alignment hold time (s) */
    float32_t ForceRunDelay_sec;    /*!< Forced-angle run time before CL transition (s) */
    float32_t FwcDelay_sec;         /*!< Field weakening activation delay after startup (s) */

    /*=======================================================================
     * ESMO OBSERVER GAINS
     *=======================================================================*/

    float32_t EsmoKslideMax;     /*!< SMC maximum gain */
    float32_t EsmoKslideMin;     /*!< SMC minimum gain */
    float32_t EsmoPllKpMax;      /*!< PLL maximum Kp */
    float32_t EsmoPllKpMin;      /*!< PLL minimum Kp */
    float32_t EsmoPllKpSf;       /*!< PLL Kp adjusting coefficient */
    float32_t EsmoPllKi;         /*!< PLL Ki (wn^2 * Ts) */
    float32_t EsmoBemfThreshold; /*!< BEMF threshold (fraction of rated voltage) */
    float32_t EsmoBemfKslfFcSf;  /*!< BEMF LPF coefficient (fc * 2pi * Ts) */
    float32_t EsmoThetaOffsetSf; /*!< Theta offset compensation coefficient */
    float32_t EsmoSpeedLpfFc_Hz; /*!< Speed LPF cutoff frequency (Hz) */

    /*=======================================================================
     * FLUX OBSERVER PARAMS
     *=======================================================================*/

    float32_t FluxObsInvKeInit;       /*!< Initial 1/Ke value (rad/s/V); 0 = auto-compute */
    float32_t FluxObsInvKeMaxFactor;  /*!< InvKeMax = InvKeInit * factor */
    float32_t FluxObsInvKeMinFactor;  /*!< InvKeMin = InvKeInit * factor */
    float32_t FluxObsBemfFiltFactor;  /*!< BEMF filter constant scaling factor (default 0.1) */
    float32_t FluxObsSpeedFiltFactor; /*!< First-stage speed filter scaling factor (default 0.1) */
    float32_t FluxObsSpeedLpfFc_Hz;   /*!< Second-stage speed LPF cutoff frequency (Hz);
                                           recommended ≈ ratedSpeed_Hz / 10 */
    uint32_t  FluxObsTrapIntDisable;  /*!< 0=trapezoidal, 1=rectangular integration */
    uint32_t  FluxObsInductorFiltEn;  /*!< Enable DQ-frame inductive drop filter */

    /*=======================================================================
     * HALL SENSOR CALIBRATION PARAMETERS
     *=======================================================================*/

    float32_t HallCalAlignCurrent_A; /*!< Alignment current for Hall calibration (A) */
    float32_t HallCalSweepCurrent_A; /*!< Sweep current for Hall calibration (A) */
    float32_t HallCalStepSize_rad;   /*!< Step size for Hall calibration (rad) */
    float32_t HallCalWaitTime_sec;   /*!< Wait time between steps (sec) */
    float32_t HallCalMaxCurrent_A;   /*!< Maximum current limit during calibration (A) */
    float32_t HallCalMinCurrent_A;   /*!< Minimum current limit during calibration (A) */
    uint32_t  HallCalMaxSteps;       /*!< Maximum number of calibration steps */

} Motor_ParamsType;

/** @} */

/**
 * \anchor MOTOR_PARAMS_EXTERN
 * \name Motor Table Declarations
 * @{
 */

/** \brief Teknic ClearPath NEMA-23 motor constant table */
extern const Motor_ParamsType Motor_Params_Teknic;

/** \brief Panasonic blower motor constant table */
extern const Motor_ParamsType Motor_Params_Panasonic_Blower;

/** \brief Anaheim Automation BLY171S-24V-4000 constant table (DRV8376EVM bench) */
extern const Motor_ParamsType Motor_anaheimBly171sParams;

/**
 * \brief Active motor 1 parameter table pointer.
 *
 * Change this pointer in \c motor_params.c to select a different motor.
 * The pointer itself is \c const (cannot be reassigned at runtime by accident),
 * but the target can be changed at link time or — for runtime switching —
 * by making the pointer non-const and updating it before calling
 * \c initMotor1CtrlParameters().
 */
extern const Motor_ParamsType *const motorParams_M1;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_PARAMS_H */

/** @} */
