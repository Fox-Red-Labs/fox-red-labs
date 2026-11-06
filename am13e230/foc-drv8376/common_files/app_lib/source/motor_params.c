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

/*
 * motor_params.c — Motor constant tables
 *
 * All numeric values are sourced from the per-motor macro blocks defined in
 * motor_params.h.  To add a new motor:
 *   1. Add a macro block in motor_params.h (prefix MOTOR_<NAME>_)
 *   2. Add a new const Motor_ParamsType entry below using those macros
 *   3. Update motorParams_M1 to point to it
 *   4. Update the MOTOR_PH_Ls_d_H / MOTOR_PH_Ls_q_H aliases in motor_params.h
 */

#include "motor_params.h"

/*===========================================================================
 * Teknic ClearPath NEMA-23 motor
 *===========================================================================*/
const Motor_ParamsType Motor_teknicParams = {
    /* Nameplate */
    .NumPolePairs   = MOTOR_TEKNIC_NUM_POLE_PAIRS,
    .Rs_Ohm         = MOTOR_TEKNIC_Rs_Ohm,
    .Ls_d_H         = MOTOR_TEKNIC_Ls_d_H,
    .Ls_q_H         = MOTOR_TEKNIC_Ls_q_H,
    .RatedFlux_VpHz = MOTOR_TEKNIC_RATED_FLUX_VpHz,
    .RatedCurrent_A = MOTOR_TEKNIC_RATED_CURRENT_A,
    .Inertia_Kgm2   = MOTOR_TEKNIC_INERTIA_Kgm2,
    .RatedSpeed_Hz  = MOTOR_TEKNIC_RATED_SPEED_Hz,
    .RatedVoltage_V = MOTOR_TEKNIC_RATED_VOLTAGE_V,

    /* Closed-loop speed tuning */
    .SpdBwRatio       = MOTOR_TEKNIC_SPD_BW_RATIO,
    .ClAccelRate_Hzps = MOTOR_TEKNIC_CL_ACCEL_RATE_Hzps,

    /* Fault limits */
    .OverCurrentLim_A = MOTOR_TEKNIC_OVER_CURRENT_LIM_A,

    /* Open-loop startup */
    .OlSpeedThr_Hz       = MOTOR_TEKNIC_OL_SPEED_THR_Hz,
    .OlAccelRate_Hzps    = MOTOR_TEKNIC_OL_ACCEL_RATE_Hzps,
    .OlThetaRampRate_dps = MOTOR_TEKNIC_OL_THETA_RAMP_RATE_dps,
    .OlTorqueCurrent_A   = MOTOR_TEKNIC_OL_TORQUE_CURRENT_A,

    /* Alignment */
    .AlignCurrent_A = MOTOR_TEKNIC_ALIGN_CURRENT_A,

    /* Flying start */
    .FlyingStartMinSpeed_Hz = MOTOR_TEKNIC_FLYING_START_MIN_SPEED_Hz,

    /* IPD */
    .IpdPulseCurrent_A   = MOTOR_TEKNIC_IPD_PULSE_CURRENT_A,
    .IpdTimeoutFault_sec = MOTOR_TEKNIC_IPD_TIMEOUT_FAULT_sec,
    .IpdCycleCountMax    = MOTOR_TEKNIC_IPD_CYCLE_COUNT_MAX,

    /* Control state timing */
    .FlyingStartDelay_sec = MOTOR_TEKNIC_FLYING_START_DELAY_sec,
    .AlignDelay_sec       = MOTOR_TEKNIC_ALIGN_DELAY_sec,
    .ForceRunDelay_sec    = MOTOR_TEKNIC_FORCE_RUN_DELAY_sec,
    .FwcDelay_sec         = MOTOR_TEKNIC_FWC_DELAY_sec,

    /* ESMO observer gains */
    .EsmoKslideMax     = MOTOR_TEKNIC_ESMO_KSLIDE_MAX,
    .EsmoKslideMin     = MOTOR_TEKNIC_ESMO_KSLIDE_MIN,
    .EsmoPllKpMax      = MOTOR_TEKNIC_ESMO_PLL_KP_MAX,
    .EsmoPllKpMin      = MOTOR_TEKNIC_ESMO_PLL_KP_MIN,
    .EsmoPllKpSf       = MOTOR_TEKNIC_ESMO_PLL_KP_SF,
    .EsmoPllKi         = MOTOR_TEKNIC_ESMO_PLL_KI,
    .EsmoBemfThreshold = MOTOR_TEKNIC_ESMO_BEMF_THRESHOLD,
    .EsmoBemfKslfFcSf  = MOTOR_TEKNIC_ESMO_BEMF_KSLF_FC_SF,
    .EsmoThetaOffsetSf = MOTOR_TEKNIC_ESMO_THETA_OFFSET_SF,
    .EsmoSpeedLpfFc_Hz = MOTOR_TEKNIC_ESMO_SPEED_LPF_FC_Hz,

    /* Flux Observer gains */
    .FluxObsInvKeInit       = MOTOR_TEKNIC_FLUX_OBS_INVKE_INIT,
    .FluxObsInvKeMaxFactor  = MOTOR_TEKNIC_FLUX_OBS_INVKE_MAX_FACTOR,
    .FluxObsInvKeMinFactor  = MOTOR_TEKNIC_FLUX_OBS_INVKE_MIN_FACTOR,
    .FluxObsBemfFiltFactor  = MOTOR_TEKNIC_FLUX_OBS_BEMF_FILT_FACTOR,
    .FluxObsSpeedFiltFactor = MOTOR_TEKNIC_FLUX_OBS_SPEED_FILT_FACTOR,
    .FluxObsSpeedLpfFc_Hz   = MOTOR_TEKNIC_FLUX_OBS_SPEED_LPF_FC_Hz,
    .FluxObsTrapIntDisable  = MOTOR_TEKNIC_FLUX_OBS_TRAP_INT_DISABLE,
    .FluxObsInductorFiltEn  = MOTOR_TEKNIC_FLUX_OBS_INDUCTOR_FILT_EN,

    /* Hall sensor calibration parameters */
    .HallCalAlignCurrent_A = 2.0f,
    .HallCalSweepCurrent_A = 2.0f,
    .HallCalStepSize_rad   = (float32_t)(MATH_PI / 90.0f),
    .HallCalWaitTime_sec   = 0.1f,
    .HallCalMaxCurrent_A   = 5.0f,
    .HallCalMinCurrent_A   = 0.5f,
    .HallCalMaxSteps       = 180U,
};

