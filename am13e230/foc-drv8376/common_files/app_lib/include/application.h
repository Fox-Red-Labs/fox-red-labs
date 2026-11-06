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
 * \file application.h
 * \brief Motor control application layer interface
 *
 * \defgroup APPLICATION Motor Control Application
 * \brief High-level motor control application functions and state machine
 * @{
 */

#ifndef APPLICATION_H
#define APPLICATION_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#include "frl_guard.h"  /* FRL port: frl_direction (revision H5) */
#include "motor_common.h"
#include "foc.h"

/**
 * \brief Runs the main motor control application loop
 *
 * This is the high-priority ISR function that executes every PWM cycle.
 * It performs FOC feedback, observer updates, state machine execution,
 * feedforward control, and data logging.
 *
 * \param[in] handle Handle to motor control structure
 */
extern void application_run(MOTOR_Handle handle);

/**
 * \brief Runs the low-priority state machine for motor control
 *
 * This function handles fault checking, motor stop commands, and
 * other non-time-critical state machine operations. Should be called
 * from the background loop.
 *
 * \param[in] motorHandle Handle to motor control structure
 */
extern void FOC_runLowPriorityStateMachine(MOTOR_Handle motorHandle);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

/* Configure respctive Observer parameter based on TiTue Values */
static inline void setObserverParams(TiTune_Handle TiTune_handle, MOTOR_Handle motorHandle)
{
    // #TAG_FAST_OBS_START#
    if (motorHandle->inputCtrl.estimatorMode == ESTIMATOR_MODE_FAST)
    {
        EST_setRs_Ohm(motorHandle->estHandle, TiTune_handle->TiTune_Rs.Rs_Final);
        EST_setLs_q_H(motorHandle->estHandle, TiTune_handle->TiTune_LDQ_handler->lq_Final);
        EST_setLs_d_H(motorHandle->estHandle, TiTune_handle->TiTune_LDQ_handler->ld_Final);
    }
    // #TAG_FAST_OBS_END#
    // #TAG_ESMO_START#
    if (motorHandle->inputCtrl.estimatorMode == ESTIMATOR_MODE_ESMO)
    {
        motorHandle->userParamsHandle->motor_Rs_Ohm = TiTune_handle->TiTune_Rs.Rs_Final;
        motorHandle->userParamsHandle->motor_Ls_d_H = TiTune_handle->TiTune_LDQ_handler->ld_Final;
        motorHandle->userParamsHandle->motor_Ls_q_H = TiTune_handle->TiTune_LDQ_handler->lq_Final;
        ESMO_setParams(motorHandle->esmoHandle, motorHandle->userParamsHandle);
    }
    // #TAG_ESMO_END#
}

/* Apply TI Tune identified motor params and recompute FOC PI gains.
 * Called once when TiTune_STATE_DONE is reached.
 *
 * Current loops (pi_Id, pi_Iq): recomputed via setupControllers() using the
 * identified Ld/Lq/Rs written into USER_Params (Kp = L*BWc, Ki = 0.25*R/L*T).
 *
 * Speed loop (pi_spd): copied directly from TiTune_PI_Speed which was computed
 * from the J/Kt frequency-sweep result and validated in closed-loop flux estimation.
 * This avoids the need to back-calculate Kctrl_Wb_p_kgm2 from JoverKt_Avg.
 *
 * All identified values and final PI gains are also written to MOTOR_SetVars_t
 * for monitoring and display.
 */
// #TAG_TI_TUNE_START#
static inline void TiTune_applyFOCControllers(TiTune_Handle tiTuneHandle, MOTOR_Handle motorHandle)
{
    TiTune_Obj      *tiTune  = (TiTune_Obj *)tiTuneHandle;
    MOTOR_Vars_t    *motor   = (MOTOR_Vars_t *)motorHandle;
    USER_Params     *objUser = (USER_Params *)motorHandle->userParamsHandle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)motorHandle->motorSetsHandle;

    /* 1. Write identified motor params to USER_Params so that
     *    setupControllers() derives the current PI gains from them. */
    objUser->motor_Ls_d_H       = tiTune->TiTune_LDQ_handler->ld_Final;
    objUser->motor_Ls_q_H       = tiTune->TiTune_LDQ_handler->lq_Final;
    objUser->motor_Rs_Ohm       = tiTune->TiTune_Rs_handler->Rs_Final;
    objUser->motor_ratedFlux_Wb = tiTune->TiTune_J_Coast_handler->lambda_PM;

    /* 1b. Update Kctrl using the inertia already computed by TiTune_Run_stateMachine()
     *     at TiTune_STATE_INERTIA_DONE (Results.J_Kgm2 = τ × 1.5 × P² × λ_PM / G_DC). */
    objUser->Kctrl_Wb_p_kgm2 =
        (1.5f * (float32_t)objUser->motor_numPolePairs * objUser->motor_ratedFlux_Wb) / tiTune->Results.J_Kgm2;

    /* 2. Store identified params in MOTOR_SetVars_t for monitoring. */
    objSets->Ls_d_H  = tiTune->TiTune_LDQ_handler->ld_Final;
    objSets->Ls_q_H  = tiTune->TiTune_LDQ_handler->lq_Final;
    objSets->Rs_Ohm  = tiTune->TiTune_Rs_handler->Rs_Final;
    objSets->flux_Wb = tiTune->TiTune_J_Coast_handler->lambda_PM;

    /* 3. Recompute and apply current loop PI gains (pi_Id, pi_Iq) and a
     *    placeholder speed gain using setupControllers().  The speed gain
     *    will be replaced in step 4. */
    setupControllers(motorHandle);

    /* 4. Override pi_spd with the gains TI Tune computed directly from the
     *    J/Kt frequency-sweep measurement.  These gains were already proven
     *    stable in closed-loop during flux estimation. */
    PI_setGains(&motor->pi_spd, tiTune->TiTune_PI_Speed.Kp, tiTune->TiTune_PI_Speed.Ki);

    /* 5. Sync the final set of PI gains (including the overridden speed gains)
     *    back to MOTOR_SetVars_t for display. */
    getControllers(motorHandle);

    setObserverParams(tiTuneHandle, motorHandle);
}
// #TAG_TI_TUNE_END#

