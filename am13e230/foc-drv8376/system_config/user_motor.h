/*
 * Copyright (c) 2021 Texas Instruments Incorporated - http://www.ti.com
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
 *
 */

/**
 * \file user_motor.h
 * \brief Motor board and algorithm configuration parameters.
 *
 * This file contains only board-level and algorithm-level constants.
 * Motor nameplate and tuning data (Rs, Ls, flux, ESMO gains, etc.) are
 * stored in the \c Motor_ParamsType constant table in motor_params.h and
 * motor_params.c.  To change the active motor, update the
 * \c motorParams_M1 pointer in motor_params.c.
 *
 * \defgroup USER_MOTOR_MODULE Motor Configuration Module
 * \brief User-configurable parameters for motor
 * @{
 */

#ifndef USER_MOTOR_H
#define USER_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "userParams.h"

/* =========================================================================
 * Section A — System / Board Constants
 * =========================================================================
 * These values are fixed by the hardware platform and do not change when
 * a different motor is connected.
 * ========================================================================= */

/**
 * \anchor USER_MOTOR_SYSTEM_CONFIG
 * \name System Configuration
 * @{
 */
#define USER_SYSTEM_FREQ_MHz (float32_t)(200.0f)  //!< System clock frequency in MHz
/** @} */

/**
 * \anchor USER_MOTOR_ISR_TIMING
 * \name ISR and PWM Timing
 * @{
 */
#define USER_M1_NUM_PWM_TICKS_PER_ISR_TICK     (1)   //!< PWM clock ticks per ISR tick (valid: 1, 2, 3)
#define USER_M1_NUM_ISR_TICKS_PER_CURRENT_TICK (1)   //!< ISR ticks per current controller tick
#define USER_M1_NUM_ISR_TICKS_PER_SPEED_TICK   (10)  //!< ISR ticks per speed controller tick
/** @} */

/**
 * \anchor USER_MOTOR_PWM_CONFIG
 * \name PWM Configuration
 * @{
 */
#define USER_M1_PWM_FREQ_kHz (20)  //!< Motor 1 PWM frequency in kHz (SDK: 50). 20 kHz leaves 8 to 92 percent duty with the 2 us minimum compare
#define USER_M1_BENCH_MAX_CURRENT_A (0.4f)  //!< Limit the first FOC runs to the 12 V, 0.5 A bench adapter; independent of the motor's 1.8 A rating
#define USER_M1_PWM_TBPRD_NUM \
    (uint16_t)(USER_SYSTEM_FREQ_MHz * 1000.0 / USER_M1_PWM_FREQ_kHz / 2.0)  //!< PWM time base period
#define USER_M1_PWM_PERIOD_usec (1000.0f / USER_M1_PWM_FREQ_kHz)            //!< PWM period in microseconds
#define USER_M1_ISR_FREQ_Hz \
    (USER_M1_PWM_FREQ_kHz * 1000.0f / (float32_t)USER_M1_NUM_PWM_TICKS_PER_ISR_TICK)  //!< ISR frequency in Hz
#define USER_M1_ISR_PERIOD_usec \
    (USER_M1_PWM_PERIOD_usec * (float32_t)USER_M1_NUM_PWM_TICKS_PER_ISR_TICK)  //!< ISR period in microseconds
/** @} */

/**
 * \anchor USER_MOTOR_CONTROLLER_TIMING
 * \name Controller Timing
 * @{
 */
#define USER_M1_CTRL_PERIOD_usec ((float32_t)USER_M1_ISR_PERIOD_usec)  //!< Controller execution period in usec
#define USER_M1_CTRL_PERIOD_sec \
    ((float32_t)USER_M1_CTRL_PERIOD_usec / 1000000.0f)  //!< Controller execution period in sec
/** @} */

/**
 * \anchor USER_MOTOR_FILTER_POLES
 * \name Filter Pole Locations
 * @{
 */
#define USER_M1_DCBUS_POLE_rps       (100)                   //!< DC bus filter pole location in rad/sec
#define USER_M1_OFFSET_POLE_rps      (20)                    //!< Voltage/current offset estimation pole in rad/s
#define USER_M1_SPEED_POLE_rps       (100)                   //!< Speed control filter pole in rad/sec
#define USER_M1_DIRECTION_POLE_rps   (MATH_TWO_PI * 10.0f)   //!< Direction filter pole in rad/sec
#define USER_M1_DIRECTION_POLE_2_rps (MATH_TWO_PI * 100.0f)  //!< Second direction filter pole in rad/sec
/** @} */

