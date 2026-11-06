/*
 *  Copyright (C) 2025 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file Ti_Tune.h
 * \brief TI Tune motor parameter identification module
 *
 * \defgroup TITUNE_MODULE TI Tune Module
 * \brief Motor parameter identification and tuning algorithms
 * @{
 */

#ifndef LIBRARIES_TITUNE_H_
#define LIBRARIES_TITUNE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ti_tune_j_coast.h"
#include "ti_tune_ldq.h"
#include "ti_tune_ol.h"
#include "ti_tune_rs.h"

#include "ipark.h"
#include "motor_params.h" /* motorParams_M1 runtime pointer */
#include "park.h"
#include "pi.h"
#include "userParams.h"
#include "user_motor.h" /* board-level and algorithm constants */
#include <stdbool.h>
#include <stdlib.h>
// math_types.h internally includes <math.h> and fastrts_tmu.h for TMU
// acceleration
#include "math_types.h"
#include "types.h"

//*****************************************************************************
//
//! \anchor TITUNE_DEFINES
//! \name TI Tune Configuration Defines
//! @{
//
//*****************************************************************************

/*===========================================================================
 * SECTION A: BOARD CONSTANT
 * Set once per board design. Does not change per motor.
 *===========================================================================*/

//! \brief Maximum board voltage based on ADC full scale (110k/4.99k voltage
//! divider)
#define TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V (76.04549098f)

//! \brief Maximum phase voltage magnitude for SVPWM output in per-unit.
//!        Guard prevents redefinition if user_mtr1.h already defines this.
#ifndef USER_M1_MAX_VS_MAG_PU
#define USER_M1_MAX_VS_MAG_PU (0.576f)
#endif

/*===========================================================================
 * SECTION B: USER-TUNABLE PARAMETERS
 * Review these for each new motor. All have sensible defaults that work
 * for most small PMSMs (1-10A, 50-300 Hz rated speed) without modification.
 * Motor nameplate (current, speed, voltage) is set in user_mtr1.h.
 *===========================================================================*/

/* -- R/L Estimation -------------------------------------------------------- */

//! \brief Current loop bandwidth in rad/sec.
//!        Increase for motors with a low L/R ratio (high-speed,
//!        low-inductance). Decrease for motors with a high L/R ratio (large,
//!        high-inductance). Typical range: 2*pi*50 to 2*pi*200 rad/sec.
#define TI_TUNE_TORQUE_LOOP_BW_RADPS (MATH_TWO_PI * 200.0f)

//! \brief Rs test current fractions (fraction of MOTOR_RATED_CURRENT_A).
//!        Two-point measurement improves accuracy. Keep ratio
//!        approximately 1.25x. Increase if Rs measurement is noisy; decrease
//!        for very small motors.
#define TI_TUNE_RS_TEST_CURRENT1_FRACTION (0.4f)
#define TI_TUNE_RS_TEST_CURRENT2_FRACTION (0.5f)

/* -- J/B Estimation -------------------------------------------------------- */

//! \brief Settling time per steady-state test point (seconds).
//!        The motor is held at each current step for this duration before
//!        recording data. Increase for high-inertia loads (fans, pumps).
//!        Default 5 sec suits most motors.
#define TI_TUNE_J_COAST_SETTLING_TIME_SEC (5.0f)

/* -- Open-Loop Startup ----------------------------------------------------- */

//! \brief Open-loop target speed as fraction of MOTOR_RATED_SPEED_HZ.
//!        Must be above the minimum observer lock speed. Default 20% suits most
//!        motors. Increase to 0.4-0.5 if the observer fails to lock reliably.
#define TI_TUNE_OL_SPEED_TARGET_FRACTION (0.2f)

//! \brief Open-loop Iq current as fraction of MOTOR_RATED_CURRENT_A.
//!        Increase for high-friction or high-inertia loads. Default 20% suits
//!        most motors.
#define TI_TUNE_OL_CURRENT_FRACTION (0.2f)

//! \brief Open-loop D-axis current injection as fraction of rated current.
//!        A small Id injection during OL startup improves angle convergence
//!        stability at low angle error. Default 5% suits most motors.
#define TI_TUNE_OL_ID_FRACTION (0.05f)

