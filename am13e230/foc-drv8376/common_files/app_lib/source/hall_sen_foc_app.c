
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

//
// REFACTORED APPLICATION.C WITH MODULAR ARCHITECTURE
// This file contains only the main entry point functions
// All helper functions have been moved to application.h as static inline
// ARM M33 compatible - uses TMU math functions via fastrts_tmu.h
//

#include <application.h>
#include "hall_sensored_foc.h" /* FOC_runHallObserver — hall-only, no #if needed */
#include "hall_cal.h"          /* HallCal_run_inline, HallCal_average, HallCal_validate */
#include "ipd.h"               /* Needed for IPD functions */
#include "mtr1_init.h"
#include "frl_guard.h"
#include <string.h>

//=============================================================================
// Type-Safe Casting Macros for Sensor Interface Access
//=============================================================================

/**
 * \brief Cast app sensor interface to Hall calibration config
 * \param obj Motor control handle
 * \return Pointer to HallCal_Config_t
 */
#define HALL_APP_CFG(obj) ((HallCal_Config_t *)(obj)->appSensorInterface->appSensorConfig)

/**
 * \brief Cast app sensor interface to Hall calibration data
 * \param obj Motor control handle
 * \return Pointer to HallCal_Data_t
 */
#define HALL_APP_DATA(obj) ((HallCal_Data_t *)(obj)->appSensorInterface->appSensorData)

/**
 * \brief Cast HAL sensor interface to Hall hardware data
 * \param obj Motor control handle
 */
#define HALL_HW_CFG(obj) ((HAL_Hall_Config_t *)(obj)->halSensorInterface->sensorConfig)

/**
 * \brief Cast HAL sensor Data to Hall Sensor Data
 * \param obj Motor control handle
 * \return Pointer to HAL_Hall_Config_t
 */
#define HALL_HW_DATA(obj) ((HAL_Hall_Data_t *)(obj)->halSensorInterface->sensorData)

/**
 * @brief Main state machine execution - determines FOC angle based on motor state
 * @param obj Pointer to motor control structure
 */
static inline void FOC_runStateMachine(MOTOR_Vars_t *obj)
{
    // Execute state-specific logic
    switch (obj->outVars.motorState)
    {
        case MOTOR_OFFSET_CALIB:
            StateMachine_handleOffsetCalib(obj);
            break;

        case MOTOR_STOP_IDLE:
            // Do nothing - motor is stopped
            break;

        case MOTOR_FAULT_STOP:
            break;

        case MOTOR_BRAKE_STOP:
            // TODO: Handle braking based Stop

            obj->outVars.motorState = MOTOR_STOP_IDLE;

            break;

            // #TAG_IPD_START#
        case MOTOR_IPD:
            StateMachine_handleIPD(obj);
            break;
            // #TAG_IPD_END#

        // #TAG_PARAM_ID_START#
        case MOTOR_PARAM_IDENT:
            // #TAG_INSTA_SPIN_START#
            if (obj->statusVars.flagMotorTiTuneRun == false)
            {
                StateMachine_handleParameterID(obj);
            }
            // #TAG_INSTA_SPIN_END#
            // #TAG_TI_TUNE_START#
            else
            {
                StateMachine_handleTiTune(obj);
            }
            // #TAG_TI_TUNE_END#
            break;
            // #TAG_PARAM_ID_END#

        case MOTOR_HALL_CAL:
        {
            /* Hall sensor bidirectional calibration — ISR hot-path.
             * Reads Hall state from the data handle (already updated by
             * FOC_runHallObserver earlier this ISR cycle) and runs one tick
             * of the dither-sweep state machine.  Writes angleFOC_rad and
             * Idq_out_A directly so the existing current-control path is used. */

            HallCal_run_inline(HALL_APP_CFG(obj), HALL_APP_DATA(obj), HALL_HW_DATA(obj)->hallState,
                               &obj->outVars.angleFOC_rad, &obj->outVars.Idq_out_A.value[0],
                               &obj->outVars.Idq_out_A.value[1]);
            break;
        }

        case MOTOR_ALIGNMENT:
            StateMachine_handleAlignment(obj);
            break;

        case MOTOR_OL_START:
            // Run angle generation (for open-loop)
            FOC_runAngleGeneration(obj);
            StateMachine_handleOpenLoopStart(obj);
            break;

        case MOTOR_OL_END:
            // Run angle generation (for open-loop)
            FOC_runAngleGeneration(obj);
            StateMachine_handleOpenLoopEnd(obj);
            break;

        case MOTOR_HALL_STARTUP:
            StateMachine_handleHallStartup(obj);
            break;

        case MOTOR_CL_RUNNING:
        case MOTOR_CTRL_RUN:
            StateMachine_handleClosedLoopRunning(obj);
            break;

        case MOTOR_SEEK_POS:
            StateMachine_handleFlyingStart(obj);
            break;

        default:
            // Handle other states or faults
            break;
    }

    /* Estimated Speed */
    obj->statusVars.speedAbs_Hz = fabsf(obj->outVars.speed_Hz);
}
// ============================================================================
// MAIN APPLICATION RUN - REFACTORED WITH CLEAN MODULAR STRUCTURE
// ============================================================================