/* ============================================================================
 * STATE MACHINE FUNCTIONS - STATIC INLINE (for use in monolithic version)
 * ============================================================================ */

/* Handles ISR-synchronous ADC offset calibration state.
 * Increments the calibration tick counter and calls HAL_RunOffsetCalStep()
 * with fresh PWM-synchronous ADC data.  When the step function signals
 * completion (returns 1) the counter is reset and the state machine
 * transitions to MOTOR_STOP_IDLE. */
static inline void StateMachine_handleOffsetCalib(MOTOR_Vars_t *obj)
{
    obj->statusVars.offsetCalibTimeCnt++;

    if (HAL_RunOffsetCalStep(obj->halInterface, obj->statusVars.offsetCalibTimeCnt,
                             obj->motorSetsHandle->offsetCalibTimeSet_cnt) != 0U)
    {
        obj->statusVars.offsetCalibTimeCnt = 0U;
        obj->outVars.motorState            = MOTOR_STOP_IDLE;

        /* Mark calibration done and release the low-priority state machine.
         * main() polls obj->statusVars.flagOffsetCalibDone before setting flagEnableSystem,
         * so fault checks and monitors only start after valid offsets are set. */
        obj->statusVars.flagOffsetCalibDone = true;
    }
    else
    {
        /* Calibration still running - remain in MOTOR_OFFSET_CALIB */
    }
}

/* Handles IPD (Initial Position Detection) state */
static inline void StateMachine_handleIPD(MOTOR_Vars_t *obj)
{
    ipdRun(obj->ipdHandle);

    if (getIpdComplete(obj->ipdHandle))
    {
        if (getIpdTestMode(obj->ipdHandle))
        {
            obj->outVars.motorState            = MOTOR_ALIGNMENT;
            obj->outVars.angleFOC_rad          = obj->ipdHandle->angle * MATH_TWO_PI;
            obj->inputCtrl.flagEnableAlignment = true;
        }
        else
        {
            // IPD complete, transition to open-loop start
            obj->outVars.motorState         = MOTOR_OL_START;
            obj->outVars.Idq_out_A.value[0] = 0;

            if (obj->inputCtrl.direction > 0.0)
            {
                obj->outVars.Idq_out_A.value[1] = obj->motorSetsHandle->startCurrent_A;
                obj->IsRef_A                    = obj->motorSetsHandle->startCurrent_A;
                obj->outVars.angleFOC_rad       = obj->ipdHandle->angle * MATH_TWO_PI;
            }
            else
            {
                obj->IsRef_A                    = -obj->motorSetsHandle->startCurrent_A;
                obj->outVars.Idq_out_A.value[1] = -obj->motorSetsHandle->startCurrent_A;
                obj->outVars.angleFOC_rad       = obj->ipdHandle->angle * MATH_TWO_PI;
            }

            // #TAG_FAST_OBS_START#
            // Initialize estimators with detected angle
            EST_setAngle_rad(obj->estHandle, obj->outVars.angleFOC_rad);
            EST_setAngle_est_rad(obj->estHandle, obj->outVars.angleFOC_rad);
            // #TAG_FAST_OBS_END#

            TRAJ_setIntValue(&obj->traj_spd, 0.0);

            // #TAG_ESMO_START#
            ESMO_setAnglePu(obj->esmoHandle, obj->outVars.angleFOC_rad);
            // #TAG_ESMO_END#

            ANGLE_GEN_setAngle(&obj->angleGen, obj->outVars.angleFOC_rad);
            PI_setUi(&obj->pi_spd, obj->outVars.Idq_out_A.value[1]);
        }
        obj->statusVars.stateRunTimeCnt = 0;
    }

    if (getIpdFaultStatus(obj->ipdHandle))
    {
        obj->outVars.faultMtrNow.bit.motorLostPhase = 1;
        obj->outVars.motorState                     = MOTOR_FAULT_STOP;
    }
}