//! \brief Open-loop settle duration (seconds) before transitioning to observer.
//!        Time the motor runs at open-loop target speed before handing off to
//!        the observer.
#define TI_TUNE_OL_SETTLE_TIME_SEC (4.0f)

/* -- Closed-Loop Torque Settle --------------------------------------------- */

//! \brief CL Id ramp-down time (seconds).
//!        Time to ramp Id from OL injection level to zero after OL-to-CL handoff.
//!        Actual A/sample = (OL.IdRef_A) / (TI_TUNE_CL_ID_RAMP_TIME_SEC / USER_M1_CTRL_PERIOD_sec)
#define TI_TUNE_CL_ID_RAMP_TIME_SEC (5.0f)

/*===========================================================================
 * SECTION C: INTERNAL ALGORITHM CONSTANTS -- DO NOT MODIFY
 * These are universal for PMSM identification. No per-motor tuning is needed.
 * Changing these values may cause estimation failures.
 *===========================================================================*/

/* -- State machine --------------------------------------------------------- */

//! \brief Inter-state delay count (ISR samples) for state machine transitions
#define TI_TUNE_STATE_DELAY_COUNT (1000U)

/* -- Current controller ---------------------------------------------------- */

//! \brief Ki scaling factor for current controller (fraction of R/L bandwidth).
//!        Ki = TI_TUNE_CURRENT_CTRL_KI_FACTOR * (Rs/L) *
//!        USER_M1_CTRL_PERIOD_sec
#define TI_TUNE_CURRENT_CTRL_KI_FACTOR (0.25f)

/* -- R/L Estimation internals ---------------------------------------------- */

//! \brief Rs averaging window (seconds) -- converted to ISR count at init
#define TI_TUNE_RS_AVERAGING_TIME_SEC (0.333f)

//! \brief Rs current settle time (seconds) -- converted to ISR count at init
#define TI_TUNE_RS_SETTLE_TIME_SEC (1.0f)

//! \brief Rs maximum settle time (seconds) -- converted to ISR count at init
#define TI_TUNE_RS_MAX_SETTLE_TIME_SEC (2.0f)

//! \brief Id error limit for Rs settle detection (fraction of rated current)
#define TI_TUNE_RS_ID_ERR_LIM_FRACTION (0.05f)

//! \brief LDQ injection half-period limits (samples)
#define TI_TUNE_LDQ_INJ_HALF_PRD_MAX  (30.0f)
#define TI_TUNE_LDQ_INJ_HALF_PRD_INIT (8.0f)

//! \brief LDQ injection settle cycles per half-period
#define TI_TUNE_LDQ_INJ_SETTLE_PRD (2.0f)

//! \brief LDQ electrical theta step per injection cycle
#define TI_TUNE_LDQ_ELEC_THETA_STEP (0.004f)

//! \brief LDQ injection voltage initial magnitude (per-unit)
#define TI_TUNE_LDQ_INJ_VOL_MAGN_INIT (0.002f)

//! \brief LDQ current magnitude limits (per-unit)
#define TI_TUNE_LDQ_IS_MAGN_MIN_LIM (0.02f)
#define TI_TUNE_LDQ_IS_MAGN_MAX_LIM (0.5f)

//! \brief LDQ injection voltage magnitude limit (per-unit)
#define TI_TUNE_LDQ_INJ_VOL_MAGN_MAX_LIM (0.98f)

/* -- J/B Estimation internals ---------------------------------------------- */

//! \brief Coast-down sampling period (seconds) -- decimation for regression
#define TI_TUNE_J_COAST_COASTDOWN_SAMPLE_PERIOD_SEC (0.01f)

//! \brief Current ramp rate (per-unit per ISR sample) during acceleration phase.
//!        Actual ramp rate in A/sec = TI_TUNE_J_COAST_CURRENT_RAMP_RATE *
//!        I_Base / samplePeriod_sec
#define TI_TUNE_J_COAST_CURRENT_RAMP_RATE (0.0000001f)

