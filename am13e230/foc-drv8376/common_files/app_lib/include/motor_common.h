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
 *
 */

/**
 * \file motor_common.h
 * \brief Motor control common definitions and data structures
 *
 * \defgroup MOTOR_COMMON_MODULE Motor Common Module
 * \brief Common motor control definitions, types, and structures
 * @{
 */

#ifndef _MOTOR_COMMON_H_
#define _MOTOR_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

// Platform types for boolean and standard types (TRUE/FALSE definitions)
#include "Platform_Types.h"

#include "dl_common.h"

/*
 * Motor Control RAM Function Placement Macro
 *
 * TI ARM Clang / GCC: uses RAMFUNC from dl_common.h which places functions in
 * the .TI.ramfunc section (RAM_C) via BINIT copy at startup.
 *
 * IAR: expands to nothing (flash execution). IAR's __ramfunc enforces a
 * transitive call restriction that is impractical for the motor control ISR
 * call chain (which calls many static inline HAL/FOC functions). RAM execution
 * for IAR requires adding .TI.ramfunc to the IAR .icf linker script and using
 * _Pragma("location=\".TI.ramfunc\"") - not enabled by default.
 *
 * Usage: MOTOR_CTRL_RAMFUNC void myFunction(void) { ... }
 */
#if defined(__IAR_SYSTEMS_ICC__)
#define MOTOR_CTRL_RAMFUNC
#else
#define MOTOR_CTRL_RAMFUNC RAMFUNC
#endif

// ARM M33 math library with TMU support
// Provides __sin, __cos, __sqrt macros mapped to TMU-accelerated functions

// math_types.h internally includes <math.h> and fastrts_tmu.h for TMU acceleration
#include "math_types.h"
#include "motor.h"
#include "userParams.h"
#include "est.h"
#include "dclink_ss.h"
#include "clarke.h"
#include "filter_fo.h"
#include "ipark.h"
#include "park.h"
#include "pi.h"
#include "svgen.h"
#include "svgen_current.h"
#include "traj.h"
#include "fwc.h"
#include "mtpa.h"
#include "ti_tune.h"
#include "vsf.h"
#include "vs_freq.h"
#include "angle_gen.h"
#include "volt_recons.h"
#include "dead_time_comp.h"
#include "ipd.h"
#include "esmo.h"
#include "speedfr.h"
#include "hal_foc_interface.h"
#include "hal.h"
#include "hal_sensor_interface.h"
#include "app_sensor_interface.h"
#include "system_config.h"
#include "flux_obs.h"

//*****************************************************************************
//
//! \anchor MOTOR_FAULT_BITS
//! \name Motor Fault Bit Definitions
//! \brief Bit definitions for motor fault detection and protection
//! @{
//
//*****************************************************************************
#define M_OVER_VOLTAGE_BIT       0x0001U  //!< DC Bus Over Voltage Fault
#define M_UNDER_VOLTAGE_BIT      0x0002U  //!< DC Bus Under Voltage Fault
#define M_MOTOR_OVER_TEMPER_BIT  0x0004U  //!< Motor over temperature Fault
#define M_MODULE_OVER_TEMPER_BIT 0x0008U  //!< Module over temperature Fault

#define M_MODULE_OVER_CURRENT_BIT 0x0010U  //!< Hardware Over Current Fault
#define M_OVER_PEAK_CURRENT_BIT   0x0020U  //!< Internal CMPSS Over Current Fault
#define M_MOTOR_OVER_LOAD_BIT     0x0040U  //!< Over Load Error
#define M_MOTOR_LOST_PHASE_BIT    0x0080U  //!< Motor Lost Phase

#define M_CURRENT_UNBALANCE_BIT 0x0100U  //!< Motor Phase Current Unbalance
#define M_MOTOR_STALL_BIT       0x0200U  //!< Motor Stall
#define M_STARTUP_FAILE_BIT     0x0400U  //!< Startup failed
#define M_MOTOR_OVER_SPEED_BIT  0x0800U  //!< Motor Over Speed

#define M_RESERVE_12_BIT     0x1000U  //!< Reserved for further use
#define M_RESERVE_13_BIT     0x2000U  //!< Reserved for further use
#define M_CURRENT_OFFSET_BIT 0x4000U  //!< Current offsets error
#define M_VOLTAGE_OFFSET_BIT 0x8000U  //!< Voltage offsets error

#define M_MASK_ALL_FAULT_BITS   0x0000U  //!< Mask to disable all fault bits
#define M_ENABLE_ALL_FAULT_BITS 0xFFFFU  //!< Mask to enable all fault bits
//! @}

//*****************************************************************************
//
//! \anchor MOTOR_FAULT_MASKS
//! \name Motor Fault Mask Definitions
//! \brief Predefined fault mask combinations for different protection levels
//! @{
//
//*****************************************************************************

//! \brief Over voltage brake fault mask
#define MTR_FAULT_OV_BRAKE M_OVER_VOLTAGE_BIT

//! \brief Enable over current faults only
#define MTR_FAULT_ENABLE_OC   \
    M_MODULE_OVER_CURRENT_BIT \
    +M_OVER_PEAK_CURRENT_BIT

//! \brief Enable over current, over/under voltage, and offset faults
#define MTR_FAULT_ENABLE_OC_OUV                                                                         \
    M_OVER_VOLTAGE_BIT                                                                                  \
    +M_UNDER_VOLTAGE_BIT + M_MODULE_OVER_CURRENT_BIT + M_OVER_PEAK_CURRENT_BIT + M_CURRENT_OFFSET_BIT + \
        M_VOLTAGE_OFFSET_BIT