/*===========================================================================
 * Panasonic blower motor
 *===========================================================================*/
const Motor_ParamsType Motor_panasonicBlowerParams = {
    /* Nameplate */
    .NumPolePairs   = MOTOR_PANASONIC_BLOWER_NUM_POLE_PAIRS,
    .Rs_Ohm         = MOTOR_PANASONIC_BLOWER_Rs_Ohm,
    .Ls_d_H         = MOTOR_PANASONIC_BLOWER_Ls_d_H,
    .Ls_q_H         = MOTOR_PANASONIC_BLOWER_Ls_q_H,
    .RatedFlux_VpHz = MOTOR_PANASONIC_BLOWER_RATED_FLUX_VpHz,
    .RatedCurrent_A = MOTOR_PANASONIC_BLOWER_RATED_CURRENT_A,
    .Inertia_Kgm2   = MOTOR_PANASONIC_BLOWER_INERTIA_Kgm2,
    .RatedSpeed_Hz  = MOTOR_PANASONIC_BLOWER_RATED_SPEED_Hz,
    .RatedVoltage_V = MOTOR_PANASONIC_BLOWER_RATED_VOLTAGE_V,

    /* Closed-loop speed tuning */
    .SpdBwRatio       = MOTOR_PANASONIC_BLOWER_SPD_BW_RATIO,
    .ClAccelRate_Hzps = MOTOR_PANASONIC_BLOWER_CL_ACCEL_RATE_Hzps,

    /* Fault limits */
    .OverCurrentLim_A = MOTOR_PANASONIC_BLOWER_OVER_CURRENT_LIM_A,

    /* Open-loop startup */
    .OlSpeedThr_Hz       = MOTOR_PANASONIC_BLOWER_OL_SPEED_THR_Hz,
    .OlAccelRate_Hzps    = MOTOR_PANASONIC_BLOWER_OL_ACCEL_RATE_Hzps,
    .OlThetaRampRate_dps = MOTOR_PANASONIC_BLOWER_OL_THETA_RAMP_RATE_dps,
    .OlTorqueCurrent_A   = MOTOR_PANASONIC_BLOWER_OL_TORQUE_CURRENT_A,

    /* Alignment */
    .AlignCurrent_A = MOTOR_PANASONIC_BLOWER_ALIGN_CURRENT_A,

    /* Flying start */
    .FlyingStartMinSpeed_Hz = MOTOR_PANASONIC_BLOWER_FLYING_START_MIN_SPEED_Hz,

    /* IPD */
    .IpdPulseCurrent_A   = MOTOR_PANASONIC_BLOWER_IPD_PULSE_CURRENT_A,
    .IpdTimeoutFault_sec = MOTOR_PANASONIC_BLOWER_IPD_TIMEOUT_FAULT_sec,
    .IpdCycleCountMax    = MOTOR_PANASONIC_BLOWER_IPD_CYCLE_COUNT_MAX,

    /* Control state timing */
    .FlyingStartDelay_sec = MOTOR_PANASONIC_BLOWER_FLYING_START_DELAY_sec,
    .AlignDelay_sec       = MOTOR_PANASONIC_BLOWER_ALIGN_DELAY_sec,
    .ForceRunDelay_sec    = MOTOR_PANASONIC_BLOWER_FORCE_RUN_DELAY_sec,
    .FwcDelay_sec         = MOTOR_PANASONIC_BLOWER_FWC_DELAY_sec,

    /* ESMO observer gains */
    .EsmoKslideMax     = MOTOR_PANASONIC_BLOWER_ESMO_KSLIDE_MAX,
    .EsmoKslideMin     = MOTOR_PANASONIC_BLOWER_ESMO_KSLIDE_MIN,
    .EsmoPllKpMax      = MOTOR_PANASONIC_BLOWER_ESMO_PLL_KP_MAX,
    .EsmoPllKpMin      = MOTOR_PANASONIC_BLOWER_ESMO_PLL_KP_MIN,
    .EsmoPllKpSf       = MOTOR_PANASONIC_BLOWER_ESMO_PLL_KP_SF,
    .EsmoPllKi         = MOTOR_PANASONIC_BLOWER_ESMO_PLL_KI,
    .EsmoBemfThreshold = MOTOR_PANASONIC_BLOWER_ESMO_BEMF_THRESHOLD,
    .EsmoBemfKslfFcSf  = MOTOR_PANASONIC_BLOWER_ESMO_BEMF_KSLF_FC_SF,
    .EsmoThetaOffsetSf = MOTOR_PANASONIC_BLOWER_ESMO_THETA_OFFSET_SF,
    .EsmoSpeedLpfFc_Hz = MOTOR_PANASONIC_BLOWER_ESMO_SPEED_LPF_FC_Hz,

    /* Flux Observer gains */
    .FluxObsInvKeInit       = MOTOR_PANASONIC_BLOWER_FLUX_OBS_INVKE_INIT,
    .FluxObsInvKeMaxFactor  = MOTOR_PANASONIC_BLOWER_FLUX_OBS_INVKE_MAX_FACTOR,
    .FluxObsInvKeMinFactor  = MOTOR_PANASONIC_BLOWER_FLUX_OBS_INVKE_MIN_FACTOR,
    .FluxObsBemfFiltFactor  = MOTOR_PANASONIC_BLOWER_FLUX_OBS_BEMF_FILT_FACTOR,
    .FluxObsSpeedFiltFactor = MOTOR_PANASONIC_BLOWER_FLUX_OBS_SPEED_FILT_FACTOR,
    .FluxObsSpeedLpfFc_Hz   = MOTOR_PANASONIC_BLOWER_FLUX_OBS_SPEED_LPF_FC_Hz,
    .FluxObsTrapIntDisable  = MOTOR_PANASONIC_BLOWER_FLUX_OBS_TRAP_INT_DISABLE,
    .FluxObsInductorFiltEn  = MOTOR_PANASONIC_BLOWER_FLUX_OBS_INDUCTOR_FILT_EN,

    /* Hall sensor calibration parameters */
    .HallCalAlignCurrent_A = 2.0f,
    .HallCalSweepCurrent_A = 2.0f,
    .HallCalStepSize_rad   = (float32_t)(MATH_PI / 90.0f),
    .HallCalWaitTime_sec   = 0.1f,
    .HallCalMaxCurrent_A   = 5.0f,
    .HallCalMinCurrent_A   = 0.5f,
    .HallCalMaxSteps       = 180U,
};
/*===========================================================================
 * PRO_RANGE_DRONE
 *===========================================================================*/