//! \brief Number of speed steps to capture during upward current ramp.
//!        The speed range (targetSpeed - initialSpeed) is divided into this many equal steps.
//!        Each step triggers a settle + data capture. Point 0 (CL entry) is always captured first.
//!        Total data points = 1 (point 0) + numSteps ramp captures.
//!        Default 5 gives 6 total data points.
#define TI_TUNE_J_COAST_NUM_STEPS (5U)

//! \brief Target speed for upward ramp as fraction of rated speed.
//!        Ramp stops when filtered speed reaches this fraction of F_Base.
//!        Default 0.8 (80% of rated speed) gives good G_DC coverage.
#define TI_TUNE_J_COAST_TARGET_SPEED_FRACTION (0.6f)

//! \brief Maximum current limit for upward ramp (fraction of rated current).
//!        Ramp stops if Iq reaches this limit before target speed.
//!        If >= 1 valid data point exists, estimation proceeds to freewheel.
//!        Default 1.0 (100% rated current).
#define TI_TUNE_J_COAST_MAX_CURRENT_FRACTION (1.0f)

//! \brief Coast-down stop threshold (fraction of initial coast-down speed)
#define TI_TUNE_J_COAST_COASTDOWN_STOP_FRACTION (0.2f)

//! \brief IIR low-pass filter cutoff frequency for Iq/Vq signals during INITIAL_SETTLE and SPEED_STEP_SETTLE (Hz)
#define TI_TUNE_J_COAST_IQ_FILTER_CUTOFF_HZ (10.0f)

//! \brief IIR low-pass filter cutoff frequency for speed signal during INITIAL_SETTLE and SPEED_STEP_SETTLE (Hz)
#define TI_TUNE_J_COAST_SPD_FILTER_CUTOFF_HZ (2.0f)

//! \brief Crossover frequency factor for speed PI design (fraction of plant pole wp).
//!        wcs = TI_TUNE_J_COAST_CROSSOVER_FREQ_FACTOR * wp  (standard pole-zero cancellation)
#define TI_TUNE_J_COAST_CROSSOVER_FREQ_FACTOR (0.2f)

/* -- OL/CL internals ------------------------------------------------------- */

//! \brief Open-loop speed ramp rate (Hz/sec) -- frequency-agnostic.
//!        Actual Hz/sample = TI_TUNE_OL_SPEED_RAMP_HZ_PER_SEC *
//!        USER_M1_CTRL_PERIOD_sec
#define TI_TUNE_OL_SPEED_RAMP_HZ_PER_SEC (50.0f)

//! \brief Open-loop current ramp-down interval (seconds).
//!        Time between each multiplicative Iq reduction step during Phase 2
//!        (current ramp-down / angle convergence). Independent of ISR frequency.
//!        Actual sample count = TI_TUNE_OL_CURRENT_RAMPDOWN_INTERVAL_SEC /
//!        USER_M1_CTRL_PERIOD_sec, computed at OL_SETUP time.
#define TI_TUNE_OL_CURRENT_RAMPDOWN_INTERVAL_SEC (2.0f)

//! \brief Angle convergence threshold for OL-to-CL transition (radians).
//!        |angleDelta_rad| must be below this value before transitioning to
//!        closed-loop torque control.
//!        0.1 rad (~5.7 deg): at this angle cos(angleDelta) = 0.995, meaning
//!        99.5% of commanded current produces useful torque -- a safe handoff point.
#define TI_TUNE_OL_ANGLE_CONVERGE_THRESHOLD_RAD (0.25f)

//! \brief Maximum number of multiplicative ramp-down steps before forced OL-to-CL transition.
//!        Timeout safety: if angle has not converged after this many steps, force
//!        transition anyway.
#define TI_TUNE_OL_CL_MAX_REDUCTION_STEPS (25U)

//! \brief Additive offset in the multiplicative OL-to-CL current ramp-down factor.
//!        factor = TI_TUNE_OL_CL_RAMP_FACTOR_OFFSET + |cos(angleDelta)| * TI_TUNE_OL_CL_RAMP_FACTOR_SCALE
//!        |cos| near 1.0 (aligned)  -> factor ~1.0 -> barely reduces current
//!        |cos| near 0.5 (60 deg)   -> factor ~0.75 -> moderate reduction
//!        |cos| near 0.0 (90 deg)   -> factor ~0.5  -> halves current
#define TI_TUNE_OL_CL_RAMP_FACTOR_OFFSET (0.5f)