//! \brief Enable all fault protection
#define MTR_FAULT_ENABLE_ALL                                                                                 \
    M_OVER_VOLTAGE_BIT                                                                                       \
    +M_UNDER_VOLTAGE_BIT + M_MOTOR_OVER_TEMPER_BIT + M_MODULE_OVER_TEMPER_BIT + M_MODULE_OVER_CURRENT_BIT +  \
        M_OVER_PEAK_CURRENT_BIT + M_MOTOR_OVER_LOAD_BIT + M_MOTOR_LOST_PHASE_BIT + M_CURRENT_UNBALANCE_BIT + \
        M_MOTOR_STALL_BIT + M_STARTUP_FAILE_BIT + M_MOTOR_OVER_SPEED_BIT + M_CURRENT_OFFSET_BIT + M_VOLTAGE_OFFSET_BIT

//! \brief Clear all fault protection except over/under voltage and temperature
#define MTR_FAULT_CLEAR \
    M_OVER_VOLTAGE_BIT  \
    +M_UNDER_VOLTAGE_BIT + M_MOTOR_OVER_TEMPER_BIT + M_MODULE_OVER_TEMPER_BIT

//! \brief Disable all fault protection
#define MTR_FAULT_DISABLE_ALL 0x0000U

//! \brief Default fault mask for motor 1
#define MTR1_FAULT_MASK_SET MTR_FAULT_ENABLE_OC_OUV
//! @}

//*****************************************************************************
//
//! \anchor MOTOR_ESTIMATOR_ENUMS
//! \name Motor Estimator Enumerations
//! @{
//
//*****************************************************************************

//! \brief Enumeration for the estimator mode
typedef enum
{
    ESTIMATOR_MODE_FAST     = 0,  //!< FAST estimator mode
    ESTIMATOR_MODE_ESMO     = 1,  //!< ESMO (Enhanced Sliding Mode Observer) mode
    ESTIMATOR_MODE_FLUX_OBS = 2   //!< Flux Observer mode
} ESTIMATOR_Mode_e;

//! \brief Enumeration for the estimator algorithm type
typedef enum
{
    EST_TYPE_FAST      = 0,  //!< FAST estimator only
    EST_TYPE_ESMO      = 1,  //!< ESMO estimator only
    EST_TYPE_ENC       = 2,  //!< Encoder only
    EST_TYPE_HALL      = 3,  //!< Hall sensor only
    EST_TYPE_FAST_ENC  = 4,  //!< FAST with Encoder
    EST_TYPE_FAST_ESMO = 5,  //!< FAST with ESMO
    EST_TYPE_FAST_HALL = 6,  //!< FAST with Hall sensor
    EST_TYPE_ESMO_ENC  = 7,  //!< ESMO with Encoder
    EST_TYPE_ISBLDC    = 8   //!< ISBLDC estimator only
} EST_Type_e;

//! \brief Enumeration for the FAST library type and execution location
typedef enum
{
    FAST_TYPE_SOFTLIB_FLASH = 0,  //!< FAST software library running from Flash
    FAST_TYPE_ROMLIB_FLASH  = 1,  //!< FAST ROM library running from Flash
    FAST_TYPE_ROMLIB_RAM    = 2,  //!< FAST ROM library running from RAM
    FAST_TYPE_NONFAST_FLASH = 3,  //!< Non-FAST algorithm running from Flash
    FAST_TYPE_NONFAST_RAM   = 4   //!< Non-FAST algorithm running from RAM
} FASTLIB_Type_e;

//! \brief Enumeration for svpwm mode
//!
typedef enum
{
    SVM_COM_C = 0,  //!< SVPWM common SVM mode
    SVM_MIN_C = 1,  //!< DPWM minimum SVM mode
    SVM_MAX_C = 2,  //!< DPWM maximum SVM
    SVM_DQ_S  = 3   //!< SVPWM standard DQ mode
} SVM_Mode_e;

//! \brief Enumeration for motor parameter identification method
typedef enum
{
    MOTOR_PARAM_IDENTIFY_INSTASPIN = 0,  //!< InstaSPIN automatic parameter identification
    MOTOR_PARAM_IDENTIFY_TITUNE    = 1   //!< TI Tune guided parameter identification
} MotorParamIdentify_Method_e;

//! @}

//*****************************************************************************
//
//! \anchor MOTOR_FAULT_STRUCTS
//! \name Motor Fault Structures
//! @{
//
//*****************************************************************************

//! \brief Bit field structure for motor fault flags
typedef struct _FAULT_MTR_BITS_
{
    uint16_t overVoltage : 1;     //!< Bit 0: DC Bus Over Voltage Fault
    uint16_t underVoltage : 1;    //!< Bit 1: DC Bus Under Voltage Fault
    uint16_t motorOverTemp : 1;   //!< Bit 2: Motor over temperature Fault
    uint16_t moduleOverTemp : 1;  //!< Bit 3: Power module over temperature Fault

    uint16_t moduleOverCurrent : 1;  //!< Bit 4: Hardware Over Current Fault Flag
    uint16_t overPeakCurrent : 1;    //!< Bit 5: Internal CMPSS Over Current Fault Flag
    uint16_t overLoad : 1;           //!< Bit 6: Over Load Error
    uint16_t motorLostPhase : 1;     //!< Bit 7: Motor Lost Phase

    uint16_t currentUnbalance : 1;  //!< Bit 8: Motor Phase Current Imbalance
    uint16_t motorStall : 1;        //!< Bit 9: Motor Stall
    uint16_t startupFailed : 1;     //!< Bit 10: Startup Failed
    uint16_t overSpeed : 1;         //!< Bit 11: Motor Over Speed

    uint16_t estimError : 1;       //!< Bit 12: Error in Estimator
    uint16_t gateDriverFault : 1;  //!< Bit 13: Gate Driver Fault
    uint16_t currentOffset : 1;    //!< Bit 14: Current Offset Check Error
    uint16_t voltageOffset : 1;    //!< Bit 15: Voltage Offset Check Error
} FAULT_MTR_BITS;