const Motor_ParamsType Motor_proRangeDrone2300Params = {
    /* Nameplate */
    .NumPolePairs   = MOTOR_PRO_RANGE_DRONE_2300_NUM_POLE_PAIRS,
    .Rs_Ohm         = MOTOR_PRO_RANGE_DRONE_2300_Rs_Ohm,
    .Ls_d_H         = MOTOR_PRO_RANGE_DRONE_2300_Ls_d_H,
    .Ls_q_H         = MOTOR_PRO_RANGE_DRONE_2300_Ls_q_H,
    .RatedFlux_VpHz = MOTOR_PRO_RANGE_DRONE_2300_RATED_FLUX_VpHz,
    .RatedCurrent_A = MOTOR_PRO_RANGE_DRONE_2300_RATED_CURRENT_A,
    .Inertia_Kgm2   = MOTOR_PRO_RANGE_DRONE_2300_INERTIA_Kgm2,
    .RatedSpeed_Hz  = MOTOR_PRO_RANGE_DRONE_2300_RATED_SPEED_Hz,
    .RatedVoltage_V = MOTOR_PRO_RANGE_DRONE_2300_RATED_VOLTAGE_V,

    /* Speed Tuning */
    .SpdBwRatio       = MOTOR_PRO_RANGE_DRONE_2300_SPD_BW_RATIO,
    .ClAccelRate_Hzps = MOTOR_PRO_RANGE_DRONE_2300_CL_ACCEL_RATE_Hzps,

    /* Fault Limits */
    .OverCurrentLim_A = MOTOR_PRO_RANGE_DRONE_2300_OVER_CURRENT_LIM_A,

    /* Open-Loop Startup */
    .OlSpeedThr_Hz       = MOTOR_PRO_RANGE_DRONE_2300_OL_SPEED_THR_Hz,
    .OlAccelRate_Hzps    = MOTOR_PRO_RANGE_DRONE_2300_OL_ACCEL_RATE_Hzps,
    .OlThetaRampRate_dps = MOTOR_PRO_RANGE_DRONE_2300_OL_THETA_RAMP_RATE_dps,
    .OlTorqueCurrent_A   = MOTOR_PRO_RANGE_DRONE_2300_OL_TORQUE_CURRENT_A,

    /* Alignment */
    .AlignCurrent_A = MOTOR_PRO_RANGE_DRONE_2300_ALIGN_CURRENT_A,

    /* Flying Start */
    .FlyingStartMinSpeed_Hz = MOTOR_PRO_RANGE_DRONE_2300_FLYING_START_MIN_SPEED_Hz,

    /* IPD */
    .IpdPulseCurrent_A   = MOTOR_PRO_RANGE_DRONE_2300_IPD_PULSE_CURRENT_A,
    .IpdTimeoutFault_sec = MOTOR_PRO_RANGE_DRONE_2300_IPD_TIMEOUT_FAULT_sec,
    .IpdCycleCountMax    = MOTOR_PRO_RANGE_DRONE_2300_IPD_CYCLE_COUNT_MAX,

    /* State Timing */
    .FlyingStartDelay_sec = MOTOR_PRO_RANGE_DRONE_2300_FLYING_START_DELAY_sec,
    .AlignDelay_sec       = MOTOR_PRO_RANGE_DRONE_2300_ALIGN_DELAY_sec,
    .ForceRunDelay_sec    = MOTOR_PRO_RANGE_DRONE_2300_FORCE_RUN_DELAY_sec,
    .FwcDelay_sec         = MOTOR_PRO_RANGE_DRONE_2300_FWC_DELAY_sec,

    /* ESMO Observer Gains */
    .EsmoKslideMax     = MOTOR_PRO_RANGE_DRONE_2300_ESMO_KSLIDE_MAX,
    .EsmoKslideMin     = MOTOR_PRO_RANGE_DRONE_2300_ESMO_KSLIDE_MIN,
    .EsmoPllKpMax      = MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KP_MAX,
    .EsmoPllKpMin      = MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KP_MIN,
    .EsmoPllKpSf       = MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KP_SF,
    .EsmoPllKi         = MOTOR_PRO_RANGE_DRONE_2300_ESMO_PLL_KI,
    .EsmoBemfThreshold = MOTOR_PRO_RANGE_DRONE_2300_ESMO_BEMF_THRESHOLD,
    .EsmoBemfKslfFcSf  = MOTOR_PRO_RANGE_DRONE_2300_ESMO_BEMF_KSLF_FC_SF,
    .EsmoThetaOffsetSf = MOTOR_PRO_RANGE_DRONE_2300_ESMO_THETA_OFFSET_SF,
    .EsmoSpeedLpfFc_Hz = MOTOR_PRO_RANGE_DRONE_2300_ESMO_SPEED_LPF_FC_Hz,

    /* Flux Observer Gains */
    .FluxObsInvKeInit       = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INVKE_INIT,
    .FluxObsInvKeMaxFactor  = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INVKE_MAX_FACTOR,
    .FluxObsInvKeMinFactor  = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INVKE_MIN_FACTOR,
    .FluxObsBemfFiltFactor  = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_BEMF_FILT_FACTOR,
    .FluxObsSpeedFiltFactor = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_SPEED_FILT_FACTOR,
    .FluxObsSpeedLpfFc_Hz   = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_SPEED_LPF_FC_Hz,
    .FluxObsTrapIntDisable  = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_TRAP_INT_DISABLE,
    .FluxObsInductorFiltEn  = MOTOR_PRO_RANGE_DRONE_2300_FLUX_OBS_INDUCTOR_FILT_EN,

    /* Hall sensor calibration parameters */
    .HallCalAlignCurrent_A = 2.0f,
    .HallCalSweepCurrent_A = 2.0f,
    .HallCalStepSize_rad   = (float32_t)(MATH_PI / 90.0f),
    .HallCalWaitTime_sec   = 0.1f,
    .HallCalMaxCurrent_A   = 5.0f,
    .HallCalMinCurrent_A   = 0.5f,
    .HallCalMaxSteps       = 180U,
};
/*===========================================================================
 * KV_880_DRONE
 *===========================================================================*/