/**
 * \anchor USER_MOTOR_QEP_CONFIG
 * \name QEP Configuration
 * @{
 */
#define USER_M1_QEP_UNIT_TIMER_TICKS \
    (uint32_t)(USER_SYSTEM_FREQ_MHz / (2.0f * USER_M1_ISR_FREQ_Hz) * 1000000.0f)  //!< QEP unit timer ticks
/** @} */

/* =========================================================================
 * Section B — Algorithm / Estimator Tuning Constants
 * =========================================================================
 * These values tune the InstaSPIN estimator and FOC algorithm.  They are
 * independent of the motor nameplate data.
 * ========================================================================= */

/**
 * \anchor USER_MOTOR_INSTASPIN_EST
 * \name InstaSpin Estimation Configuration
 * @{
 */
#define INSTASPIN_EST_RSONLINE_WAIT_TIME_MS (60000U)  //!< Rs online calibration wait time in ms
#define INSTASPIN_EST_RSONLINE_WORK_TIME_MS (24000U)  //!< Rs online calibration work time in ms
#define INSTASPIN_EST_RES_CURRENT_A         (0.4)     //!< Resistance estimation current in Amps (SDK 1.5; capped for the 0.5 A bench adapter)
#define INSTASPIN_EST_IND_CURRENT_A         (0.4)     //!< Inductance estimation current in Amps (SDK -1; capped for the 0.5 A bench adapter)
#define INSTASPIN_EST_Ls_d_COMP_COEF        (0.15f)   //!< Ls_d compensation coefficient (0.0f~0.5f)
#define INSTASPIN_EST_Ls_q_COMP_COEF        (0.35f)   //!< Ls_q compensation coefficient (0.0f~0.5f)
#define INSTASPIN_EST_Ls_MIN_NUM_COEF       (0.55f)   //!< Ls minimum number coefficient (0.5f~1.0f)
#define INSTASPIN_EST_FLUX_EXC_FREQ_Hz      (600)     //!< Flux excitation frequency in Hz
#define INSTASPIN_EST_MAX_CURRENT_DELTA_A \
    (INSTASPIN_EST_RES_CURRENT_A / USER_M1_ISR_FREQ_Hz)  //!< Maximum current slope for Id trajectory
#define INSTASPIN_EST_MAX_CURRENT_DELTA_PW_A \
    (0.3 * INSTASPIN_EST_RES_CURRENT_A /     \
     USER_M1_ISR_FREQ_Hz)                        //!< Maximum current slope for Id trajectory during power warp mode
#define INSTASPIN_EST_FREQ_NEARZEROLIMIT_Hz (5)  //!< Near zero frequency limit in Hz
/** @} */

/**
 * \anchor USER_MOTOR_RL_ESTIMATION
 * \name R/L Estimation Parameters
 * @{
 */
#define USER_M1_R_OVER_L_EXC_FREQ_Hz (300)    //!< R/L excitation frequency in Hz
#define USER_M1_R_OVER_L_KP_SF       (0.02f)  //!< R/L Kp scale factor (Kp = SF * Vdc / Imax)
#define USER_M1_MAX_ACCEL_Hzps       (2)      //!< Maximum acceleration for estimation speed profiles in Hz/sec
/** @} */

/**
 * \anchor USER_MOTOR_ESTIMATION_PARAMS
 * \name Estimation Parameters
 * @{
 */
#define USER_M1_IDRATED_DELTA_A               (0.0001f)  //!< IdRated delta for estimation
#define USER_M1_FORCE_ANGLE_FREQ_Hz           (1)        //!< Forced angle frequency in Hz
#define USER_M1_FORCE_ANGLE_ACCEL_Hzps        (50)       //!< Forced angle acceleration in Hz/s
#define USER_M1_FREQ_NEARZEROSPEEDLIMIT_Hz    (0)        //!< Near zero speed limit for flux integration in Hz
#define USER_M1_IDRATED_FRACTION_FOR_L_IDENT  (0.5f)     //!< Fraction of IdRated for inductance estimation
#define USER_M1_SPEEDMAX_FRACTION_FOR_L_IDENT (1)        //!< Fraction of SpeedMax for inductance estimation
/** @} */