/* Handles motor Parameter Identification state for tiTune */
// #TAG_TI_TUNE_START#
static inline void StateMachine_handleTiTune(MOTOR_Vars_t *obj)
{
    /* TiTune Input Speed estimated from estimator */
    obj->tiTuneHandle->Config.Speed_Hz = obj->outVars.speed_Hz;

    obj->tiTuneHandle->Config.Observer_Angle_rad = obj->statusVars.angleObs_rad;

    /* Input the DC Bus Voltage ADC data */
    obj->tiTuneHandle->Config.VdcBus_V = (obj->halFOCData->adcData.VdcBus_V);

    /* Link the Alpha Current */
    obj->tiTuneHandle->Config.Iab_A.value[0] = obj->estInputData.Iab_A.value[0];

    /* Link the Beta Current */
    obj->tiTuneHandle->Config.Iab_A.value[1] = obj->estInputData.Iab_A.value[1];

    //  Run the TI_Tune Run
    TiTune_Run_stateMachine(obj->tiTuneHandle);

    /* If the TI tune method is completed , switch to regular operation*/

    // /* Update the Observer Parameters based on the Estimated Rs & Ld,Lq */
    // if (obj->tiTuneHandle->Status.state == TiTune_STATE_OBSERVER_SETUP)
    // {
    //     setObserverParams(obj->tiTuneHandle, obj);
    // }

    if ((obj->tiTuneHandle->Status.state == TiTune_STATE_DONE) ||
        (obj->inputCtrl.motorParamIdentifyMethod == MOTOR_PARAM_IDENTIFY_INSTASPIN) ||
        (obj->outVars.faultMtrNow.all != 0) || (obj->inputCtrl.flagEnableRunAndIdentify == 0))
    {
        /* Apply identified motor params and recompute FOC PI gains only when
         * identification ran to completion (not on an early-abort path). */
        if (obj->tiTuneHandle->Status.state == TiTune_STATE_DONE)
        {
            TiTune_applyFOCControllers(obj->tiTuneHandle, (MOTOR_Handle)obj);
        }

        HAL_disablePWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);

        /* Set the Output Voltage to Zero */
        obj->outVars.Vab_out_V.value[0] = 0;
        obj->outVars.Vab_out_V.value[1] = 0;

        obj->statusVars.flagMotorTiTuneRun      = false;
        obj->statusVars.flagRunIdentAndOnLine   = false;
        obj->inputCtrl.flagEnableRunAndIdentify = false;
        obj->outVars.motorState                 = MOTOR_STOP_IDLE;
    }
}
// #TAG_TI_TUNE_END#