//! \brief Union for motor fault register access
typedef union _FAULT_MTR_REG_t
{
    uint16_t       all;  //!< Access all fault bits as a single word
    FAULT_MTR_BITS bit;  //!< Access individual fault bits
} FAULT_MTR_REG_t;
//! @}

//*****************************************************************************
//
//! \anchor MOTOR_SETVARS
//! \name Motor Set Variables Structure
//! \brief Configuration parameters for motor control
//! @{
//
//*****************************************************************************

//! \brief Motor configuration and setpoint variables structure
typedef struct _MOTOR_SetVars_t_
{
    //! \name Fault Timing Configuration
    //! \brief ISR-count thresholds for fault timers (set at init from user_mtr1.h macros).
    //! @{
    uint16_t overCurrentTimesSet;  //!< Over current fault trigger count threshold
    uint16_t voltageFaultTimeSet;  //!< Voltage fault time threshold in ISR counts
    uint16_t motorStallTimeSet;    //!< Motor stall detection time threshold in ISR counts
    uint16_t startupFailTimeSet;   //!< Startup failure time threshold in ISR counts
    uint16_t overSpeedTimeSet;     //!< Over speed fault time threshold in ISR counts
    uint16_t overLoadTimeSet;      //!< Over load fault time threshold in ISR counts
    uint16_t unbalanceTimeSet;     //!< Current unbalance time threshold in ISR counts
    uint16_t lostPhaseTimeSet;     //!< Lost phase detection time threshold in ISR counts
    uint16_t RsOnlineWaitTimeSet;  //!< Rs online calibration wait time in ISR counts
    uint16_t RsOnlineWorkTimeSet;  //!< Rs online calibration work time in ISR counts
    uint16_t stopWaitTimeSet;      //!< Stop wait time before restart in ISR counts
    uint16_t restartWaitTimeSet;   //!< Restart wait time in ISR counts
    uint16_t restartTimesSet;      //!< Maximum restart attempts allowed
    uint16_t bootChargeTimeSet;    //!< Boot charge time in ISR counts
    //! @}

    //! \name Control State Timing (ISR Counts)
    //! \brief Pre-computed at init from user_mtr1.h time macros x ctrlFreq_Hz.
    //!        Read-only during operation; edit the SEC macros in user_mtr1.h to retune.
    //! @{
    uint32_t flyingStartTimeDelay_cnt;  //!< = FLYING_START_TIME_DELAY_SEC x ctrlFreq_Hz
    uint32_t alignTimeDelay_cnt;        //!< = ALIGN_TIME_DELAY_SEC x ctrlFreq_Hz
    uint32_t forceRunTimeDelay_cnt;     //!< = FORCE_RUN_TIME_DELAY_SEC x ctrlFreq_Hz
    uint32_t fwcTimeDelay_cnt;          //!< = FWC_TIME_DELAY_SEC x ctrlFreq_Hz
    uint32_t offsetCalibTimeSet_cnt;    //!< = USER_M1_OFFSET_CALIB_TIME_SEC x ctrlFreq_Hz
    uint16_t ipdCycleCountMax;          //!< = IPD_CYCLE_COUNT_MAX
    //! @}

    //! \name Motor Physical Parameters
    //! \brief Nameplate and measured electrical parameters sourced from user_mtr1.h.
    //! @{
    float32_t Rr_Ohm;      //!< Rotor resistance in Ohms
    float32_t Rs_Ohm;      //!< Stator resistance in Ohms
    float32_t Ls_d_H;      //!< D-axis stator inductance in Henries
    float32_t Ls_q_H;      //!< Q-axis stator inductance in Henries
    float32_t flux_VpHz;   //!< Motor flux linkage in Volts per Hz
    float32_t flux_Wb;     //!< Motor flux linkage in Webers
    float32_t RoverL_rps;  //!< R/L ratio in radians per second

    float32_t RsOnLine_Ohm;  //!< Online measured stator resistance in Ohms

    float32_t Ls_d_comp_H;  //!< D-axis inductance compensation in Henries
    float32_t Ls_q_comp_H;  //!< Q-axis inductance compensation in Henries

    float32_t Ls_min_H;  //!< Minimum inductance value in Henries

    float32_t Ls_d_Icomp_coef;  //!< D-axis inductance current compensation coefficient
    float32_t Ls_q_Icomp_coef;  //!< Q-axis inductance current compensation coefficient
    //! @}

    //! \name PI Controller Tuning
    //! \brief Proportional and integral gains for all closed-loop controllers.
    //! @{
    float32_t Kp_spd;  //!< Speed controller proportional gain
    float32_t Ki_spd;  //!< Speed controller integral gain

    float32_t Kp_Id;  //!< D-axis current controller proportional gain
    float32_t Ki_Id;  //!< D-axis current controller integral gain

    float32_t Kp_Iq;  //!< Q-axis current controller proportional gain
    float32_t Ki_Iq;  //!< Q-axis current controller integral gain

    float32_t Kp_fwc;           //!< Field weakening controller proportional gain
    float32_t Ki_fwc;           //!< Field weakening controller integral gain
    float32_t angleFWCMax_rad;  //!< Maximum field weakening angle in radians
    //! @}

    //! \name Operating Reference Values
    //! @{
    float32_t overModulation;     //!< Over modulation factor
    float32_t RsOnLineCurrent_A;  //!< Rs online calibration current in Amperes
    float32_t magneticCurrent_A;  //!< Magnetizing current in Amperes
    //! @}

    //! \name Speed and Current Operating Limits
    //! \brief Motor operating parameters sourced from user_mtr1.h.
    //! @{
    float32_t speedFlyingStart_Hz;     //!< Min speed for flying-start detection (Hz)
    float32_t olSpeedThr;              //!< Open-loop to closed-loop transition speed (Hz)
    float32_t accelerationMax_Hzps;    //!< Closed-loop maximum acceleration (Hz/s)
    float32_t accelerationStart_Hzps;  //!< Open-loop startup acceleration (Hz/s)
    float32_t startCurrent_A;          //!< Open-loop torque current limit (A)
    float32_t alignCurrent_A;          //!< Alignment current limit (A)
    float32_t maxCurrent_A;            //!< Maximum current limit (A)
    uint8_t   hallStartupCycles;  //!< Hall startup: electrical cycles in torque mode before closing loop (3, 6, or 12)
    //! @}

    //! \name Fault Detection Thresholds
    //! @{
    float32_t lostPhaseSet_A;     //!< Lost phase current threshold in Amperes
    float32_t unbalanceRatioSet;  //!< Current unbalance ratio threshold
    float32_t overLoadSet_W;      //!< Over load power threshold in Watts

    float32_t toqueFailMinSet_Nm;  //!< Minimum torque failure threshold in Newton-meters
    float32_t speedFailMaxSet_Hz;  //!< Maximum speed failure threshold in Hz

    float32_t speedFailMinSet_Hz;  //!< Minimum speed failure threshold in Hz
    float32_t stallCurrentSet_A;   //!< Stall current threshold in Amperes
    float32_t IsFailedChekSet_A;   //!< Current failure check threshold in Amperes

    float32_t overCurrent_A;  //!< Over current threshold in Amperes
    //! @}

    //! \name Voltage Fault Thresholds
    //! @{
    float32_t overVoltageFault_V;   //!< Over voltage fault threshold in Volts
    float32_t overVoltageNorm_V;    //!< Over voltage normal threshold in Volts
    float32_t underVoltageFault_V;  //!< Under voltage fault threshold in Volts
    float32_t underVoltageNorm_V;   //!< Under voltage normal threshold in Volts
    //! @}

} MOTOR_SetVars_t;
//! @}