/**
 * \anchor USER_MOTOR_EST_SCALE_FACTORS
 * \name Estimation Scale Factors
 * @{
 */
#define USER_M1_EST_FLUX_HF_SF (0.25f)  //!< Flux estimation scale factor (0.1f~1.25f)
#define USER_M1_EST_FREQ_HF_SF (1)      //!< Frequency estimation scale factor (0.5f~1.5f)
#define USER_M1_EST_BEMF_HF_SF (1)      //!< BEMF estimation scale factor (0.50f~1.25f)
/** @} */

/**
 * \anchor USER_MOTOR_POWER_WARP
 * \name Power Warp Parameters
 * @{
 */
/**
 * \brief Power Warp gain for computing Id reference
 *
 * If motor parameters are known, set this gain to:
 * USER_M1_PW_GAIN = SQRT(1.0 + USER_MOTOR1_Rr_Ohm / USER_MOTOR1_Rs_Ohm)
 */
#define USER_M1_PW_GAIN (1)  //!< Power Warp gain
/** @} */

/* =========================================================================
 * Section C — FOC / Fault / Monitoring Thresholds
 * =========================================================================
 * These values configure the FOC voltage limits, FWC controller, fault
 * detection thresholds, and fault timing counters.
 * ========================================================================= */

/**
 * \anchor USER_MOTOR_VOLTAGE_CONTROL
 * \name Voltage Control Parameters
 * @{
 */
#define USER_M1_VD_SF (0.95)  //!< Direct voltage (Vd) scale factor

/**
 * \brief Maximum Vs magnitude in per units allowed
 *
 * This value sets the maximum magnitude for the output of the Id and
 * Iq PI current controllers. The Id and Iq current controller outputs
 * are Vd and Vq. The relationship between Vs, Vd, and Vq is:
 * Vs = sqrt(Vd^2 + Vq^2). In this FOC controller, the Vd value is set
 * equal to USER_MAX_VS_MAG*USER_VD_MAG_FACTOR, so the Vq value is set
 * equal to sqrt(USER_MAX_VS_MAG^2 - Vd^2).
 *
 * - Set USER_MAX_VS_MAG = 0.5 for a pure sinewave with a peak at
 *   SQRT(3)/2 = 86.6% duty cycle. No current reconstruction needed.
 * - Set USER_MAX_VS_MAG = 1/SQRT(3) = 0.5774 for a pure sinewave
 *   with a peak at 100% duty cycle. Current reconstruction needed.
 * - Set USER_MAX_VS_MAG = 2/3 = 0.6666 to create a trapezoidal
 *   voltage waveform. Current reconstruction needed.
 */
#define USER_M1_MAX_VS_MAG_PU (0.576f)  //!< Maximum Vs magnitude in per units

#define USER_M1_VS_REF_MAG_PU (0.8f * USER_M1_MAX_VS_MAG_PU)  //!< Reference Vs magnitude (0.5 to 0.95 of max)
/** @} */

/**
 * \anchor USER_MOTOR_CURRENT_CONTROL
 * \name Current Control Parameters
 * @{
 */
#define USER_M1_MAX_NEGATIVE_ID_REF_CURRENT_A (-2)  //!< Maximum negative Id reference current in Amps
/** @} */

/**
 * \anchor USER_MOTOR_FWC
 * \name Field Weakening Control Parameters
 * @{
 */
#define USER_M1_FWC_KP            (0.0225f)   //!< FWC proportional gain
#define USER_M1_FWC_KI            (0.00225f)  //!< FWC integral gain
#define USER_M1_FWC_MAX_ANGLE     (-15.0f)    //!< FWC maximum current vector angle in degrees
#define USER_M1_FWC_MAX_ANGLE_RAD (USER_M1_FWC_MAX_ANGLE / 180.0f * MATH_PI)  //!< FWC max angle in radians
#define USER_M1_FWC_MIN_ANGLE     (0.0f)  //!< FWC minimum current vector angle in degrees
#define USER_M1_FWC_MIN_ANGLE_RAD (USER_M1_FWC_MIN_ANGLE / 180.0f * MATH_PI)  //!< FWC min angle in radians
/** @} */

/**
 * \anchor USER_MOTOR_SIGNAL_FILTERS
 * \name Signal Filter Parameters
 * @{
 */