//! \brief Scale on |cos(angleDelta)| in the multiplicative OL-to-CL current ramp-down factor.
#define TI_TUNE_OL_CL_RAMP_FACTOR_SCALE (0.5f)

//! @}
//*****************************************************************************
//
//! \anchor TITUNE_TYPEDEFS
//! \name TI Tune Type Definitions
//! @{
//
//*****************************************************************************

//! \brief Enumeration for the TiTune state machine
typedef enum
{
    TiTune_STATE_IDLE,              //!< Idle state - waiting for start
    TiTune_STATE_LDQ_SETUP,         //!< LDQ estimation setup state
    TiTune_STATE_LDQ_EST,           //!< D-axis and Q-axis inductance estimation (Ld/Lq)
    TiTune_STATE_LDQ_DONE,          //!< LDQ estimation complete
    TiTune_STATE_RS_SETUP,          //!< Stator resistance estimation setup
    TiTune_STATE_RS_EST,            //!< Stator resistance estimation (Rs)
    TiTune_STATE_RS_DONE,           //!< Rs estimation complete
    TiTune_STATE_OL_SETUP,          //!< Open-loop startup setup state
    TiTune_STATE_OL_RUN,            //!< Open-loop startup running state
    TiTune_STATE_CL_TORQUE_SETTLE,  //!< Closed-loop torque control settling state
    TiTune_STATE_INERTIA_SETUP,     //!< Motor inertia estimation setup
    TiTune_STATE_INERTIA_EST,       //!< Motor inertia estimation (J)
    TiTune_STATE_INERTIA_DONE,      //!< Inertia estimation complete
    TiTune_STATE_DONE,              //!< All estimations complete
    TiTune_STATE_FAULT,             //!< Fault state - observer failed to stabilize
    TiTune_NUMSTATES                //!< Number of TiTune states
} TiTune_State_e;
//! @}

//*****************************************************************************
//
//! \anchor TITUNE_RESULTS
//! \brief Consolidated motor identification results
//!
//! \details Populated once when TiTune reaches TiTune_STATE_DONE.
//!          All values are in SI physical units for direct use by the
//!          application, GUI, or debugger watch window.
//
//*****************************************************************************
typedef struct _TiTune_Results_
{
    float32_t Rs_Ohm;   //!< Stator resistance (Ohm)
    float32_t Ld_H;     //!< D-axis inductance (H)
    float32_t Lq_H;     //!< Q-axis inductance (H)
    float32_t Ke_VpHz;  //!< Back-EMF constant (V/Hz) -- lambda_PM * 2*pi
    float32_t J_Kgm2;   //!< Rotor inertia (kg*m^2) -- tau * (3/2) * P^2 * lambda_PM / G_DC
    float32_t Id_Kp;    //!< D-axis current PI proportional gain
    float32_t Id_Ki;    //!< D-axis current PI integral gain
    float32_t Iq_Kp;    //!< Q-axis current PI proportional gain
    float32_t Iq_Ki;    //!< Q-axis current PI integral gain
    float32_t Spd_Kp;   //!< Speed PI proportional gain
    float32_t Spd_Ki;   //!< Speed PI integral gain
} TiTune_Results_t;

//*****************************************************************************
//
//! \brief Application-supplied inputs -- written by the ISR each PWM cycle
//!
//! \details All fields are set by the application before calling
//!          TiTune_Run_stateMachine() and TiTune_Run_Current_Control().
//!          Grouping them here makes the interface contract explicit and
//!          simplifies GUI/debugger monitoring of live signal values.
//
//*****************************************************************************
typedef struct _TiTune_Config_
{
    float32_t          VdcBus_V;            //!< DC bus voltage in Volts
    volatile float32_t FOC_Angle_rad;       //!< Rotor angle used for Park/iPark transforms (rad)
    volatile float32_t Observer_Angle_rad;  //!< Observer estimated angle (rad) --
                                            //!< read-back from ESMO/FAST
    float32_t          Speed_Hz;            //!< Estimated electrical speed in Hz
    MATH_Vec2          Iab_A;               //!< Alpha/beta phase currents in Amperes
} TiTune_Config_t;