//! \brief Defines the MOTOR_SetVars_t handle
//!
typedef struct _MOTOR_SetVars_t_ *MOTORSETS_Handle;

//*****************************************************************************
//
//! \anchor MOTOR_INPUTCTRL
//! \name Motor Input Control Structure
//! \brief Runtime user commands — start/stop, feature enables, speed reference.
//!        User writes these fields at runtime; algorithm reads them.
//! @{
//
//*****************************************************************************

//! \brief Runtime user command and input control structure
typedef struct _MOTOR_InputCtrl_t_
{
    //! \name Motor Run / Stop Commands
    //! @{
    bool flagEnableRunAndIdentify;  //!< Start motor and trigger identification
    bool flagClearFaults;           //!< Pulse high to clear active fault register
    //! @}

    //! \name Speed Reference and Direction
    //! @{
    float32_t speedRef_Hz;  //!< Speed setpoint in Hz
    float32_t direction;    //!< Motor direction: 1.0f = forward, -1.0f = reverse
    //! @}

    //! \name Startup Mode Selects
    //! @{
    bool           flagEnableFlyingStart;  //!< Detect spinning rotor before start
    STARTUP_Mode_e startupMode;            //!< Startup mode: STARTUP_ALIGN, STARTUP_IPD, STARTUP_FREE, STARTUP_HFI
    bool           flagEnableRestart;      //!< Enable auto-restart after fault
    //! @}

    //! \name Advanced Feature Enables
    //! @{
    bool flagEnableDTC;      //!< Enable Dead Time Compensation
    bool flagEnableBraking;  //!< Activate braking
    bool flagEnableFWC;      //!< Enable field weakening control
    bool flagEnableMTPA;     //!< Enable maximum torque per ampere
    //! @}

    //! \name Identification and Online Calibration
    //! @{
    bool                        flagEnableMotorIdentify;   //!< Trigger full motor identification
    MotorParamIdentify_Method_e motorParamIdentifyMethod;  //!< Parameter identification method: INSTASPIN or TITUNE
    bool                        flagEnableRsOnLine;        //!< Enable Rs online measurement
    bool                        flagStartRsOnLine;         //!< Start Rs online measurement
    bool                        flagEnableRsRecalc;        //!< Enable Rs recalculation
    bool                        flagEnableLsUpdate;        //!< Enable Ls update
    //! @}

    //! \name Hall Sensor Calibration
    //! @{
    bool flagStartHallCal;  //!< Pulse high to start Hall sensor bidirectional calibration
    //! @}

    //! \name Debug Overrides
    //! @{
    bool flagEnableAlignment;       //!< Rotor alignment After IPD
    bool flagEnableContAlign;       //!< Continuous alignment mode
    bool flagEnableAlignDebug;      //!< Hold in alignment state for debug
    bool flagEnableOpenLoopDebug;   //!< Hold in open-loop state for debug
    bool forceOpenLoopVoltageMode;  //!< Force open-loop voltage output
    //! @}

    //! \name Operating Mode Selectors
    //! @{
    ESTIMATOR_Mode_e estimatorMode;  //!< FAST or ESMO estimator selection
    SVM_Mode_e       svmMode;        //!< Space vector modulation mode
    BRAKE_Mode_e     brakingMode;    //!< Braking strategy
    OPERATE_Mode_e   operateMode;    //!< Operating mode (speed / torque) //TODO: rename as controlMode

    //! @}

    //! \name Status //TODO: move to status variables
    //! @{
    MotorNum_e    motorNum;               //!< Motor number identifier
    bool          flagEnableSpeedCtrl;    //!< Enable closed-loop speed control
    bool          flagEnableCurrentCtrl;  //!< Enable closed-loop current control
    SAMPLE_Mode_e sampleMode;             //!< ADC sampling mode
    bool          flagBypassLockRotor;    //!< Bypass locked-rotor check
    bool          flagEnableForceAngle;   //!< Forced-angle open-loop startup
    bool          flagSetupController;    //!< Trigger controller re-setup (gains reload)
    //! @}

} MOTOR_InputCtrl_t;
//! @}