#define USER_M1_IS_FILTER_POLE_Hz  (7500.0f)  //!< Current filter pole location in Hz (7.5kHz)
#define USER_M1_IS_FILTER_POLE_rps (MATH_TWO_PI * USER_M1_IS_FILTER_POLE_Hz)  //!< Current filter pole in rad/s
#define USER_M1_VS_FILTER_POLE_Hz  (30000.0f)  //!< Voltage filter pole location in Hz (30.0kHz)
#define USER_M1_VS_FILTER_POLE_rps (MATH_TWO_PI * USER_M1_VS_FILTER_POLE_Hz)  //!< Voltage filter pole in rad/s
/** @} */

/**
 * \anchor USER_MOTOR_FAULT_THRESHOLDS
 * \name Motor Fault Detection Thresholds
 * @{
 */
#define USER_M1_LOST_PHASE_CURRENT_A  (0.01f)      //!< Lost phase current threshold in Amps
#define USER_M1_UNBALANCE_RATIO       (0.2f)       //!< Phase unbalance ratio percent threshold
#define USER_M1_OVER_LOAD_POWER_W     (90.0f)      //!< Over load power threshold in Watts
#define USER_M1_STALL_CURRENT_A       (10.0f)      //!< Motor stall current threshold in Amps
#define USER_M1_FAULT_CHECK_CURRENT_A (0.2f)       //!< Fault check current threshold in Amps
#define USER_M1_FAIL_SPEED_MAX_HZ     (3800.0f)    //!< Failed maximum speed threshold in Hz
#define USER_M1_FAIL_SPEED_MIN_HZ     (5.0f)       //!< Failed minimum speed threshold in Hz
#define USER_M1_TORQUE_FAILED_SET     (0.000001f)  //!< Failed torque threshold
/** @} */

/**
 * \anchor USER_MOTOR_FAULT_TIMING
 * \name Fault Detection Timing Parameters
 * \brief All timer counts use a 5 ms base tick (USER_M1_FAULT_TIMER_BASE_MS).
 *        Actual time = count * USER_M1_FAULT_TIMER_BASE_MS ms.
 * @{
 */
#define USER_M1_FAULT_TIMER_BASE_MS    (5U)    //!< Fault timer base period in ms
#define USER_M1_VOLTAGE_FAULT_TIME_SET (500)   //!< DC bus over/under voltage time (500 * 5ms = 2.5s)
#define USER_M1_OVER_LOAD_TIME_SET     (200)   //!< Motor over load time (200 * 5ms = 1s)
#define USER_M1_STALL_TIME_SET         (200)   //!< Motor stall time (200 * 5ms = 1s)
#define USER_M1_UNBALANCE_TIME_SET     (1000)  //!< Phase unbalanced time (1000 * 5ms = 5s)
#define USER_M1_LOST_PHASE_TIME_SET    (2000)  //!< Lost phase time (2000 * 5ms = 10s)
#define USER_M1_OVER_SPEED_TIME_SET    (600)   //!< Over speed time (600 * 5ms = 3s)
#define USER_M1_STARTUP_FAIL_TIME_SET  (2000)  //!< Startup failed time (2000 * 5ms = 10s)
#define USER_M1_OVER_CURRENT_TIMES_SET (5)     //!< Over current occurrence count
#define USER_M1_STOP_WAIT_TIME_SET     (2000)  //!< Stop wait time (2000 * 5ms = 10s)
#define USER_M1_RESTART_WAIT_TIME_SET  (2000)  //!< Restart wait time (2000 * 5ms = 10s)
#define USER_M1_START_TIMES_SET        (3)     //!< Number of restart attempts
#define USER_M1_ALIGN_TIMES_SET        (2000)  //!< Alignment time
/** @} */

/**
 * \anchor USER_M1_OFFSET_CALIB_TIMING
 * \name Offset Calibration Timing
 * \brief Duration of ISR-synchronous ADC offset calibration.
 *        Total calibration ticks = USER_M1_OFFSET_CALIB_TIME_SEC x ctrlFreq_Hz.
 *        Example: 2.0 s x 20000 Hz = 40000 ISR ticks.
 * @{
 */
#define USER_M1_OFFSET_CALIB_TIME_SEC (2.0f)  //!< Offset calibration duration in seconds
/** @} */

// the globals
extern USER_Params userParams_M1;

#ifdef __cplusplus
}
#endif

#endif /* USER_MOTOR_H */

/** @} */