/**
 * @brief Main FOC control loop - REFACTORED VERSION
 *
 * This is the high-priority ISR function that executes every PWM cycle (10-20 kHz).
 * It has been refactored into clean, modular sections:
 *
 * 1. FOC Feedback      - Input acquisition and Clarke transform
 * 2. Observers         - FAST and ESMO estimators for angle/speed
 * 3. State Machine     - Motor state management and transitions
 * 4. FOC Feedforward   - Park transform, controllers, inverse Park, SVPWM
 * 5. Data Logging      - RMS collection, datalog, diagnostics
 *
 * This function is placed in RAM (.motor_ctrl_ramfunc section) for faster execution
 * since it runs at high frequency from the ADC ISR.
 *
 * @param motorHandle Handle to motor control structure
 */
MOTOR_CTRL_RAMFUNC void application_run(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    obj->ISRCount++;

    // ========================================================================
    // SECTION 1: FOC FEEDBACK - Input Acquisition
    // ========================================================================
    FOC_runFeedback(obj);

    // ========================================================================
    // SECTION 2: OBSERVERS - Angle and Speed Estimation
    // ========================================================================
    // FOC_runObservers(obj);
    frl_obs_pre(obj);   // FRL: TI's sensorless observer in shadow beside the Halls (frl_guard.c)

    FOC_runHallObserver(obj);
    frl_obs_post(obj);  // FRL: observer angle error, hand-over to the observer and back

    // ========================================================================
    // SECTION 3: STATE MACHINE - Motor State Management
    // ========================================================================
    FOC_runStateMachine(obj);
    frl_test_angle(obj);  // FRL (revision H5): the hold test's angle, test mode only (frl_guard.c)

    // ========================================================================
    // SECTION 4: FOC FEEDFORWARD - Control and PWM Generation
    // ========================================================================
    FOC_runFeedforward(obj);
}

/**
 * @brief Main low-priority state machine execution -
 * @param motorHandle Handle to motor control structure
 *
 *
 * ARCHITECTURE:
 * - High Priority (ISR): FOC_runStateMachine() - Time-critical FOC control
 * - Low Priority (Background): FOC_runLowPriorityStateMachine() - Management & control
 * - Protection (Background): runMotorMonitor() - Monitoring & protection (case 1)
 */