//! \brief Handle for MOTOR_InputCtrl_t
typedef struct _MOTOR_InputCtrl_t_ *MOTORINPUT_Handle;

//*****************************************************************************
//
//! \anchor MOTOR_OUTVARS
//! \name Motor Output Variables Structure
//! \brief Active tuning view — closed-loop quantities for TI Tune, DAC debug,
//!        and tuning tools. Take address of embedded member for handle access.
//! @{
//
//*****************************************************************************

//! \brief Motor output and active tuning variables structure
typedef struct _MOTOR_OutVars_t_
{
    //! \name Motor State
    //! @{
    MOTOR_Status_e  motorState;   //!< Current operating state machine state
    FAULT_MTR_REG_t faultMtrNow;  //!< Active (raw) fault register — for tuning monitor
    //! @}

    //! \name Speed
    //! @{
    float32_t speed_Hz;   //!< Controller speed in Hz (primary)
    float32_t speed_rps;  //!< Controller speed in rad/s
    //! @}

    //! \name Current
    //! @{
    float32_t Is_A;       //!< Stator current magnitude sqrt(Id^2+Iq^2) in Amperes (AC phasor)
    float32_t Idc_A;      //!< DC bus source current Da*Ia+Db*Ib+Dc*Ic in Amperes (supply current)
    MATH_Vec2 Idq_in_A;   //!< Measured d/q currents in Amperes
    MATH_Vec2 IdqRef_A;   //!< D/Q current reference in Amperes
    MATH_Vec2 Idq_out_A;  //!< D/Q current output in Amperes
    //! @}

    //! \name Voltage Commands
    //! @{
    MATH_Vec2 Vab_out_V;     //!< Alpha/beta output voltage to inverter in Volts
    MATH_Vec2 Vdq_out_V;     //!< D/Q voltage command in Volts
    MATH_Vec2 Vdq_offset_V;  //!< D/Q voltage compensation offset in Volts
    //! @}

    //! \name Angles (controller-facing)
    //! @{
    volatile float32_t angleFOC_rad;   //!< FOC angle used by controller (volatile)
    float32_t          angleFWC_rad;   //!< Field weakening angle output in radians
    float32_t          angleMTPA_rad;  //!< MTPA angle output in radians
    float32_t          mtpaKconst;     //!< MTPA K constant
    //! @}

    //! \name Power and Torque
    //! @{
    float32_t torque_Nm;      //!< Estimated torque in Newton-meters
    float32_t powerActive_W;  //!< Active power in Watts
    //! @}

    //! \name Online Estimation Results (tuning visibility)
    //! @{
    float32_t LsOnline_d_H;   //!< Online d-axis inductance in Henries
    float32_t LsOnline_q_H;   //!< Online q-axis inductance in Henries
    float32_t fluxOnline_Wb;  //!< Online flux estimate in Webers
    //! @}

    //! \name PWM
    //! @{
    float32_t sector;  //!< SVM sector number
    //! @}

} MOTOR_OutVars_t;
//! @}

//! \brief Handle for MOTOR_OutVars_t
typedef struct _MOTOR_OutVars_t_ *MOTOROUT_Handle;

//*****************************************************************************
//
//! \anchor MOTOR_STATUSVARS
//! \name Motor Status Variables Structure
//! \brief Deep debug view — all per-source signals, fault counters, RMS buffers,
//!        and system-written status flags (read-only from application).
//! @{
//
//*****************************************************************************