/* Handles motor Parameter Identification state */
static inline void StateMachine_handleParameterID(MOTOR_Vars_t *obj)
{
    // Handle motor identification or Rs recalibration
    // #TAG_FAST_OBS_START#
    if (((EST_isMotorIdentified(obj->estHandle) == false) || (EST_getState(obj->estHandle) == EST_STATE_RS)) &&
        (EST_isEnabled(obj->estHandle) == true))
    {
        obj->outVars.Idq_out_A.value[0] = 0.0f;
        obj->outVars.motorState         = MOTOR_PARAM_IDENT;

        // Setup trajectory generator for identification
        EST_setupTrajState(obj->estHandle, obj->outVars.Idq_out_A.value[1], obj->inputCtrl.speedRef_Hz, 0.0);
        EST_runTraj(obj->estHandle);

        obj->IdRated_A                 = EST_getIntValue_Id_A(obj->estHandle);
        obj->estInputData.speed_ref_Hz = EST_getIntValue_spd_Hz(obj->estHandle);
        obj->statusVars.speed_int_Hz   = obj->estInputData.speed_ref_Hz;

        obj->statusVars.enableSpeedCtrl   = EST_doSpeedCtrl(obj->estHandle);
        obj->statusVars.enableCurrentCtrl = EST_doCurrentCtrl(obj->estHandle);
    }
    // #TAG_FAST_OBS_END#

    obj->outVars.angleFOC_rad = obj->statusVars.angleEST_rad;
}
/* Handles motor alignment state */
static inline void StateMachine_handleAlignment(MOTOR_Vars_t *obj)
{
    obj->statusVars.stateRunTimeCnt++;

    obj->statusVars.enableSpeedCtrl   = false;
    obj->statusVars.enableCurrentCtrl = true;

    obj->IsRef_A                    = 0.0f;
    obj->outVars.Idq_out_A.value[0] = obj->motorSetsHandle->alignCurrent_A;
    obj->outVars.Idq_out_A.value[1] = 0.0f;

    if (obj->inputCtrl.flagEnableAlignDebug == true)
    {
        obj->statusVars.enableCurrentCtrl = false;
        obj->outVars.Vdq_out_V.value[0]   = obj->motorSetsHandle->alignCurrent_A * obj->userParamsHandle->motor_Rs_Ohm;
        obj->outVars.Vdq_out_V.value[1]   = 0.0f;
    }
    TRAJ_setIntValue(&obj->traj_spd, 0.0f);

    // #TAG_ESMO_START#
    ESMO_setAnglePu(obj->esmoHandle, obj->angleAlign_rad);
    // #TAG_ESMO_END#

    ANGLE_GEN_setAngle(&obj->angleGen, obj->angleAlign_rad);

    obj->outVars.angleFOC_rad = obj->angleAlign_rad;

    if (((obj->statusVars.stateRunTimeCnt > obj->motorSetsHandle->alignTimeDelay_cnt) ||
         (obj->inputCtrl.flagEnableAlignment == false)) &&
        (!obj->inputCtrl.flagEnableContAlign))
    {
        // Alignment complete, transition to open-loop start
        obj->statusVars.stateRunTimeCnt = 0;
        obj->outVars.motorState         = MOTOR_OL_START;
        if (obj->inputCtrl.direction > 0.0)
        {
            obj->outVars.Idq_out_A.value[0] = 0;
            obj->IsRef_A                    = obj->motorSetsHandle->startCurrent_A;
            obj->outVars.Idq_out_A.value[1] = obj->motorSetsHandle->startCurrent_A;
        }
        else
        {
            obj->outVars.Idq_out_A.value[0] = 0;
            obj->IsRef_A                    = -obj->motorSetsHandle->startCurrent_A;
            obj->outVars.Idq_out_A.value[1] = -obj->motorSetsHandle->startCurrent_A;
        }
        obj->statusVars.enableSpeedCtrl   = false;
        obj->statusVars.enableCurrentCtrl = true;

        // #TAG_FAST_OBS_START#
        EST_setAngle_rad(obj->estHandle, obj->outVars.angleFOC_rad);
        // #TAG_FAST_OBS_END#

        PI_setUi(&obj->pi_spd, obj->outVars.Idq_out_A.value[1]);
    }
}

/* Handles open-loop start state */
static inline void StateMachine_handleOpenLoopStart(MOTOR_Vars_t *obj)
{
    obj->statusVars.enableSpeedCtrl = false;

    obj->statusVars.enableCurrentCtrl = true;

    obj->outVars.Idq_out_A.value[0] = 0;

    if (obj->inputCtrl.direction > 0.0)
    {
        obj->outVars.Idq_out_A.value[1] = obj->motorSetsHandle->startCurrent_A;
        obj->IsRef_A                    = obj->motorSetsHandle->startCurrent_A;
    }
    else
    {
        obj->outVars.Idq_out_A.value[1] = -obj->motorSetsHandle->startCurrent_A;
        obj->IsRef_A                    = -obj->motorSetsHandle->startCurrent_A;
    }

    // Check if speed reached force threshold
    if ((fabsf(obj->statusVars.speed_int_Hz) >= obj->motorSetsHandle->olSpeedThr) &&
        (!obj->inputCtrl.flagEnableOpenLoopDebug))
    {
        obj->angleOLDelta_rad = obj->angleGen_rad - obj->statusVars.angleObs_rad;

        /* Compute the Iq target for the threshold speed*/
        obj->iqTarget_OLEnd_A = obj->motorSetsHandle->startCurrent_A * fabsf(__cos(obj->angleOLDelta_rad));

        obj->outVars.motorState = MOTOR_OL_END;
    }

    obj->outVars.angleFOC_rad = obj->angleGen_rad;
}

/* Handles open-loop to closed-loop transition state */
static inline void StateMachine_handleOpenLoopEnd(MOTOR_Vars_t *obj)
{
    obj->statusVars.enableSpeedCtrl = true;

    float32_t sineAngleOffset = __sin(obj->angleOLDelta_rad);

    /* Gradually reduce angle offset toward zero */
    if (sineAngleOffset > 0)
    {
        obj->angleOLDelta_rad = obj->angleOLDelta_rad - obj->angleOLStep_rad;
    }
    else
    {
        obj->angleOLDelta_rad = obj->angleOLDelta_rad + obj->angleOLStep_rad;
    }

    obj->outVars.angleFOC_rad = MATH_incrAngle(obj->statusVars.angleObs_rad, obj->angleOLDelta_rad);

    if (fabsf(sineAngleOffset) < __sin(obj->angleOLStep_rad * 5.0f))
    {
        obj->outVars.motorState = MOTOR_CTRL_RUN;

        PI_setMinMax(&obj->pi_spd, -obj->motorSetsHandle->maxCurrent_A, obj->motorSetsHandle->maxCurrent_A);
        TRAJ_setTargetValue(&obj->traj_spd, obj->inputCtrl.speedRef_Hz);
    }
}