/* TiTune_OL_t has been replaced by TiTune_OL_Obj (library module).
 * The open-loop startup state is now fully encapsulated in the TiTune_OL
 * library object. Access via TiTune_OL_* API and getters only. */

//*****************************************************************************
//
//! \brief Closed-loop torque-settle state
//!
//! \details All variables used exclusively during the CL_TORQUE_SETTLE state.
//!          Iq is held constant at the OL handoff value throughout.
//!          Id is ramped from the OL injection level to zero over
//!          TI_TUNE_CL_ID_RAMP_TIME_SEC.
//
//*****************************************************************************
typedef struct _TiTune_CL_
{
    float32_t IqRef_A;       //!< Iq reference during CL torque settle (A); held constant
                             //!< at the value inherited from OL handoff
    float32_t IdRef_A;       //!< Id reference during CL torque settle (A); ramped from
                             //!< OL injection level to zero over TI_TUNE_CL_ID_RAMP_TIME_SEC
    float32_t IdRampDown_A;  //!< Id ramp-down step per ISR sample (A/sample); computed as
                             //!< OL.IdRef_A / (TI_TUNE_CL_ID_RAMP_TIME_SEC / USER_M1_CTRL_PERIOD_sec)
    uint32_t  SettleCount;   //!< Elapsed settle time counter (samples)
    uint32_t  SettleTarget;  //!< Required settle time (samples); derived from
                             //!< TI_TUNE_CL_SETTLE_TIME_SEC / USER_M1_CTRL_PERIOD_sec
} TiTune_CL_t;

//*****************************************************************************
//
//! \brief Working / intermediate variables
//!
//! \details Internal computation state that is neither a user-supplied input
//!          nor a final result.  Includes transform intermediates, the state
//!          machine counter, and all output signals produced by TiTune for
//!          consumption by the application (Vab, speed reference).
//
//*****************************************************************************
typedef struct _TiTune_Status_
{
    MATH_Vec2               phasor;                //!< cos/sin phasor for Park / iPark transforms
    float32_t               Rs_IPD_Angle;          //!< Initial position detection angle from LDQ (rad)
    MATH_Vec2               Out_Vdq_V;             //!< D/Q voltage output from current controllers (V)
    MATH_Vec2               Idq_A;                 //!< D/Q currents after Park transform (A)
    MATH_Vec2               IdqRef_A;              //!< D/Q current references fed to PI controllers (A)
    MATH_Vec2               Idq_out_A;             //!< D/Q current output (A)
    MATH_Vec2               Out_Vab_V;             //!< Alpha/beta voltage output from iPark (V)
    float32_t               Speed_out_Hz;          //!< Speed reference output for ESMO/observer (Hz)
    volatile TiTune_State_e state;                 //!< Current state machine state
    uint32_t                count;                 //!< General-purpose state machine counter
    bool                    faultObserverTimeout;  //!< Observer failed to stabilize within timeout
} TiTune_Status_t;