//! \brief Motor status and debug variables structure
typedef struct _MOTOR_StatusVars_t_
{
    //! \name System-Written Status Flags (read-only from application)
    //! @{
    bool flagRunIdentAndOnLine;  //!< Currently running identification and online mode
    bool flagMotorIdentified;    //!< Motor parameter identification complete
    bool flagMotorTiTuneRun;     //!< TI Tune identification running
    bool flagBrakeDone;          //!< Braking sequence complete
    bool flagUpdateMTPAParams;   //!< MTPA parameters are being updated
    bool flagVIrmsCal;           //!< Voltage/current RMS calibration active
    bool enableSpeedCtrl;        //!< Speed control loop currently active
    bool enableCurrentCtrl;      //!< Current control loop currently active
    bool flagRsOnLineContinue;   //!< Rs online measurement continuing
    bool flagEstStateChanged;    //!< Estimator state changed flag
    //! @}

    //! \name Internal Control State (written by algorithm, not user commands)
    //! @{
    MotorNum_e    motorNum;               //!< Motor number identifier
    bool          flagEnableSpeedCtrl;    //!< Speed control loop enable (set by init/algorithm)
    bool          flagEnableCurrentCtrl;  //!< Current control loop enable (set by init/algorithm)
    SAMPLE_Mode_e sampleMode;             //!< ADC sampling mode
    bool          flagBypassLockRotor;    //!< Bypass locked-rotor check
    bool          flagEnableForceAngle;   //!< Forced-angle open-loop startup
    bool          flagSetupController;    //!< Trigger controller re-setup (gains reload)
    //! @}

    //! \name Estimator States
    //! @{
    EST_State_e      estState;   //!< Estimator state
    EST_Traj_State_e trajState;  //!< Trajectory state
    //! @}

    //! \name Fault Detail (masked / historical)
    //! @{
    FAULT_MTR_REG_t faultMtrUse;   //!< Masked active fault register
    FAULT_MTR_REG_t faultMtrMask;  //!< Fault enable mask register
    FAULT_MTR_REG_t faultMtrPrev;  //!< Previous fault register (fault history)
    //! @}

    //! \name Fault and Restart Counters
    //! @{
    uint16_t overCurrentTimesCnt;  //!< Over current event counter
    uint16_t overVoltageTimeCnt;   //!< Over voltage time counter
    uint16_t underVoltageTimeCnt;  //!< Under voltage time counter
    uint16_t motorStallTimeCnt;    //!< Motor stall time counter
    uint16_t startupFailTimeCnt;   //!< Startup failure time counter
    uint16_t bootChargeTimeCnt;    //!< Boot charge time counter
    uint16_t stopWaitTimeCnt;      //!< Stop wait time counter
    uint16_t restartTimesCnt;      //!< Restart attempts counter
    uint16_t startSumTimesCnt;     //!< Total start attempts counter
    uint16_t overSpeedTimeCnt;     //!< Over speed time counter
    uint16_t overLoadTimeCnt;      //!< Over load time counter
    uint16_t unbalanceTimeCnt;     //!< Current unbalance time counter
    uint16_t lostPhaseTimeCnt;     //!< Lost phase time counter
    uint32_t stateRunTimeCnt;      //!< State run time counter
    uint32_t offsetCalibTimeCnt;   //!< Offset calibration ISR tick counter
    uint16_t hallStartupEdgeCnt;   //!< Hall edges counted during MOTOR_HALL_STARTUP torque-mode phase
    //! @}

    //! \name Speed — All Sources (Hz)
    //! @{
    float32_t speedAbs_Hz;     //!< Absolute speed magnitude in Hz
    float32_t speedFilter_Hz;  //!< Secondary filtered speed in Hz
    float32_t speed_int_Hz;    //!< Internal speed reference in Hz
    float32_t speedEST_Hz;     //!< Speed from FAST estimator in Hz
    // #TAG_ESMO_START#
    float32_t speedPLL_Hz;  //!< Speed from PLL in Hz
    // #TAG_ESMO_END#
    float32_t speedENC_Hz;   //!< Speed from encoder in Hz
    float32_t speedHall_Hz;  //!< Speed from Hall sensors in Hz
    float32_t speedINT_Hz;   //!< Speed from ISBLDC in Hz
    //! @}

    //! \name Angles — All Sources (rad)
    //! @{
    volatile float32_t angleObs_rad;  //!< Observer angle (volatile, primary observer output)
    float32_t          angleEST_rad;  //!< Angle from FAST estimator in radians
    float32_t          angleSMO_rad;  //!< Angle from SMO in radians
    // #TAG_ESMO_START#
    float32_t          anglePLL_rad;  //!< Angle from PLL in radians
    // #TAG_ESMO_END#
    float32_t          angleENC_rad;        //!< Angle from encoder in radians
    float32_t          angleHall_rad;       //!< Angle from Hall sensors in radians
    float32_t          angleBrake_rad;      //!< Rotor angle for braking in radians
    float32_t          angleCurrent_rad;    //!< Current angle in radians
    float32_t          angleOffsetIPD_rad;  //!< IPD angle offset in radians
    float32_t          angleDetectIPD_rad;  //!< IPD detected angle in radians
    //! @}

    //! \name Raw Measurements
    //! @{
    float32_t Vs_V;   //!< Stator voltage magnitude in Volts
    MATH_Vec2 Vab_V;  //!< Alpha/beta voltage values in Volts
    MATH_Vec2 Iab_A;  //!< Alpha/beta current values in Amperes
    //! @}

    //! \name Power Detail
    //! @{
    float32_t powerReal_W;     //!< Real power in Watts
    float32_t powerInvtOut_W;  //!< Inverter output power in Watts
    //! @}

    //! \name RMS Phase Currents
    //! @{
    float32_t Irms_A[3];       //!< RMS phase currents in Amperes (3 phases)
    float32_t unbalanceRatio;  //!< Phase current unbalance ratio
    float32_t IrmsCalSum[3];   //!< RMS calibration accumulation buffers
    float32_t IrmsPrdSum[3];   //!< RMS period accumulation buffers
    float32_t VIrmsIsrScale;   //!< V/I RMS ISR scale factor
    float32_t IrmsCalSF;       //!< Current RMS calculation scale factor
    uint16_t  VIrmsIsrSet;     //!< V/I RMS ISR setpoint
    uint16_t  VIrmsIsrCnt;     //!< V/I RMS ISR counter
    //! @}

    //! \name Online Calibration Timing
    //! @{
    uint16_t      RsOnlineTimeCnt;  //!< Rs online measurement time counter
    //! @}
    volatile bool flagOffsetCalibDone;
} MOTOR_StatusVars_t;
//! @}

//! \brief Handle for MOTOR_StatusVars_t
typedef struct _MOTOR_StatusVars_t_ *MOTORSTATUS_Handle;

//*****************************************************************************
//
//! \anchor MOTOR_VARS
//! \name Motor Variables Structure
//! \brief Runtime variables for motor control
//! @{
//
//*****************************************************************************