const Motor_ParamsType Motor_KV_880DroneParams = {
    /* Nameplate */
    .NumPolePairs   = MOTOR_KV_880_DRONE_NUM_POLE_PAIRS,
    .Rs_Ohm         = MOTOR_KV_880_DRONE_Rs_Ohm,
    .Ls_d_H         = MOTOR_KV_880_DRONE_Ls_d_H,
    .Ls_q_H         = MOTOR_KV_880_DRONE_Ls_q_H,
    .RatedFlux_VpHz = MOTOR_KV_880_DRONE_RATED_FLUX_VpHz,
    .RatedCurrent_A = MOTOR_KV_880_DRONE_RATED_CURRENT_A,
    .Inertia_Kgm2   = MOTOR_KV_880_DRONE_INERTIA_Kgm2,
    .RatedSpeed_Hz  = MOTOR_KV_880_DRONE_RATED_SPEED_Hz,
    .RatedVoltage_V = MOTOR_KV_880_DRONE_RATED_VOLTAGE_V,

    /* Speed Tuning */
    .SpdBwRatio       = MOTOR_KV_880_DRONE_SPD_BW_RATIO,
    .ClAccelRate_Hzps = MOTOR_KV_880_DRONE_CL_ACCEL_RATE_Hzps,

    /* Fault Limits */
    .OverCurrentLim_A = MOTOR_KV_880_DRONE_OVER_CURRENT_LIM_A,

    /* Open-Loop Startup */
    .OlSpeedThr_Hz       = MOTOR_KV_880_DRONE_OL_SPEED_THR_Hz,
    .OlAccelRate_Hzps    = MOTOR_KV_880_DRONE_OL_ACCEL_RATE_Hzps,
    .OlThetaRampRate_dps = MOTOR_KV_880_DRONE_OL_THETA_RAMP_RATE_dps,
    .OlTorqueCurrent_A   = MOTOR_KV_880_DRONE_OL_TORQUE_CURRENT_A,

    /* Alignment */
    .AlignCurrent_A = MOTOR_KV_880_DRONE_ALIGN_CURRENT_A,

    /* Flying Start */
    .FlyingStartMinSpeed_Hz = MOTOR_KV_880_DRONE_FLYING_START_MIN_SPEED_Hz,

    /* IPD */
    .IpdPulseCurrent_A   = MOTOR_KV_880_DRONE_IPD_PULSE_CURRENT_A,
    .IpdTimeoutFault_sec = MOTOR_KV_880_DRONE_IPD_TIMEOUT_FAULT_sec,
    .IpdCycleCountMax    = MOTOR_KV_880_DRONE_IPD_CYCLE_COUNT_MAX,

    /* State Timing */
    .FlyingStartDelay_sec = MOTOR_KV_880_DRONE_FLYING_START_DELAY_sec,
    .AlignDelay_sec       = MOTOR_KV_880_DRONE_ALIGN_DELAY_sec,
    .ForceRunDelay_sec    = MOTOR_KV_880_DRONE_FORCE_RUN_DELAY_sec,
    .FwcDelay_sec         = MOTOR_KV_880_DRONE_FWC_DELAY_sec,

    /* ESMO Observer Gains */
    .EsmoKslideMax     = MOTOR_KV_880_DRONE_ESMO_KSLIDE_MAX,
    .EsmoKslideMin     = MOTOR_KV_880_DRONE_ESMO_KSLIDE_MIN,
    .EsmoPllKpMax      = MOTOR_KV_880_DRONE_ESMO_PLL_KP_MAX,
    .EsmoPllKpMin      = MOTOR_KV_880_DRONE_ESMO_PLL_KP_MIN,
    .EsmoPllKpSf       = MOTOR_KV_880_DRONE_ESMO_PLL_KP_SF,
    .EsmoPllKi         = MOTOR_KV_880_DRONE_ESMO_PLL_KI,
    .EsmoBemfThreshold = MOTOR_KV_880_DRONE_ESMO_BEMF_THRESHOLD,
    .EsmoBemfKslfFcSf  = MOTOR_KV_880_DRONE_ESMO_BEMF_KSLF_FC_SF,
    .EsmoThetaOffsetSf = MOTOR_KV_880_DRONE_ESMO_THETA_OFFSET_SF,
    .EsmoSpeedLpfFc_Hz = MOTOR_KV_880_DRONE_ESMO_SPEED_LPF_FC_Hz,

    /* Flux Observer Gains */
    .FluxObsInvKeInit       = MOTOR_KV_880_DRONE_FLUX_OBS_INVKE_INIT,
    .FluxObsInvKeMaxFactor  = MOTOR_KV_880_DRONE_FLUX_OBS_INVKE_MAX_FACTOR,
    .FluxObsInvKeMinFactor  = MOTOR_KV_880_DRONE_FLUX_OBS_INVKE_MIN_FACTOR,
    .FluxObsBemfFiltFactor  = MOTOR_KV_880_DRONE_FLUX_OBS_BEMF_FILT_FACTOR,
    .FluxObsSpeedFiltFactor = MOTOR_KV_880_DRONE_FLUX_OBS_SPEED_FILT_FACTOR,
    .FluxObsSpeedLpfFc_Hz   = MOTOR_KV_880_DRONE_FLUX_OBS_SPEED_LPF_FC_Hz,
    .FluxObsTrapIntDisable  = MOTOR_KV_880_DRONE_FLUX_OBS_TRAP_INT_DISABLE,
    .FluxObsInductorFiltEn  = MOTOR_KV_880_DRONE_FLUX_OBS_INDUCTOR_FILT_EN,

    /* Hall sensor calibration parameters */
    .HallCalAlignCurrent_A = 2.0f,
    .HallCalSweepCurrent_A = 2.0f,
    .HallCalStepSize_rad   = (float32_t)(MATH_PI / 90.0f),
    .HallCalWaitTime_sec   = 0.1f,
    .HallCalMaxCurrent_A   = 5.0f,
    .HallCalMinCurrent_A   = 0.5f,
    .HallCalMaxSteps       = 180U,
};
/*===========================================================================
 * High Power Drone Parameters
 *===========================================================================*/
