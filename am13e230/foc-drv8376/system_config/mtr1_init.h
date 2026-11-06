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

#ifndef MOTOR1_INIT_H
#define MOTOR1_INIT_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __TMS320C28XX_CLA__
#include "libraries/math/include/CLAmath.h"
#else
// includes
// math_types.h internally includes <math.h> and fastrts_tmu.h for TMU acceleration
#include "math_types.h"
#endif

#include "userParams.h"
#include "motor_common.h"
#include "motor_params.h"

//=============================================================================
// Global Variables for Motor 1
//=============================================================================

//! Motor control variables structure
extern MOTOR_Vars_t motorVars_M1;

//! Motor settings/configuration variables
extern MOTOR_SetVars_t motorSetVars_M1;

//! User parameters structure
extern USER_Params userParams_M1;
//=============================================================================
//  Optional Feature Objects for Motor 1
//=============================================================================
// #TAG_ESMO_START#
extern ESMO_Obj    esmo_M1;
// #TAG_ESMO_END#

// #TAG_IPD_START#
extern IPD_Obj ipd_M1;
// #TAG_IPD_END#

// #TAG_MTPA_START#
extern MTPA_Obj mtpa_M1;
// #TAG_MTPA_END#

// #TAG_RECONST_INV_VOLT_START#
extern VOLREC_Obj volrec_M1;
// #TAG_RECONST_INV_VOLT_END#

extern HAL_FOC_Data_Obj halFOCDataObj_M1;  // Serial Communication
//=============================================================================
// Function Prototypes
//=============================================================================

//! \brief Initialize motor control handles and HAL interface
//! \param[in] handle  The motor handle
extern void initMotor1Handles(MOTOR_Handle handle);

//! \brief Initialize all motor control parameters for Motor 1
//! This includes USER params, control params, FOC objects, and optional features
//! \param[in] handle      The motor handle
//! \param[in] motorParams Pointer to the motor constant table
extern void initMotor1CtrlParameters(MOTOR_Handle handle, const Motor_ParamsType *motorParams);

// #TAG_FAST_OBS_START#
//! \brief Set USER parameters for Motor 1 from the motor constant table
//! \param[in] handle      The user parameters handle
//! \param[in] gdHandle    The gate driver data handle
//! \param[in] motorParams Pointer to the motor constant table
extern void USER_setMotor1Params(userParams_Handle handle, const HAL_GD_DATA_Obj *gdHandle,
                                 const Motor_ParamsType *motorParams);

//! \brief Flag to request a low-priority re-initialisation of Motor 1 control
//! parameters from the active motor constant table (motorParams_M1).
//!
//! Set this flag to TRUE from the GUI or debugger while the motor is stopped.
//! FOC_runLowPriorityStateMachine() will call Motor_Params_ReInit() on the
//! next background pass, re-apply all parameters, then clear the flag.
extern volatile bool flagReInitMotorParams;

//! \brief Re-initialise Motor 1 control parameters from motorParams_M1.
//!
//! Called by FOC_runLowPriorityStateMachine() when flagReInitMotorParams is
//! TRUE.  Calls initMotor1CtrlParameters() to re-apply all motor-specific
//! settings and clears the flag on completion.
//!
//! \note Must only be called while the motor is in MOTOR_STOP_IDLE state to
//!       avoid corrupting live control variables.
extern void Motor_Params_ReInit(void);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif  // extern "C"

#endif  // MOTOR1_INIT_H