/* Handles Hall-sensor torque-mode startup state (MOTOR_HALL_STARTUP).
 *
 * Drives a fixed Iq (startCurrent_A, direction-aware) with speed control
 * disabled so the motor accelerates under open torque.  Uses the Hall angle
 * directly for FOC rather than an observer or open-loop generator.
 * Counts Hall edge transitions; after (hallStartupCycles * 6) edges the
 * filtered Hall speed is checked.  When speed is non-zero the speed-PI
 * integrator is pre-loaded with the current Iq and the trajectory is seeded
 * at the actual Hall speed, then the state transitions to MOTOR_CTRL_RUN for
 * closed-loop speed control.  If speed is still zero the torque-mode phase
 * continues until the next edge threshold. */
static inline void StateMachine_handleHallStartup(MOTOR_Vars_t *obj)
{
    obj->statusVars.enableSpeedCtrl   = false;
    obj->statusVars.enableCurrentCtrl = true;
    obj->outVars.Idq_out_A.value[0]   = 0.0f;

    if (obj->inputCtrl.direction >= 0.0f)
    {
        obj->outVars.Idq_out_A.value[1] = obj->motorSetsHandle->startCurrent_A;
        obj->IsRef_A                    = obj->motorSetsHandle->startCurrent_A;
    }
    else
    {
        obj->outVars.Idq_out_A.value[1] = -obj->motorSetsHandle->startCurrent_A;
        obj->IsRef_A                    = -obj->motorSetsHandle->startCurrent_A;
    }

    /* Use Hall angle directly — no open-loop generator, no observer needed */
    HAL_Hall_Data_t *hallData = (HAL_Hall_Data_t *)(obj->halSensorInterface->sensorData);
    obj->outVars.angleFOC_rad = hallData->angleHall_rad;

    /* Accumulate Hall edge transitions */
    if (hallData->flagNewEdge == true)
    {
        obj->statusVars.hallStartupEdgeCnt++;
    }

    /* Target: hallStartupCycles full electrical revolutions (6 edges each) */
    uint16_t targetEdges = (uint16_t)obj->motorSetsHandle->hallStartupCycles * 6U;

    if (obj->statusVars.hallStartupEdgeCnt >= targetEdges)
    {
        float32_t hallSpeed_Hz = hallData->speedHall_Hz;

        if (fabsf(hallSpeed_Hz) > 0.0f)
        {
            /* Pre-load speed PI integrator with current Iq to avoid a current
             * step at the torque-to-speed-control handoff */
            PI_setUi(&obj->pi_spd, obj->outVars.Idq_out_A.value[1]);
            PI_setRefValue(&obj->pi_spd, hallSpeed_Hz);
            PI_setMinMax(&obj->pi_spd, -obj->motorSetsHandle->maxCurrent_A, obj->motorSetsHandle->maxCurrent_A);

            /* Seed trajectory at current Hall speed, ramp toward user reference */
            TRAJ_setIntValue(&obj->traj_spd, hallSpeed_Hz);
            TRAJ_setTargetValue(&obj->traj_spd, obj->inputCtrl.speedRef_Hz);
            obj->statusVars.speed_int_Hz = hallSpeed_Hz;

            obj->statusVars.hallStartupEdgeCnt = 0U;
            obj->statusVars.enableSpeedCtrl    = true;
            obj->outVars.motorState            = MOTOR_CTRL_RUN;
        }
        /* else: speed not yet established — keep counting edges and driving torque */
    }
}

/* Handles closed-loop running state */
static inline void StateMachine_handleClosedLoopRunning(MOTOR_Vars_t *obj)
{
    TRAJ_run(&obj->traj_spd);
    obj->statusVars.speed_int_Hz = TRAJ_getIntValue(&obj->traj_spd);

    obj->outVars.angleFOC_rad = obj->statusVars.angleObs_rad;

    collectRMSData(obj);
}