//*****************************************************************************
//
//! \brief Defines the TiTune object structure
//!
//! \details This structure contains all variables and sub-objects needed for
//!          motor parameter identification.  Fields are grouped into four
//!          logical sub-structures:
//!
//!          - \c Config  : application-supplied inputs (written each ISR)
//!          - \c OL      : open-loop startup state
//!          - \c CL      : closed-loop torque-settle state
//!          - \c Status  : working variables, transform intermediates, outputs
//!
//!          PI controllers, estimation sub-module objects, identified results,
//!          and motor base values remain at the top level.
//
//*****************************************************************************
typedef struct _TiTune_Obj_
{
    /* Enable flag */
    bool TiTune_enable;  //!< TI Tune enable flag

    TiTune_Results_t Results;  //!< All identified motor parameters in one place
    TiTune_Config_t  Config;   //!< Application-supplied inputs (written each ISR by
                               //!< the application)
    TiTune_Status_t  Status;   //!< Working variables, transform intermediates, and outputs

    /* Estimation Module Objects */

    TiTune_Rs_Obj    TiTune_Rs;          //!< Stator resistance estimation object
    TiTune_Rs_Handle TiTune_Rs_handler;  //!< Rs estimation handle

    TiTune_LDQ_Obj    TiTune_LDQ;          //!< LDQ inductance estimation object
    TiTune_LDQ_Handle TiTune_LDQ_handler;  //!< LDQ estimation handle

    TiTune_J_Coast_Obj    TiTune_J_Coast;          //!< Inertia estimation object (coast-down)
    TiTune_J_Coast_Handle TiTune_J_Coast_handler;  //!< Inertia estimation handle (coast-down)

    /* Motor base values -- RAM copy initialised from motorParams_M1 by
     * TiTune_setParams(). Can be overwritten via GUI/debugger for live tuning
     * without touching flash. Re-call TiTune_setParams() to restore defaults from
     * the const motor table. */
    float32_t RatedCurrent_A;  //!< Motor rated current (A) -- per-unit current
                               //!< base; writable via GUI
    float32_t RatedSpeed_Hz;   //!< Motor rated electrical speed (Hz) -- per-unit
                               //!< speed base; writable via GUI

    /* PI Controller Objects */
    PI_Obj TiTune_PI_Speed;  //!< Speed PI controller object
    PI_Obj TiTune_PI_Id;     //!< D-axis current PI controller object
    PI_Obj TiTune_PI_Iq;     //!< Q-axis current PI controller object

    TiTune_OL_Obj    TiTune_OL;          //!< Open-loop startup object (library module)
    TiTune_OL_Handle TiTune_OL_handler;  //!< Open-loop startup handle

    TiTune_CL_t CL;  //!< Closed-loop torque-settle state variables

} TiTune_Obj;

//*****************************************************************************
//
//! \brief Defines the TiTune handle
//
//*****************************************************************************
typedef struct _TiTune_Obj_ *TiTune_Handle;

//*****************************************************************************
//
//! \brief Global TI Tune object declarations
//! @{
//
//*****************************************************************************
extern TiTune_Obj    TiTune_M1;       //!< TI Tune object for motor 1
extern TiTune_Handle TiTune_handler;  //!< TI Tune handle
//! @}

//*****************************************************************************
//
//! \anchor TITUNE_FUNCTIONS
//! \name TI Tune Function Prototypes
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
//! \brief Initializes the TiTune module
//!
//! \param[in] pMemory   Pointer to memory for the TiTune object
//! \param[in] numBytes  Number of bytes allocated for the TiTune object
//!
//! \return The TiTune object handle
//
//*****************************************************************************
extern TiTune_Handle TiTune_init(void *pMemory, const size_t numBytes);

//*****************************************************************************
//
//! \brief     Sets the TiTune parameters
//!
//! \param[in] handle        The TiTune handle
//!
//*****************************************************************************
extern void TiTune_setParams(TiTune_Handle handle);

//*****************************************************************************
//
//! \brief     State machine handler function for TI Tune Routines
//!
//! \param[in] handle        The TiTune handle
//!
//*****************************************************************************
extern void TiTune_Run_stateMachine(TiTune_Handle handle);

//*****************************************************************************
//
//! \brief     Current Controller functions for TI Tune Routines
//!
//! \param[in] handle        The TiTune handle
//!
//*****************************************************************************
extern void TiTune_Run_Current_Control(TiTune_Handle handle);

//*****************************************************************************
//
//! \brief     Speed Controller functions for TI Tune Routines
//!
//! \param[in] handle        The TiTune handle
//!
//*****************************************************************************
//*****************************************************************************
//
//! \brief     Set parameters for Inductance Estimation
//!
//! \param[in] handle        The TiTune handle
//!
//*****************************************************************************
extern void TiTune_LDQ_setParams(TiTune_LDQ_Handle handle);
//*****************************************************************************
//
//! \brief     Set parameters for Resistance Estimation
//!
//! \param[in] handle        The TiTune handle
//!
//*****************************************************************************
extern void TiTune_Rs_setParams(TiTune_Rs_Handle handle);
//! @}

#ifdef __cplusplus
}
#endif

#endif /* LIBRARIES_TITUNE_H_ */

/** @} */