void FOC_runLowPriorityStateMachine(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    LowPriority_handleMotorCheckFaults(motorHandle);

    /* Check for Motor Stop Command if Motor is in Active State */

    LowPriority_handleMotorCheckStop(motorHandle);

    /* Handle Insta Spin Estimator related Errors */
    LowPriority_handleCheckInstaSpinEstimatorErrors(motorHandle);

    LowPriority_handleMotorIDAndTrajectory(motorHandle);

    runMotorMonitor(motorHandle);

    if (HALL_HW_DATA(obj)->flagHallFault == true)
    {
        obj->outVars.faultMtrNow.bit.estimError = 1U;
    }

    // State-specific tasks
    switch (obj->outVars.motorState)
    {
        case MOTOR_STOP_IDLE:
            // During Motor Idle State, continuously reset motor Control
            // TODO: Only Once when exiting from Fault or Stop State
            resetMotorControl(motorHandle);

            HAL_SetCompOCLimits(obj->halInterface);

            /* Restart the Motor if Ru Command is Received */
            LowPriority_handleMotorRestart(motorHandle);

            /* Re-apply motor parameters from motorParams_M1 when requested via GUI.
             * Motor_Params_ReInit() checks flagReInitMotorParams internally and clears
             * it on completion.  Only effective while motor is stopped. */
            Motor_Params_ReInit();

            break;

        case MOTOR_FAULT_STOP:
            // Handle fault stop state
            LowPriority_handleMotorFaults(motorHandle);

            break;

        case MOTOR_NORM_STOP:

            stopMotorControl(motorHandle);

            // Handle brake stop state
            break;

        case MOTOR_BRAKE_STOP:
            // Handle brake stop state
            break;

        case MOTOR_PARAM_IDENT:
            // Handle parameter identification state
            LowPriority_handleMotorIDAndTrajectory(motorHandle);

            // #TAG_FAST_OBS_START#

            LowPriority_runInstaSpinParameterID(motorHandle);
            // #TAG_FAST_OBS_END#
            break;

        case MOTOR_HALL_CAL:
        {
            /* Hall calibration background tasks.
             * When the ISR sweep completes (state == HALL_CAL_AVERAGE), run
             * averaging and validation here in the low-priority loop.
             * On success, wire the calibrated tables into the HAL config so
             * HAL_Hall_computeAngle_inline() uses them immediately. */

            if (HALL_APP_DATA(obj)->state == HALL_CAL_AVERAGE)
            {
                HallCal_average(HALL_APP_DATA(obj), HALL_APP_CFG(obj));
                /* FRL port (2026-10-17): set flagCalDone before validating. HallCal_validate() returns invalid
                 * whenever flagCalDone is false, and in SDK 26.01.00.03 the flag was set one line after the call,
                 * so every calibration was marked invalid (the hall_cal.h comment says average sets it; it does not). */
                HALL_APP_DATA(obj)->flagCalDone = true;
                HallCal_validate(HALL_APP_DATA(obj));
                HALL_APP_DATA(obj)->state       = HALL_CAL_DONE;
            }
            else if (HALL_APP_DATA(obj)->state == HALL_CAL_DONE)
            {
                obj->statusVars.enableSpeedCtrl   = false;
                obj->statusVars.enableCurrentCtrl = false;

                if (HALL_APP_DATA(obj)->flagCalValid == true)
                {
                    /* Wire calibrated tables into the HAL config.
                     * calAngleFwdTablePtr / calAngleRevTablePtr are non-const
                     * pointer fields inside the otherwise-const config struct,
                     * so casting away the outer const is safe here. */
                    HAL_Hall_Config_t *cfg   = HALL_HW_CFG(obj);
                    cfg->calAngleFwdTablePtr = HALL_APP_DATA(obj)->calAngleFwd_rad;
                    cfg->calAngleRevTablePtr = HALL_APP_DATA(obj)->calAngleRev_rad;
                }
                else
                {
                    /* Calibration failed validation — flag fault */
                    obj->outVars.faultMtrNow.bit.estimError = 1U;
                }
                /* Return to idle regardless of validation result */
                obj->outVars.motorState         = MOTOR_STOP_IDLE;
                obj->inputCtrl.flagStartHallCal = false;
            }
            else
            {
                /* Calibration still running in ISR — nothing to do here */
            }
        }

        case MOTOR_HALL_STARTUP:
            /* Hall torque-mode startup — run basic protection monitors only.
             * Full load/stall/phase-loss checks are skipped until closed loop. */
            HAL_SetCompOCLimits(obj->halInterface);
            monitorOverLoad(motorHandle);
            monitorMotorStall(motorHandle);
            break;

        case MOTOR_CL_RUNNING:
        case MOTOR_CTRL_RUN:
            // Handle closed-loop running state - full management

            LowPriority_handleDeadTimeCompensation(motorHandle);

            calculateRMSData(motorHandle);

            HAL_SetCompOCLimits(obj->halInterface);

            // #TAG_FAST_OBS_START#
            //  Monitor Rs online calibration timing

            runRsOnLine(motorHandle);

            // #TAG_FAST_OBS_END#

            // Power and mechanical faults
            monitorOverLoad(motorHandle);
            monitorMotorStall(motorHandle);
            monitorLostPhase(motorHandle);

            // Current and speed faults (only when current is above threshold)
            if (motorHandle->outVars.Is_A > motorHandle->motorSetsHandle->IsFailedChekSet_A)
            {
                monitorCurrentUnbalance(motorHandle);
                monitorOverSpeed(motorHandle);
                monitorStartupFailure(motorHandle);
            }

            break;

        default:
            break;
    }

    LowPriority_handleControllerSetup(motorHandle);

    // #TAG_ESMO_START#
    LowPriority_handleESMOParameterUpdates(motorHandle);
    // #TAG_ESMO_END#

    /* Update Hall speed filter bandwidth dynamically from motor reference speed.
     * See FOC_updateHallSpeedFilter() in hall_sensored_foc.h for details. */
    FOC_updateHallSpeedFilter(obj);

    LowPriority_handleGlobalVariablesUpdate(motorHandle);
}