typedef struct _MOTOR_Vars_t_
{
    //! \name Embedded Sub-Structs (zero indirection — same cost as direct field access)
    //! \brief Access via obj->inputCtrl.xxx, obj->outVars.xxx, obj->statusVars.xxx.
    //!        For handle-based access (TI Tune, DAC, tools): take address at init.
    //! @{
    MOTOR_InputCtrl_t  inputCtrl;   //!< Runtime user commands  (EMBEDDED — no double deref)
    MOTOR_OutVars_t    outVars;     //!< Active tuning outputs  (EMBEDDED — no double deref)
    MOTOR_StatusVars_t statusVars;  //!< Debug / status view    (EMBEDDED — no double deref)
    //! @}

    //! \name Configuration and HAL Handles
    //! @{
    MOTORSETS_Handle                 motorSetsHandle;  //!< Configuration → MOTOR_SetVars_t (pointer — slow path)
    const HAL_FOC_Interface_t *const halInterface;     //!< HAL FOC interface handle
    const HAL_GD_Interface_t        *gateDriveHandle;  //!< Gate driver interface handle
    HAL_Sensor_Interface_t *halSensorInterface;        //!< Generic HAL sensor interface (NULL in sensorless projects)
    APP_Sensor_Interface_t *appSensorInterface;        //!< Generic app sensor interface (NULL in sensorless projects)
    //! @}

    //! \name Internal Control State (not for application access)
    //! \brief Intermediate quantities computed inside the ISR and monitor loop.
    //!        Do not read or modify these from application code.
    //! @{
    // #TAG_FIELD_WEAKENING_START#
    float32_t VsRef_pu;  //!< Voltage reference in per-unit
    float32_t VsRef_V;   //!< Voltage reference in Volts
    // #TAG_FIELD_WEAKENING_END#
    float32_t oneOverDcBus_invV;  //!< DC Bus inverse (1/V)

    float32_t IdRated_A;  //!< Rated d-axis current in Amperes
    float32_t IsRef_A;    //!< Stator current reference in Amperes

    MATH_Vec2 phasor;           //!< Sensorless FOC phasor
    float32_t frswPos_sf;       //!< Frequency switch position scale factor
    float32_t angleDelayed_sf;  //!< Delayed angle scale factor

    MATH_Vec2 Vdq_OpenLoop;  //!< Output voltage for forced open loop mode

    float32_t angleAlign_rad;    //!< Rotor angle compensation during OL transition in radians
    float32_t angleOLStep_rad;   //!< Open loop angle step in radians
    float32_t angleOLDelta_rad;  //!< Open loop angle delta in radians

    float32_t iqTarget_OLEnd_A;  //!< Target Iq at end of OL transition
    float32_t angleDelta_rad;    //!< Rotor angle compensation value in radians
    float32_t angleComp_rad;     //!< Rotor angle delay compensation in radians
    float32_t angleGen_rad;      //!< Rotor angle from generator modules in radians
    // #TAG_ESMO_START#
    float32_t anglePLLComp_rad;  //!< PLL angle compensation in radians
    // #TAG_ESMO_END#

    uint16_t counterSpeed;      //!< Speed loop execution counter
    uint16_t counterTrajSpeed;  //!< Trajectory speed counter

    uint32_t  ISRCount;   //!< ISR execution counter
    float32_t power_sf;   //!< Power scale factor
    float32_t debugData;  //!< Debug data variable
    //! @}

    //! \name Algorithm Objects
    //! \brief Embedded algorithm objects and optional feature handles.
    //!        TAG markers are preserved for code-generation tools.
    //! @{

    userParams_Handle userParamsHandle;  //!< User parameters handle
    // #TAG_FAST_OBS_START#
    EST_Handle        estHandle;  //!< Estimator handle (external library)
    // #TAG_FAST_OBS_END#

    IPARK_Obj ipark_V;  //!< Inverse Park transform object
    PARK_Obj  park_I;   //!< Current Park transform object
    PARK_Obj  park_V;   //!< Voltage Park transform object

    PI_Obj pi_spd;  //!< Speed PI controller object
    PI_Obj pi_Id;   //!< D-axis current PI controller object
    PI_Obj pi_Iq;   //!< Q-axis current PI controller object
    // #TAG_FIELD_WEAKENING_START#
    PI_Obj pi_fwc;  //!< Field weakening PI controller object
    // #TAG_FIELD_WEAKENING_END#

    TRAJ_Obj      traj_spd;  //!< Speed reference trajectory object
    ANGLE_GEN_Obj angleGen;  //!< Angle generator for open loop control

    SPDFR_Obj spdfr;  //!< Speed frequency calculator object

    // #TAG_MTPA_START#
    MTPA_Handle mtpaHandle;  //!< Maximum Torque Per Ampere (MTPA) handle
    // #TAG_MTPA_END#

    // #TAG_DEAD_TIME_COMP_START#
    DTC_Handle deadTimeCompHandle;  //!< Dead time compensation handle
    // #TAG_DEAD_TIME_COMP_END#

    // #TAG_READ_INV_VOLT_START#
    VOLREC_Handle volrecHandle;  //!< Voltage reconstruct handle
    // #TAG_READ_INV_VOLT_END#

    // #TAG_IPD_START#
    IPD_Handle ipdHandle;  //!< Initial Position Detection (IPD) handle
    // #TAG_IPD_END#

    // #TAG_ESMO_START#
    ESMO_Handle esmoHandle;  //!< Enhanced Sliding Mode Observer (ESMO) handle
    // #TAG_ESMO_END#

    // #TAG_FLUX_OBS_START#
    FLUX_OBS_Handle fluxObsHandle;  //!< Flux Observer handle
    // #TAG_FLUX_OBS_END#

    EST_InputData_t  estInputData;   //!< Estimator input data structure
    EST_OutputData_t estOutputData;  //!< Estimator output data structure

    HAL_FOC_Data_Obj *halFOCData;  //!< Pointer to HAL FOC data object
    HAL_ADCData_t    *adcData;     //!< Pointer to ADC data structure
    HAL_PWMData_t    *pwmData;     //!< Pointer to PWM data structure
    MATH_Vec3        *pwmVabc_pu;  //!< Pointer to PWM ABC values in per-unit

    // #TAG_TI_TUNE_START#
    struct _TiTune_Obj_ *tiTuneHandle;  //!< TI Tune handle (forward-declared)
    // #TAG_TI_TUNE_END#

    //! @}

} MOTOR_Vars_t;
//! @}
/**
 * \brief Handle (pointer) for the motor control variables structure.
 */