/* Handles flying start (seek position) state */
static inline void StateMachine_handleFlyingStart(MOTOR_Vars_t *obj)
{
    obj->IsRef_A                    = 0.0f;
    obj->outVars.Idq_out_A.value[0] = 0.0f;
    obj->outVars.Idq_out_A.value[1] = 0.0f;
    obj->outVars.angleFOC_rad       = 0.0;

    obj->statusVars.stateRunTimeCnt++;

    if (obj->statusVars.stateRunTimeCnt > obj->motorSetsHandle->flyingStartTimeDelay_cnt)
    {
        obj->statusVars.stateRunTimeCnt = 0;

        if (obj->statusVars.speedAbs_Hz > obj->motorSetsHandle->speedFlyingStart_Hz)
        {
            obj->statusVars.enableSpeedCtrl = true;
            obj->statusVars.speed_int_Hz    = obj->outVars.speed_Hz;
            // #TAG_ESMO_START#
            obj->statusVars.speedPLL_Hz     = obj->outVars.speed_Hz;
            // #TAG_ESMO_END#
            TRAJ_setIntValue(&obj->traj_spd, obj->outVars.speed_Hz);
            PI_setUi(&obj->pi_spd, obj->outVars.Idq_in_A.value[1]);
            obj->outVars.motorState = MOTOR_CTRL_RUN;
        }
        else
        {
            switch (obj->inputCtrl.startupMode)
            {
                case STARTUP_IPD:
                    obj->ipdHandle->state   = IPD_INIT;
                    obj->outVars.motorState = MOTOR_IPD;
                    break;
                case STARTUP_ALIGN:
                default:
                    obj->outVars.motorState = MOTOR_ALIGNMENT;
                    break;
            }
        }
    }
}

/* ============================================================================
 * SECTION 6: LOW PRIORITY STATE MACHINE - Background Tasks
 * ============================================================================ */

/* Handles motor Fault Stop Exit condition */
static inline void LowPriority_handleMotorFaults(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;
    if (obj->inputCtrl.flagClearFaults == true)
    {
        HAL_clearMtrFaultStatus(obj->halInterface);
        obj->outVars.faultMtrNow.all   &= MTR_FAULT_CLEAR;
        obj->inputCtrl.flagClearFaults  = false;

        obj->outVars.motorState = MOTOR_NORM_STOP;
    }
    HAL_disablePWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);
}

/* Handles motor trip fault checking */
static inline void LowPriority_handleMotorCheckFaults(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)motorHandle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(motorHandle->motorSetsHandle);

    /* check for Motor Trip Fault Status.
     * Skip during MOTOR_OFFSET_CALIB and MOTOR_IPD: PWM is enabled at 0% duty
     * during calibration and comparator trips from startup noise must not abort it. */
    if (HAL_getPwmEnableStatus(&obj->halInterface->halFOCDataHandle->pwmData) == true)
    {
        if ((HAL_getMtrTripFaults(&obj->halInterface->halFOCConfigHandle->pwmCfg) != 0) &&
            (obj->outVars.motorState != MOTOR_IPD) && (obj->outVars.motorState != MOTOR_OFFSET_CALIB))
        {
            obj->outVars.faultMtrNow.bit.moduleOverCurrent = 1;
        }
    }

    if ((HAL_getGDTripFaults(&obj->halInterface->halFOCConfigHandle->pwmCfg) != 0))
    {
        obj->outVars.faultMtrNow.bit.gateDriverFault = 1;
    }

    obj->statusVars.faultMtrPrev.all |= obj->outVars.faultMtrNow.all;
    obj->statusVars.faultMtrUse.all   = obj->outVars.faultMtrNow.all & obj->statusVars.faultMtrMask.all;

    // Had some faults to stop the motor
    if (obj->statusVars.faultMtrUse.all != 0)
    {
        obj->outVars.motorState = MOTOR_FAULT_STOP;

        obj->statusVars.stopWaitTimeCnt = objSets->restartWaitTimeSet;
    }
    // TODO: Check for the Restart logic..
    //        obj->restartTimesCnt++;
    //
    //        if(obj->flagEnableRestart == false)
    //        {
    //            obj->stopWaitTimeCnt = 0;
    //        }
    //
    //        if(obj->stopWaitTimeCnt == 0)
    //        {
    //            if(obj->restartTimesCnt < objSets->restartTimesSet)
    //            {
    //                obj->flagClearFaults = 1;
    //            }
    //            else
    //            {
    //                obj->flagEnableRunAndIdentify = false;
    //            }
    //        }
    //    }
    if (obj->inputCtrl.flagClearFaults == true)
    {
        HAL_clearMtrFaultStatus(obj->halInterface);
        obj->outVars.faultMtrNow.all   &= MTR_FAULT_CLEAR;
        obj->inputCtrl.flagClearFaults  = false;
        obj->outVars.motorState         = MOTOR_NORM_STOP;
    }
}

/* Handles Normal Stop Operation */
static inline void LowPriority_handleMotorCheckStop(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)motorHandle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(motorHandle->motorSetsHandle);

    /* If Stop Motor Command is issued, move the motor to Stop State */
    if ((obj->inputCtrl.flagEnableRunAndIdentify == false) && (obj->outVars.motorState >= MOTOR_SEEK_POS))
    {
        if (obj->inputCtrl.flagEnableFlyingStart == false)
        {
            obj->statusVars.stopWaitTimeCnt = objSets->stopWaitTimeSet;
        }
        else
        {
            obj->statusVars.stopWaitTimeCnt = 0;
        }
        // disable the PWM
        HAL_disablePWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);
        obj->outVars.motorState = MOTOR_NORM_STOP;
    }
}