const Motor_ParamsType Motor_droneHpParams = {
    /* Nameplate */
    .NumPolePairs   = MOTOR_DRONE_HP_POLE_PAIRS,
    .Rs_Ohm         = MOTOR_DRONE_HP_Rs_Ohm,
    .Ls_d_H         = MOTOR_DRONE_HP_Ls_d_H,
    .Ls_q_H         = MOTOR_DRONE_HP_Ls_q_H,
    .RatedFlux_VpHz = MOTOR_DRONE_HP_RATED_FLUX_VpHz,
    .RatedCurrent_A = MOTOR_DRONE_HP_RATED_CURRENT_A,
    .Inertia_Kgm2   = MOTOR_DRONE_HP_INERTIA_Kgm2,
    .RatedSpeed_Hz  = MOTOR_DRONE_HP_RATED_SPEED_Hz,
    .RatedVoltage_V = MOTOR_DRONE_HP_RATED_VOLTAGE_V,

    /* Speed Tuning */
    .SpdBwRatio       = MOTOR_DRONE_HP_SPD_BW_RATIO,
    .ClAccelRate_Hzps = MOTOR_DRONE_HP_CL_ACCEL_RATE_Hzps,

    /* Fault Limits */
    .OverCurrentLim_A = MOTOR_DRONE_HP_OVER_CURRENT_LIM_A,

    /* Open-Loop Startup */
    .OlSpeedThr_Hz       = MOTOR_DRONE_HP_OL_SPEED_THR_Hz,
    .OlAccelRate_Hzps    = MOTOR_DRONE_HP_OL_ACCEL_RATE_Hzps,
    .OlThetaRampRate_dps = MOTOR_DRONE_HP_OL_THETA_RAMP_RATE_dps,
    .OlTorqueCurrent_A   = MOTOR_DRONE_HP_OL_TORQUE_CURRENT_A,

    /* Alignment */
    .AlignCurrent_A = MOTOR_DRONE_HP_ALIGN_CURRENT_A,

    /* Flying Start */
    .FlyingStartMinSpeed_Hz = MOTOR_DRONE_HP_FLYING_START_MIN_SPEED_Hz,

    /* IPD */
    .IpdPulseCurrent_A   = MOTOR_DRONE_HP_IPD_PULSE_CURRENT_A,
    .IpdTimeoutFault_sec = MOTOR_DRONE_HP_IPD_TIMEOUT_FAULT_sec,
    .IpdCycleCountMax    = MOTOR_DRONE_HP_IPD_CYCLE_COUNT_MAX,

    /* State Timing */
    .FlyingStartDelay_sec = MOTOR_DRONE_HP_FLYING_START_DELAY_sec,
    .AlignDelay_sec       = MOTOR_DRONE_HP_ALIGN_DELAY_sec,
    .ForceRunDelay_sec    = MOTOR_DRONE_HP_FORCE_RUN_DELAY_sec,
    .FwcDelay_sec         = MOTOR_DRONE_HP_FWC_DELAY_sec,

    /* ESMO Observer Gains */
    .EsmoKslideMax     = MOTOR_DRONE_HP_ESMO_KSLIDE_MAX,
    .EsmoKslideMin     = MOTOR_DRONE_HP_ESMO_KSLIDE_MIN,
    .EsmoPllKpMax      = MOTOR_DRONE_HP_ESMO_PLL_KP_MAX,
    .EsmoPllKpMin      = MOTOR_DRONE_HP_ESMO_PLL_KP_MIN,
    .EsmoPllKpSf       = MOTOR_DRONE_HP_ESMO_PLL_KP_SF,
    .EsmoPllKi         = MOTOR_DRONE_HP_ESMO_PLL_KI,
    .EsmoBemfThreshold = MOTOR_DRONE_HP_ESMO_BEMF_THRESHOLD,
    .EsmoBemfKslfFcSf  = MOTOR_DRONE_HP_ESMO_BEMF_KSLF_FC_SF,
    .EsmoThetaOffsetSf = MOTOR_DRONE_HP_ESMO_THETA_OFFSET_SF,
    .EsmoSpeedLpfFc_Hz = MOTOR_DRONE_HP_ESMO_SPEED_LPF_FC_Hz,

    /* Flux Observer gains */
    .FluxObsInvKeInit       = MOTOR_DRONE_HP_FLUX_OBS_INVKE_INIT,
    .FluxObsInvKeMaxFactor  = MOTOR_DRONE_HP_FLUX_OBS_INVKE_MAX_FACTOR,
    .FluxObsInvKeMinFactor  = MOTOR_DRONE_HP_FLUX_OBS_INVKE_MIN_FACTOR,
    .FluxObsBemfFiltFactor  = MOTOR_DRONE_HP_FLUX_OBS_BEMF_FILT_FACTOR,
    .FluxObsSpeedFiltFactor = MOTOR_DRONE_HP_FLUX_OBS_SPEED_FILT_FACTOR,
    .FluxObsSpeedLpfFc_Hz   = MOTOR_DRONE_HP_FLUX_OBS_SPEED_LPF_FC_Hz,
    .FluxObsTrapIntDisable  = MOTOR_DRONE_HP_FLUX_OBS_TRAP_INT_DISABLE,
    .FluxObsInductorFiltEn  = MOTOR_DRONE_HP_FLUX_OBS_INDUCTOR_FILT_EN,

    /* Hall sensor calibration parameters */
    .HallCalAlignCurrent_A = 2.0f,
    .HallCalSweepCurrent_A = 2.0f,
    .HallCalStepSize_rad   = (float32_t)(MATH_PI / 90.0f),
    .HallCalWaitTime_sec   = 0.1f,
    .HallCalMaxCurrent_A   = 5.0f,
    .HallCalMinCurrent_A   = 0.5f,
    .HallCalMaxSteps       = 180U,

};