typedef struct _MOTOR_Vars_t_ *MOTOR_Handle;
//*****************************************************************************
//
//! \anchor MOTOR_FUNCTIONS
//! \name Motor Control Functions
//! \brief Function prototypes for motor control operations
//! @{
//
//*****************************************************************************

//! \brief Checks motor faults and updates fault registers
//! \param[in] handle The motor control handle
extern void checkMotorFaults(MOTOR_Handle handle);

/*
 * Updates the PI controller gains from motor settings.
 * This function updates Id, Iq, and speed controller gains when
 * the motor is running and identified.
 */
static inline void updateControllers(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if ((obj->outVars.motorState == MOTOR_CTRL_RUN) && (obj->statusVars.flagMotorIdentified == true))
    {
        /* Update the Id controller */
        PI_setGains(&obj->pi_Id, objSets->Kp_Id, objSets->Ki_Id);

        /* Update the Iq controller */
        PI_setGains(&obj->pi_Iq, objSets->Kp_Iq, objSets->Ki_Iq);

        /* Update the speed controller */
        PI_setGains(&obj->pi_spd, objSets->Kp_spd, objSets->Ki_spd);
    }
}

/*
 * Gets the current PI controller parameters and stores them in motor settings.
 * This function retrieves Kp and Ki values from Id, Iq, and speed controllers.
 */
static inline void getControllers(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    /* Get the Id controller parameters */
    objSets->Kp_Id = PI_getKp(&obj->pi_Id);
    objSets->Ki_Id = PI_getKi(&obj->pi_Id);

    /* Get the Iq controller parameters */
    objSets->Kp_Iq = PI_getKp(&obj->pi_Iq);
    objSets->Ki_Iq = PI_getKi(&obj->pi_Iq);

    /* Get the speed controller parameters */
    objSets->Kp_spd = PI_getKp(&obj->pi_spd);
    objSets->Ki_spd = PI_getKi(&obj->pi_spd);
}

//! \brief Sets up control parameters for stopping motor
//! \param[in] handle The motor control handle
extern void stopMotorControl(MOTOR_Handle handle);

//! \brief Sets up control parameters for restarting motor
//! \param[in] handle The motor control handle
extern void restartMotorControl(MOTOR_Handle handle);

//! \brief Resets motor control parameters for restarting motor
//! \param[in] handle The motor control handle
extern void resetMotorControl(MOTOR_Handle handle);

//! \brief Sets up the current controllers (Id and Iq)
//! \param[in] handle The motor control handle
extern void setupCurrentControllers(MOTOR_Handle handle);

//! \brief Sets up all motor controllers (speed, Id, Iq, FWC)
//! \param[in] handle The motor control handle
extern void setupControllers(MOTOR_Handle handle);

//! \brief Collects current and voltage data for RMS calculation
//! \param[in] handle The motor control handle
extern void collectRMSData(MOTOR_Handle handle);

//! \brief Calculates RMS values from collected data
//! \param[in] handle The motor control handle
extern void calculateRMSData(MOTOR_Handle handle);

//! \brief Runs motor monitoring in main loop timer
//! \param[in] handle The motor control handle
extern void runMotorMonitor(MOTOR_Handle handle);

//! \brief Performs Rs online calibration
//! \param[in] handle The motor control handle
extern void runRsOnLine(MOTOR_Handle handle);

//! \brief Updates the global motor variables from estimator
//! \param[in] handle The motor control handle
extern void updateGlobalVariables(MOTOR_Handle handle);

//! \brief Updates the Field Weakening Control parameters
//! \param[in] handle The motor control handle
extern void updateFWCParams(MOTOR_Handle handle);

//! \brief Updates the Maximum Torque Per Ampere parameters
//! \param[in] handle The motor control handle
extern void updateMTPAParams(MOTOR_Handle handle);

//! \brief Initializes motor control system
//!
//! This function initializes interrupts, PWM, gate driver, parameters,
//! and performs offset calibration.
//!
//! \param[in] motorHandle The motor control handle
//! \param[in] isrFunc Pointer to the motor's interrupt service routine
void initMotorControlSystem(MOTOR_Handle motorHandle);

//! \brief Monitors motor for over load condition
//! \param[in] motorHandle The motor control handle
extern void monitorOverLoad(MOTOR_Handle motorHandle);

//! \brief Monitors motor for stall condition
//! \param[in] motorHandle The motor control handle
extern void monitorMotorStall(MOTOR_Handle motorHandle);

//! \brief Monitors motor for lost phase condition
//! \param[in] motorHandle The motor control handle
extern void monitorLostPhase(MOTOR_Handle motorHandle);

//! \brief Monitors motor for current unbalance condition
//! \param[in] motorHandle The motor control handle
extern void monitorCurrentUnbalance(MOTOR_Handle motorHandle);

//! \brief Monitors motor for over speed condition
//! \param[in] motorHandle The motor control handle
extern void monitorOverSpeed(MOTOR_Handle motorHandle);

//! \brief Monitors motor for startup failure condition
//! \param[in] motorHandle The motor control handle
extern void monitorStartupFailure(MOTOR_Handle motorHandle);

//! @}
#ifdef __cplusplus
}
#endif

#endif /* _MOTOR_COMMON_H_ */

/** @} */