/* Handles fault detection and motor restart logic */
static inline void LowPriority_handleMotorRestart(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    if (motorHandle->inputCtrl.flagEnableRunAndIdentify == true)
    {
        if (HAL_getPwmEnableStatus(&obj->halInterface->halFOCDataHandle->pwmData) == false)
        {
            // enable the PWM
            HAL_enablePWM(&obj->halInterface->halFOCConfigHandle->pwmCfg,
                          &obj->halInterface->halFOCDataHandle->pwmData);
        }

        restartMotorControl(motorHandle);

        // TODO: Move to Estimator routine

        // enable the estimator
        // #TAG_FAST_OBS_START#
        EST_enable(obj->estHandle);

        // enable the trajectory generator
        EST_enableTraj(obj->estHandle);
        // #TAG_FAST_OBS_END#
    }
}

/* Handles estimator and PWM enable logic */
// #TAG_FAST_OBS_START#
static inline void LowPriority_runInstaSpinParameterID(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    USER_Params *objUser = (USER_Params *)(motorHandle->userParamsHandle);

    // TODO : Identify the purpose and requirement
    EST_updateTrajState(obj->estHandle);

    if (obj->statusVars.flagEstStateChanged == true)
    {
        // During InstaSpin based Motor Parameter Identification,
        //  configure the trajectory generator
        EST_configureTraj(obj->estHandle);

        // configure the controllers
        EST_configureTrajState(obj->estHandle, obj->userParamsHandle, &obj->pi_spd, &obj->pi_Id, &obj->pi_Iq);

        /* Exit condition for Instaspin based Motor Parameter Identification */
        /* Move to Parameter Identification */
        if (objUser->flag_bypassMotorId == false)
        {
            if (((EST_isMotorIdentified(obj->estHandle) == true) && (EST_isIdle(obj->estHandle) == true)))
            {
                if (EST_isMotorIdentified(obj->estHandle) == true)
                {
                    obj->statusVars.flagMotorIdentified = true;

                    // clear the flag
                    obj->outVars.motorState = MOTOR_NORM_STOP;

                    obj->inputCtrl.flagEnableRunAndIdentify = false;

                    // disable the estimator
                    EST_disable(obj->estHandle);

                    // enable the trajectory generator
                    EST_disableTraj(obj->estHandle);
                }
            }
        }
        if ((EST_isOnLine(obj->estHandle) == true) && (obj->inputCtrl.flagEnableRsRecalc == true))
        {
            obj->inputCtrl.flagEnableRsRecalc = false;
            obj->outVars.motorState           = MOTOR_STOP_IDLE;
        }
    }
}
// #TAG_FAST_OBS_END#

/* Updates saturation inductance based on current.
 * This function updates the d-axis and q-axis inductances based on the
 * stator current magnitude, accounting for saturation effects. */
static inline void LowPriority_updateSaturationInductance(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)motorHandle;
    USER_Params     *objUser = (USER_Params *)(motorHandle->userParamsHandle);
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(motorHandle->motorSetsHandle);

    // Calculate the Ld and Lq which reduce with current
    objSets->Ls_d_comp_H = objUser->motor_Ls_d_H * (1.0f - obj->outVars.Is_A * objSets->Ls_d_Icomp_coef);
    objSets->Ls_q_comp_H = objUser->motor_Ls_q_H * (1.0f - obj->outVars.Is_A * objSets->Ls_q_Icomp_coef);

    // Enforce minimum inductance limits
    if (objSets->Ls_d_comp_H < objSets->Ls_min_H)
    {
        objSets->Ls_d_comp_H = objSets->Ls_min_H;
    }
    if (objSets->Ls_q_comp_H < objSets->Ls_min_H)
    {
        objSets->Ls_q_comp_H = objSets->Ls_min_H;
    }

    // #TAG_FAST_OBS_START#
    //  Update the Ld and Lq for motor control
    EST_setLs_d_H(obj->estHandle, objSets->Ls_d_comp_H);
    EST_setLs_q_H(obj->estHandle, objSets->Ls_q_comp_H);
    // #TAG_FAST_OBS_END#
}