/*===========================================================================
 * Anaheim Automation BLY171S-24V-4000 (DRV8376EVM bench, 12 V, 0.5 A adapter)
 * Values and their sources: the MOTOR_BLY171S_* block in motor_params.h
 *===========================================================================*/
const Motor_ParamsType Motor_anaheimBly171sParams = {
    /* Nameplate */
    .NumPolePairs   = MOTOR_BLY171S_NUM_POLE_PAIRS,
    .Rs_Ohm         = MOTOR_BLY171S_Rs_Ohm,
    .Ls_d_H         = MOTOR_BLY171S_Ls_d_H,
    .Ls_q_H         = MOTOR_BLY171S_Ls_q_H,
    .RatedFlux_VpHz = MOTOR_BLY171S_RATED_FLUX_VpHz,
    .RatedCurrent_A = MOTOR_BLY171S_RATED_CURRENT_A,
    .Inertia_Kgm2   = MOTOR_BLY171S_INERTIA_Kgm2,
    .RatedSpeed_Hz  = MOTOR_BLY171S_RATED_SPEED_Hz,
    .RatedVoltage_V = MOTOR_BLY171S_RATED_VOLTAGE_V,

    /* Closed-loop speed tuning */
    .SpdBwRatio       = MOTOR_BLY171S_SPD_BW_RATIO,
    .ClAccelRate_Hzps = MOTOR_BLY171S_CL_ACCEL_RATE_Hzps,

    /* Fault limits */
    .OverCurrentLim_A = MOTOR_BLY171S_OVER_CURRENT_LIM_A,

    /* Open-loop startup */
    .OlSpeedThr_Hz       = MOTOR_BLY171S_OL_SPEED_THR_Hz,
    .OlAccelRate_Hzps    = MOTOR_BLY171S_OL_ACCEL_RATE_Hzps,
    .OlThetaRampRate_dps = MOTOR_BLY171S_OL_THETA_RAMP_RATE_dps,
    .OlTorqueCurrent_A   = MOTOR_BLY171S_OL_TORQUE_CURRENT_A,

    /* Alignment */
    .AlignCurrent_A = MOTOR_BLY171S_ALIGN_CURRENT_A,

    /* Flying start */
    .FlyingStartMinSpeed_Hz = MOTOR_BLY171S_FLYING_START_MIN_SPEED_Hz,

    /* IPD */
    .IpdPulseCurrent_A   = MOTOR_BLY171S_IPD_PULSE_CURRENT_A,
    .IpdTimeoutFault_sec = MOTOR_BLY171S_IPD_TIMEOUT_FAULT_sec,
    .IpdCycleCountMax    = MOTOR_BLY171S_IPD_CYCLE_COUNT_MAX,

    /* Control state timing */
    .FlyingStartDelay_sec = MOTOR_BLY171S_FLYING_START_DELAY_sec,
    .AlignDelay_sec       = MOTOR_BLY171S_ALIGN_DELAY_sec,
    .ForceRunDelay_sec    = MOTOR_BLY171S_FORCE_RUN_DELAY_sec,
    .FwcDelay_sec         = MOTOR_BLY171S_FWC_DELAY_sec,

    /* ESMO observer gains */
    .EsmoKslideMax     = MOTOR_BLY171S_ESMO_KSLIDE_MAX,
    .EsmoKslideMin     = MOTOR_BLY171S_ESMO_KSLIDE_MIN,
    .EsmoPllKpMax      = MOTOR_BLY171S_ESMO_PLL_KP_MAX,
    .EsmoPllKpMin      = MOTOR_BLY171S_ESMO_PLL_KP_MIN,
    .EsmoPllKpSf       = MOTOR_BLY171S_ESMO_PLL_KP_SF,
    .EsmoPllKi         = MOTOR_BLY171S_ESMO_PLL_KI,
    .EsmoBemfThreshold = MOTOR_BLY171S_ESMO_BEMF_THRESHOLD,
    .EsmoBemfKslfFcSf  = MOTOR_BLY171S_ESMO_BEMF_KSLF_FC_SF,
    .EsmoThetaOffsetSf = MOTOR_BLY171S_ESMO_THETA_OFFSET_SF,
    .EsmoSpeedLpfFc_Hz = MOTOR_BLY171S_ESMO_SPEED_LPF_FC_Hz,

    /* Flux Observer gains */
    .FluxObsInvKeInit       = MOTOR_BLY171S_FLUX_OBS_INVKE_INIT,
    .FluxObsInvKeMaxFactor  = MOTOR_BLY171S_FLUX_OBS_INVKE_MAX_FACTOR,
    .FluxObsInvKeMinFactor  = MOTOR_BLY171S_FLUX_OBS_INVKE_MIN_FACTOR,
    .FluxObsBemfFiltFactor  = MOTOR_BLY171S_FLUX_OBS_BEMF_FILT_FACTOR,
    .FluxObsSpeedFiltFactor = MOTOR_BLY171S_FLUX_OBS_SPEED_FILT_FACTOR,
    .FluxObsSpeedLpfFc_Hz   = MOTOR_BLY171S_FLUX_OBS_SPEED_LPF_FC_Hz,
    .FluxObsTrapIntDisable  = MOTOR_BLY171S_FLUX_OBS_TRAP_INT_DISABLE,
    .FluxObsInductorFiltEn  = MOTOR_BLY171S_FLUX_OBS_INDUCTOR_FILT_EN,

    /* Hall sensor calibration parameters */
    .HallCalAlignCurrent_A = MOTOR_BLY171S_HALL_CAL_ALIGN_CURRENT_A,
    .HallCalSweepCurrent_A = MOTOR_BLY171S_HALL_CAL_SWEEP_CURRENT_A,
    .HallCalStepSize_rad   = (float32_t)(MATH_PI / 90.0f),
    .HallCalWaitTime_sec   = 0.1f,
    .HallCalMaxCurrent_A   = MOTOR_BLY171S_HALL_CAL_MAX_CURRENT_A,
    .HallCalMinCurrent_A   = MOTOR_BLY171S_HALL_CAL_MIN_CURRENT_A,
    .HallCalMaxSteps       = 180U,
};

/*===========================================================================
 * Active motor selection — change this pointer to switch motors.
 * Also update MOTOR_PH_Ls_d_H / MOTOR_PH_Ls_q_H in motor_params.h.
 *===========================================================================*/
const Motor_ParamsType *const motorParams_M1 = &Motor_anaheimBly171sParams;