/* Handles motor identification and trajectory setup */
static inline void LowPriority_handleMotorIDAndTrajectory(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj     = (MOTOR_Vars_t *)motorHandle;
    USER_Params  *objUser = (USER_Params *)(motorHandle->userParamsHandle);

    /* Motor ID will be true if Motor ID is bypassed / Motor ID completes */
    /* Trajectory is set for Openloop & Closed Loop */
    /* Set the SVM Mode , Min max Current , Set the Ramp rate */
    if (obj->statusVars.flagMotorIdentified == true)
    {
        /* FRL port (guard revision H5): the direction from the rotor while it turns, and a zero reference keeps the
         * last direction; the SDK used the reference's sign, with 0 read as reverse (frl_guard.c, frl_direction) */
        obj->inputCtrl.direction = frl_direction(obj->inputCtrl.speedRef_Hz, obj->inputCtrl.direction);

        // #TAG_FAST_OBS_START#
        //  enable or disable force angle
        EST_setFlag_enableForceAngle(obj->estHandle, obj->statusVars.flagEnableForceAngle);
        // #TAG_FAST_OBS_END#

        if (obj->outVars.motorState >= MOTOR_CL_RUNNING)
        {
            TRAJ_setTargetValue(&obj->traj_spd, obj->inputCtrl.speedRef_Hz);
        }
        else
        {
            TRAJ_setTargetValue(&obj->traj_spd, (obj->motorSetsHandle->olSpeedThr * obj->inputCtrl.direction));
        }

        if (obj->outVars.motorState == MOTOR_CTRL_RUN)
        {
            TRAJ_setMaxDelta(&obj->traj_spd, (obj->motorSetsHandle->accelerationMax_Hzps * objUser->ctrlPeriod_sec));

            if (obj->inputCtrl.flagEnableLsUpdate == true)
            {
                // Update saturation inductance based on current
                LowPriority_updateSaturationInductance(motorHandle);
            }

            /* Set the Current Limit to -Ve to allow reverse current to Bus */
            // TODO: Enable only when AVS is Disabled.

            PI_setMinMax(&obj->pi_spd, -obj->motorSetsHandle->maxCurrent_A, obj->motorSetsHandle->maxCurrent_A);
        }
        else
        {
            TRAJ_setMaxDelta(&obj->traj_spd, (obj->motorSetsHandle->accelerationStart_Hzps * objUser->ctrlPeriod_sec));

            if (obj->statusVars.speed_int_Hz >= 0.0f)
            {
                PI_setMinMax(&obj->pi_spd, 0.0f, obj->motorSetsHandle->startCurrent_A);
            }
            else
            {
                PI_setMinMax(&obj->pi_spd, -obj->motorSetsHandle->startCurrent_A, 0.0f);
            }
        }
    }
}

/* Handles estimator error checking */
static inline void LowPriority_handleCheckInstaSpinEstimatorErrors(MOTOR_Handle motorHandle)
{
    // #TAG_FAST_OBS_START#
    // check for the error in the fast estimator
    if ((EST_isError(motorHandle->estHandle) == true) || (EST_isTrajError(motorHandle->estHandle) == true))
    {
        // disable the PWM
        HAL_disablePWM(&motorHandle->halInterface->halFOCConfigHandle->pwmCfg,
                       &motorHandle->halInterface->halFOCDataHandle->pwmData);
        motorHandle->outVars.faultMtrNow.bit.estimError = 1;
    }
    // #TAG_FAST_OBS_END#
}

/* Handles controller setup and updates */
static inline void LowPriority_handleControllerSetup(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    obj->statusVars.flagEstStateChanged = false;

    // #TAG_FAST_OBS_START#
    obj->statusVars.flagEstStateChanged = EST_updateState(obj->estHandle, 0.0f);

    obj->statusVars.flagMotorIdentified = EST_isMotorIdentified(obj->estHandle);
    // #TAG_FAST_OBS_END#

    // TODO: Only based on updated state
    if (obj->statusVars.flagMotorIdentified == true)
    {
        if (obj->statusVars.flagSetupController == true)
        {
            // update the controller
            updateControllers(motorHandle);
        }
        else
        {
            obj->statusVars.flagSetupController = true;

            setupControllers(motorHandle);
        }
    }
}

/* Handles dead time compensation setup */
static inline void LowPriority_handleDeadTimeCompensation(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj     = (MOTOR_Vars_t *)motorHandle;
    USER_Params  *objUser = (USER_Params *)(motorHandle->userParamsHandle);

    /* Sync flagEnableDTC → DTC library enable/disable state */
    if (obj->inputCtrl.flagEnableDTC == true)
    {
        DTC_enable(obj->deadTimeCompHandle);
    }
    else
    {
        DTC_disable(obj->deadTimeCompHandle);
    }

    if (DTC_isEnabled(obj->deadTimeCompHandle) == true)
    {
        DTC_updateCoeffs(obj->deadTimeCompHandle, obj->outVars.speed_Hz, objUser->ctrlPeriod_sec,
                         obj->halInterface->halFOCDataHandle->adcData.VdcBus_V);
    }
}

/* Handles global variables update */
static inline void LowPriority_handleGlobalVariablesUpdate(MOTOR_Handle motorHandle)
{
    // update the global variables
    updateGlobalVariables(motorHandle);
}

/* Handles ESMO parameter updates */
// #TAG_ESMO_START#
static inline void LowPriority_handleESMOParameterUpdates(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    // ESMO : Update ESMO Params
    ESMO_updateFilterParams(obj->esmoHandle);
    ESMO_updatePLLParams(obj->esmoHandle);
}
// #TAG_ESMO_END#
#endif
