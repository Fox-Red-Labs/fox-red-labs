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
//! \file   /solutions/universal_motorcontrol_lab/common/source/motor_comm.c
//!
//! \brief  This project is used to implement motor control with FAST, eSMO
//!         Encoder, and Hall sensors based sensored/sensorless-FOC.
//!         Supports multiple TI EVM boards
//!         ARM M33 compatible - uses TMU math functions via fastrts_tmu.h
//!
//

//
// include the related header files
//

#include "motor_common.h"
#include "ti_tune.h"
#include "hal.h"
#include "hal_communication.h"

//! \brief checks motor faults
void checkMotorFaults(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    if (HAL_getPwmEnableStatus(&obj->halInterface->halFOCDataHandle->pwmData) == true)
    {
        if (HAL_getMtrTripFaults(&obj->halInterface->halFOCConfigHandle->pwmCfg) != 0)
        {
            obj->outVars.faultMtrNow.bit.moduleOverCurrent = 1;
        }
    }
    obj->statusVars.faultMtrPrev.all |= obj->outVars.faultMtrNow.all;
    obj->statusVars.faultMtrUse.all   = obj->outVars.faultMtrNow.all & obj->statusVars.faultMtrMask.all;
    //    HAL_setMtrCMPSSDACValue(obj->halInterface->halFOCHandle,
    //                            objSets->dacCMPValH, objSets->dacCMPValL);
    if (obj->inputCtrl.flagClearFaults == true)
    {
        HAL_clearMtrFaultStatus(obj->halInterface);
        obj->outVars.faultMtrNow.all   &= MTR_FAULT_CLEAR;
        obj->inputCtrl.flagClearFaults  = false;
    }
    return;
}

// Sets up control parameters for stopping motor
void stopMotorControl(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    obj->statusVars.speed_int_Hz = 0.0f;

    obj->statusVars.flagRunIdentAndOnLine = false;
    if (obj->outVars.motorState == MOTOR_BRAKE_STOP)
    {
        if (obj->inputCtrl.brakingMode == HARDSWITCH_BRAKE_MODE)
        {
            // Exit braking PWM mode
            HAL_ResetPWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);
        }

        obj->outVars.motorState          = MOTOR_STOP_IDLE;
        obj->inputCtrl.flagEnableBraking = false;

        obj->IsRef_A = 0.0f;
        PI_setUi(&obj->pi_spd, 0.0f);
        PI_setRefValue(&obj->pi_spd, 0.0f);
    }

    obj->outVars.motorState = MOTOR_STOP_IDLE;

    obj->inputCtrl.svmMode = SVM_COM_C;

    obj->statusVars.restartTimesCnt = 0;

    return;
}

// Sets up control parameters for restarting motor
void restartMotorControl(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    USER_Params *objUser = (USER_Params *)(handle->userParamsHandle);

    if (obj->inputCtrl.flagEnableMotorIdentify == true)
    {
        if (obj->inputCtrl.motorParamIdentifyMethod == MOTOR_PARAM_IDENTIFY_INSTASPIN)
        {
            obj->inputCtrl.estimatorMode        = ESTIMATOR_MODE_FAST;
            obj->statusVars.flagMotorIdentified = false;
            obj->statusVars.flagSetupController = false;
            obj->inputCtrl.svmMode              = SVM_COM_C;
            obj->inputCtrl.flagEnableFWC        = false;
            obj->inputCtrl.flagEnableMTPA       = false;

            obj->inputCtrl.speedRef_Hz = objUser->fluxExcFreq_Hz;

            objUser->flag_bypassMotorId = false;

            // #TAG_FAST_OBS_START#
            // disable interrupts
            __disable_irq();

            // set the default estimator parameters
            EST_setParams(obj->estHandle, obj->userParamsHandle);

            // enable interrupts

            __enable_irq();

            // #TAG_FAST_OBS_END#
        }
        else
        {
            obj->statusVars.flagMotorTiTuneRun = true;
            obj->statusVars.enableSpeedCtrl    = false;
            obj->statusVars.enableCurrentCtrl  = false;
        }
        obj->inputCtrl.flagEnableMotorIdentify = false;

        obj->outVars.motorState = MOTOR_PARAM_IDENT;
    }
    // #TAG_FAST_OBS_START#
    else if (obj->inputCtrl.flagEnableRsRecalc == true)
    {
        obj->inputCtrl.estimatorMode = ESTIMATOR_MODE_FAST;

        // disable the estimator
        EST_disable(obj->estHandle);

        // disable the trajectory generator
        EST_disableTraj(obj->estHandle);

        // disable interrupts

        __disable_irq();

        // set the default estimator parameters
        EST_setParams(obj->estHandle, obj->userParamsHandle);

        // enable interrupts

        __enable_irq();

        // disable the estimator
        EST_enable(obj->estHandle);

        // disable the trajectory generator
        EST_enableTraj(obj->estHandle);

        /* Flag Motor Is Identified set to not perform complete ID Routine */
        EST_setFlag_motorIdentified(obj->estHandle, 1);

        /* Flag EnableRsRecalc is enabled to perform Rs Recalibration */
        EST_setFlag_enableRsRecalc(obj->estHandle, 1);

        obj->outVars.motorState = MOTOR_PARAM_IDENT;
    }
    // #TAG_FAST_OBS_END#

    // TODO: Fix Brake based Stop and Brake Time Count.
    else if (obj->outVars.motorState == MOTOR_BRAKE_STOP)
    {
        if (obj->inputCtrl.brakingMode == HARDSWITCH_BRAKE_MODE)
        {
            // Exit braking PWM mode
            HAL_ResetPWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);
        }

        obj->outVars.motorState          = MOTOR_STOP_IDLE;
        obj->inputCtrl.flagEnableBraking = false;

        obj->IsRef_A = 0.0f;
        PI_setUi(&obj->pi_spd, 0.0f);
        PI_setRefValue(&obj->pi_spd, 0.0f);
    }
    else if (obj->inputCtrl.flagStartHallCal == true)
    {
        obj->outVars.motorState           = MOTOR_HALL_CAL;
        obj->statusVars.enableSpeedCtrl   = false;
        obj->statusVars.enableCurrentCtrl = true;
    }
    else if (obj->inputCtrl.flagEnableFlyingStart == true)
    {
        obj->outVars.motorState           = MOTOR_SEEK_POS;
        obj->statusVars.enableSpeedCtrl   = false;
        obj->statusVars.enableCurrentCtrl = true;
    }
    else
    {
        switch (obj->inputCtrl.startupMode)
        {
            case STARTUP_IPD:
                obj->ipdHandle->state   = IPD_INIT;
                obj->outVars.motorState = MOTOR_IPD;
                break;

            case STARTUP_HALL:
                obj->statusVars.hallStartupEdgeCnt = 0U;
                obj->statusVars.enableSpeedCtrl    = false;
                obj->statusVars.enableCurrentCtrl  = true;
                obj->outVars.motorState            = MOTOR_HALL_STARTUP;
                break;

            case STARTUP_ALIGN:
            default:
                obj->outVars.motorState = MOTOR_ALIGNMENT;
                break;
        }
    }

    // #TAG_ESMO_START#
    if (obj->inputCtrl.estimatorMode == ESTIMATOR_MODE_ESMO)
    {
        ESMO_resetParams(obj->esmoHandle);

        ESMO_resetPLL(obj->esmoHandle);

        obj->statusVars.speedPLL_Hz  = 0;
        obj->statusVars.anglePLL_rad = 0;
    }
    // #TAG_ESMO_END#

    obj->statusVars.speed_int_Hz = 0.0f;

    obj->inputCtrl.svmMode = SVM_COM_C;

    obj->statusVars.flagRunIdentAndOnLine = true;
    obj->statusVars.stateRunTimeCnt       = 0;
    obj->statusVars.startSumTimesCnt++;

    return;
}

// Resets motor control parameters for restarting motor
void resetMotorControl(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    // #TAG_FAST_OBS_START#
    if (obj->inputCtrl.flagEnableFlyingStart == false)
    {
        // disable the estimator
        EST_disable(obj->estHandle);

        // disable the trajectory generator
        EST_disableTraj(obj->estHandle);
    }
    else
    {
        EST_enable(obj->estHandle);
    }

    EST_setFlag_enableRsRecalc(obj->estHandle, 0);

    EST_setRsOnLineId_mag_A(obj->estHandle, 0.0f);
    EST_setRsOnLineId_A(obj->estHandle, 0.0f);
    EST_setRsOnLine_Ohm(obj->estHandle, EST_getRs_Ohm(obj->estHandle));

    EST_setFlag_enableRsOnLine(obj->estHandle, false);
    EST_setFlag_updateRs(obj->estHandle, false);

    // #TAG_FAST_OBS_END#

    /* Disable Speed and Current Control*/
    obj->statusVars.enableCurrentCtrl = false;
    obj->statusVars.enableSpeedCtrl   = false;

    /* Set Voltage output to Zero*/

    obj->outVars.Vdq_out_V.value[0] = 0.0f;
    obj->outVars.Vdq_out_V.value[1] = 0.0f;

    TRAJ_setIntValue(&obj->traj_spd, obj->outVars.speed_Hz);

    TRAJ_setTargetValue(&obj->traj_spd, 0.0f);

    // disable the PWM
    HAL_disablePWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);

    // clear integral outputs of the controllers
    PI_setRefValue(&obj->pi_Id, 0.0f);
    PI_setRefValue(&obj->pi_Iq, 0.0f);
    PI_setRefValue(&obj->pi_spd, 0.0f);

    PI_setUi(&obj->pi_Id, 0.0f);
    PI_setUi(&obj->pi_Iq, 0.0f);
    PI_setUi(&obj->pi_spd, 0.0f);

    // clear current references
    obj->outVars.Idq_out_A.value[0] = 0.0f;
    obj->outVars.Idq_out_A.value[1] = 0.0f;

    obj->IdRated_A = 0.0f;
    obj->IsRef_A   = 0.0f;

    obj->statusVars.angleCurrent_rad = 0.0f;

    PI_setUi(&obj->pi_fwc, 0.0f);

    obj->statusVars.stateRunTimeCnt    = 0;
    obj->statusVars.motorStallTimeCnt  = 0;
    obj->statusVars.startupFailTimeCnt = 0;

    obj->statusVars.overSpeedTimeCnt = 0;
    obj->statusVars.overLoadTimeCnt  = 0;
    obj->statusVars.lostPhaseTimeCnt = 0;
    obj->statusVars.unbalanceTimeCnt = 0;

    obj->statusVars.flagMotorIdentified = true;

    obj->inputCtrl.svmMode = SVM_COM_C;

    // #TAG_TI_TUNE_START#
    obj->tiTuneHandle->TiTune_enable = false;

    obj->statusVars.flagMotorTiTuneRun = false;

    obj->tiTuneHandle->Status.state = TiTune_STATE_IDLE;

    /* Clear PI integrators — not reset by sub-module setParams */
    PI_setUi(&obj->tiTuneHandle->TiTune_PI_Id, 0.0f);
    PI_setUi(&obj->tiTuneHandle->TiTune_PI_Iq, 0.0f);
    PI_setUi(&obj->tiTuneHandle->TiTune_PI_Speed, 0.0f);

    /* Zero Vdq working output so IPARK produces zero Vab on the first tick */
    obj->tiTuneHandle->Status.Out_Vdq_V.value[0] = 0.0f;
    obj->tiTuneHandle->Status.Out_Vdq_V.value[1] = 0.0f;
    // #TAG_TI_TUNE_END#

    return;
}

//! \brief Monitor DC bus over-voltage condition
static inline void monitorOverVoltage(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->halInterface->halFOCDataHandle->adcData.VdcBus_V > objSets->overVoltageFault_V)
    {
        if (obj->statusVars.overVoltageTimeCnt > objSets->voltageFaultTimeSet)
        {
            obj->outVars.faultMtrNow.bit.overVoltage = 1;
        }
        else
        {
            obj->statusVars.overVoltageTimeCnt++;
        }
    }
    else if (obj->halInterface->halFOCDataHandle->adcData.VdcBus_V < objSets->overVoltageNorm_V)
    {
        if (obj->statusVars.overVoltageTimeCnt == 0)
        {
            obj->outVars.faultMtrNow.bit.overVoltage = 0;
        }
        else
        {
            obj->statusVars.overVoltageTimeCnt--;
        }
    }
}

//! \brief Monitor DC bus under-voltage condition
static inline void monitorUnderVoltage(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->halInterface->halFOCDataHandle->adcData.VdcBus_V < objSets->underVoltageFault_V)
    {
        if (obj->statusVars.underVoltageTimeCnt > objSets->voltageFaultTimeSet)
        {
            obj->outVars.faultMtrNow.bit.underVoltage = 1;
        }
        else
        {
            obj->statusVars.underVoltageTimeCnt++;
        }
    }
    else if (obj->halInterface->halFOCDataHandle->adcData.VdcBus_V > objSets->underVoltageNorm_V)
    {
        if (obj->statusVars.underVoltageTimeCnt == 0)
        {
            obj->outVars.faultMtrNow.bit.underVoltage = 0;
        }
        else
        {
            obj->statusVars.underVoltageTimeCnt--;
        }
    }
}

//! \brief Monitor motor over-load condition
void monitorOverLoad(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->outVars.powerActive_W > objSets->overLoadSet_W)
    {
        if (obj->statusVars.overLoadTimeCnt > objSets->overLoadTimeSet)
        {
            obj->outVars.faultMtrNow.bit.overLoad = 1;
            obj->statusVars.overLoadTimeCnt       = 0;
        }
        else
        {
            obj->statusVars.overLoadTimeCnt++;
        }
    }
    else if (obj->statusVars.overLoadTimeCnt > 0)
    {
        obj->statusVars.overLoadTimeCnt--;
    }
}

//! \brief Monitor motor stall condition
void monitorMotorStall(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if ((obj->outVars.Is_A > objSets->stallCurrentSet_A) && (obj->statusVars.speedAbs_Hz < objSets->speedFailMinSet_Hz))
    {
        if (obj->statusVars.motorStallTimeCnt > objSets->motorStallTimeSet)
        {
            obj->outVars.faultMtrNow.bit.motorStall = 1;
            obj->statusVars.motorStallTimeCnt       = 0;
        }
        else
        {
            obj->statusVars.motorStallTimeCnt++;
        }
    }
    else if (obj->statusVars.motorStallTimeCnt > 0)
    {
        obj->statusVars.motorStallTimeCnt--;
    }
}

//! \brief Monitor motor lost phase condition
void monitorLostPhase(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if ((obj->statusVars.speedAbs_Hz > objSets->speedFailMinSet_Hz) &&
        ((obj->statusVars.Irms_A[0] < objSets->lostPhaseSet_A) ||
         (obj->statusVars.Irms_A[1] < objSets->lostPhaseSet_A) ||
         (obj->statusVars.Irms_A[2] < objSets->lostPhaseSet_A)))
    {
        if (obj->statusVars.lostPhaseTimeCnt > objSets->lostPhaseTimeSet)
        {
            obj->outVars.faultMtrNow.bit.motorLostPhase = 1;
            obj->statusVars.lostPhaseTimeCnt            = 0;
        }
        else
        {
            obj->statusVars.lostPhaseTimeCnt++;
        }
    }
    else if (obj->statusVars.lostPhaseTimeCnt > 0)
    {
        obj->statusVars.lostPhaseTimeCnt--;
    }
}

//! \brief Monitor phase current unbalance condition
void monitorCurrentUnbalance(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->statusVars.unbalanceRatio > objSets->unbalanceRatioSet)
    {
        if (obj->statusVars.unbalanceTimeCnt > objSets->unbalanceTimeSet)
        {
            obj->outVars.faultMtrNow.bit.currentUnbalance = 1;
            obj->statusVars.unbalanceTimeCnt              = 0;
        }
        else
        {
            obj->statusVars.unbalanceTimeCnt++;
        }
    }
    else if (obj->statusVars.unbalanceTimeCnt > 0)
    {
        obj->statusVars.unbalanceTimeCnt--;
    }
}

//! \brief Monitor motor over-speed condition
void monitorOverSpeed(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->statusVars.speedAbs_Hz > objSets->speedFailMaxSet_Hz)
    {
        if (obj->statusVars.overSpeedTimeCnt > objSets->overSpeedTimeSet)
        {
            obj->outVars.faultMtrNow.bit.overSpeed = 1;
            obj->statusVars.overSpeedTimeCnt       = 0;
        }
        else
        {
            obj->statusVars.overSpeedTimeCnt++;
        }
    }
    else if (obj->statusVars.overSpeedTimeCnt > 0)
    {
        obj->statusVars.overSpeedTimeCnt--;
    }
}

//! \brief Monitor startup failure condition
void monitorStartupFailure(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if ((obj->outVars.Is_A < objSets->stallCurrentSet_A) && (obj->statusVars.speedAbs_Hz < objSets->speedFailMinSet_Hz))
    {
        if (obj->statusVars.startupFailTimeCnt > objSets->startupFailTimeSet)
        {
            obj->outVars.faultMtrNow.bit.startupFailed = 1;
            obj->statusVars.startupFailTimeCnt         = 0;
        }
        else
        {
            obj->statusVars.startupFailTimeCnt++;
        }
    }
    else if (obj->statusVars.startupFailTimeCnt > 0)
    {
        obj->statusVars.startupFailTimeCnt--;
    }
}

//=============================================================================
// Main Motor Monitor Function (Consolidated)
//=============================================================================

//! \brief Main motor monitoring function - calls all monitoring subfunctions
//! Timer base is 5ms
void runMotorMonitor(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    // Decrement stop wait timer
    if (obj->statusVars.stopWaitTimeCnt > 0)
    {
        obj->statusVars.stopWaitTimeCnt--;
    }

    // Monitor DC bus voltage conditions (always active)
    monitorOverVoltage(handle);
    monitorUnderVoltage(handle);

    return;
}

void collectRMSData(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    obj->statusVars.IrmsCalSum[0] += obj->halInterface->halFOCDataHandle->adcData.I_A.value[0] *
                                     obj->halInterface->halFOCDataHandle->adcData.I_A.value[0];
    obj->statusVars.IrmsCalSum[1] += obj->halInterface->halFOCDataHandle->adcData.I_A.value[1] *
                                     obj->halInterface->halFOCDataHandle->adcData.I_A.value[1];
    obj->statusVars.IrmsCalSum[2] += obj->halInterface->halFOCDataHandle->adcData.I_A.value[2] *
                                     obj->halInterface->halFOCDataHandle->adcData.I_A.value[2];

    obj->statusVars.VIrmsIsrCnt++;

    if (obj->statusVars.VIrmsIsrCnt > obj->statusVars.VIrmsIsrSet)
    {
        obj->statusVars.IrmsPrdSum[0] = obj->statusVars.IrmsCalSum[0];
        obj->statusVars.IrmsPrdSum[1] = obj->statusVars.IrmsCalSum[1];
        obj->statusVars.IrmsPrdSum[2] = obj->statusVars.IrmsCalSum[2];

        obj->statusVars.IrmsCalSum[0] = 0.0f;
        obj->statusVars.IrmsCalSum[1] = 0.0f;
        obj->statusVars.IrmsCalSum[2] = 0.0f;

        obj->statusVars.VIrmsIsrCnt  = 0;
        obj->statusVars.flagVIrmsCal = true;
    }
}

void calculateRMSData(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    float32_t IrmsMax_A, IrmsMin_A, VIrmsIsrSet;

    if (obj->statusVars.flagVIrmsCal == true)
    {
        obj->statusVars.flagVIrmsCal = false;

        obj->statusVars.Irms_A[0] = sqrtf(obj->statusVars.IrmsPrdSum[0] * obj->statusVars.IrmsCalSF);

        obj->statusVars.Irms_A[1] = sqrtf(obj->statusVars.IrmsPrdSum[1] * obj->statusVars.IrmsCalSF);

        obj->statusVars.Irms_A[2] = sqrtf(obj->statusVars.IrmsPrdSum[2] * obj->statusVars.IrmsCalSF);

        if (obj->statusVars.Irms_A[0] > obj->statusVars.Irms_A[1])
        {
            IrmsMax_A = obj->statusVars.Irms_A[0];
            IrmsMin_A = obj->statusVars.Irms_A[1];
        }
        else
        {
            IrmsMax_A = obj->statusVars.Irms_A[1];
            IrmsMin_A = obj->statusVars.Irms_A[0];
        }

        IrmsMax_A = (obj->statusVars.Irms_A[2] > IrmsMax_A) ? obj->statusVars.Irms_A[2] : IrmsMax_A;
        IrmsMin_A = (obj->statusVars.Irms_A[2] < IrmsMin_A) ? obj->statusVars.Irms_A[2] : IrmsMin_A;

        if (obj->statusVars.speedAbs_Hz < 0.0001f)
        {
            obj->statusVars.speedAbs_Hz = 0.0001f;
        }
        VIrmsIsrSet = obj->statusVars.VIrmsIsrScale / obj->statusVars.speedAbs_Hz;
        VIrmsIsrSet = (VIrmsIsrSet > obj->statusVars.VIrmsIsrScale) ? obj->statusVars.VIrmsIsrScale : VIrmsIsrSet;

        obj->statusVars.VIrmsIsrSet = (uint16_t)(VIrmsIsrSet);

        obj->statusVars.IrmsCalSF = 1.0f / ((float32_t)(obj->statusVars.VIrmsIsrSet));

        if ((IrmsMax_A + IrmsMin_A) < 0.001f)
        {
            obj->statusVars.unbalanceRatio = (IrmsMax_A - IrmsMin_A) / 0.001f;
        }
        else
        {
            obj->statusVars.unbalanceRatio = (IrmsMax_A - IrmsMin_A) / (IrmsMax_A + IrmsMin_A);
        }
    }
}

// setupCurrentControllers()
void setupCurrentControllers(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj     = (MOTOR_Vars_t *)handle;
    USER_Params  *objUser = (USER_Params *)(handle->userParamsHandle);

    float32_t RoverL_Kp_sf          = objUser->RoverL_Kp_sf;
    float32_t dcBus_nominal_V       = objUser->dcBus_nominal_V;
    float32_t maxCurrent_A          = objUser->maxCurrent_A;
    float32_t RoverL_min_rps        = objUser->RoverL_min_rps;
    float32_t currentCtrlPeriod_sec = (float32_t)objUser->numCtrlTicksPerCurrentTick / objUser->ctrlFreq_Hz;

    float32_t outMax_V = objUser->Vd_sf * objUser->maxVsMag_V;
    float32_t Kp       = RoverL_Kp_sf * dcBus_nominal_V / maxCurrent_A;
    float32_t Ki       = RoverL_min_rps * currentCtrlPeriod_sec;

    // set the Id controller
    PI_setGains(&obj->pi_Id, Kp, Ki);
    PI_setUi(&obj->pi_Id, 0.0f);
    PI_setRefValue(&obj->pi_Id, 0.0f);
    PI_setFbackValue(&obj->pi_Id, 0.0f);
    PI_setFfwdValue(&obj->pi_Id, 0.0f);
    PI_setMinMax(&obj->pi_Id, -outMax_V, outMax_V);

    // set the Iq controller
    PI_setGains(&obj->pi_Iq, Kp, Ki);
    PI_setUi(&obj->pi_Iq, 0.0f);
    PI_setRefValue(&obj->pi_Iq, 0.0f);
    PI_setFbackValue(&obj->pi_Iq, 0.0f);
    PI_setFfwdValue(&obj->pi_Iq, 0.0f);
    PI_setMinMax(&obj->pi_Iq, -outMax_V, outMax_V);

    return;
}  // end of setupCurrentControllers() function

void setupControllers(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj     = (MOTOR_Vars_t *)handle;
    USER_Params  *objUser = (USER_Params *)(handle->userParamsHandle);

    float32_t Ls_d_H = objUser->motor_Ls_d_H;
    float32_t Ls_q_H = objUser->motor_Ls_q_H;

    float32_t Rs_Ohm       = objUser->motor_Rs_Ohm;
    float32_t RdoverLd_rps = Rs_Ohm / Ls_d_H;
    float32_t RqoverLq_rps = Rs_Ohm / Ls_q_H;

    float32_t BWc_rps               = objUser->BWc_rps;
    float32_t currentCtrlPeriod_sec = (float32_t)objUser->numCtrlTicksPerCurrentTick / objUser->ctrlFreq_Hz;

    float32_t outMax_V = objUser->Vd_sf * objUser->maxVsMag_V;

    float32_t Kp_Id = Ls_d_H * BWc_rps;
    float32_t Ki_Id = 0.25f * RdoverLd_rps * currentCtrlPeriod_sec;

    float32_t Kp_Iq = Ls_q_H * BWc_rps;
    float32_t Ki_Iq = 0.25f * RqoverLq_rps * currentCtrlPeriod_sec;

    // set the Id controller
    PI_setGains(&obj->pi_Id, Kp_Id, Ki_Id);
    PI_setUi(&obj->pi_Id, 0.0f);
    PI_setRefValue(&obj->pi_Id, 0.0f);
    PI_setFbackValue(&obj->pi_Id, 0.0f);
    PI_setFfwdValue(&obj->pi_Id, 0.0f);
    PI_setMinMax(&obj->pi_Id, -outMax_V, outMax_V);

    // set the Iq controller
    PI_setGains(&obj->pi_Iq, Kp_Iq, Ki_Iq);

    PI_setUi(&obj->pi_Iq, 0.0f);
    PI_setRefValue(&obj->pi_Iq, 0.0f);
    PI_setFbackValue(&obj->pi_Iq, 0.0f);
    PI_setFfwdValue(&obj->pi_Iq, 0.0f);
    PI_setMinMax(&obj->pi_Iq, 0.0f, 0.0f);

    // set the speed controller
    if (objUser->Kctrl_Wb_p_kgm2 <= 0.01f)
    {
        float32_t Kp_spd1 = 2.5f * objUser->maxCurrent_A / objUser->maxFrequency_Hz;
        float32_t Ki_spd1 = 5.0f * objUser->maxCurrent_A * objUser->ctrlPeriod_sec;

        PI_setGains(&obj->pi_spd, Kp_spd1, Ki_spd1);
    }
    else
    {
        float32_t speedCtrlPeriod_sec = (float32_t)objUser->numCtrlTicksPerSpeedTick / objUser->ctrlFreq_Hz;

        float32_t BWdelta = objUser->BWdelta;

        float32_t Kctrl_Wb_p_kgm2 = objUser->Kctrl_Wb_p_kgm2;

        float32_t Kp_spd = BWc_rps / (BWdelta * Kctrl_Wb_p_kgm2);
        float32_t Ki_spd =
            BWc_rps * speedCtrlPeriod_sec /
            (BWdelta * BWdelta);  // BWc_rps * BWc_rps * speedCtrlPeriod_sec / (BWdelta * BWdelta * Kctrl_Wb_p_kgm2);

        PI_setGains(&obj->pi_spd, Kp_spd, Ki_spd);
    }

    PI_setUi(&obj->pi_spd, 0.0f);
    PI_setRefValue(&obj->pi_spd, 0.0f);
    PI_setFbackValue(&obj->pi_spd, 0.0f);
    PI_setFfwdValue(&obj->pi_spd, 0.0f);
    PI_setMinMax(&obj->pi_spd, -objUser->maxCurrent_A, objUser->maxCurrent_A);
    // copy the Id, Iq and speed controller parameters to motorVars
    getControllers(handle);

    return;
}  // end of setupControllers() function

// #TAG_FIELD_WEAKENING_START#
void updateFWCParams(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    // Update FW control parameters
    PI_setGains(&obj->pi_fwc, objSets->Kp_fwc, objSets->Ki_fwc);
    PI_setOutMin(&obj->pi_fwc, objSets->angleFWCMax_rad);
}
// #TAG_FIELD_WEAKENING_END#

// #TAG_MTPA_START#
void updateMTPAParams(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->statusVars.flagUpdateMTPAParams == true)
    {
        //
        // update motor parameters according to current
        //
        obj->outVars.LsOnline_d_H = MTPA_updateLs_d_withLUT(obj->mtpaHandle, obj->outVars.Is_A);

        obj->outVars.LsOnline_q_H = MTPA_updateLs_q_withLUT(obj->mtpaHandle, obj->outVars.Is_A);

        obj->outVars.fluxOnline_Wb = objSets->flux_Wb;

        //
        // update the motor constant for MTPA based on
        // the update Ls_d and Ls_q which are the function of Is
        //
        MTPA_computeParameters(obj->mtpaHandle, obj->outVars.LsOnline_d_H, obj->outVars.LsOnline_q_H,
                               obj->outVars.fluxOnline_Wb);
    }

    return;
}
// #TAG_MTPA_END#

// #TAG_FAST_OBS_START#
//=============================================================================
//  Motor Monitoring Subfunctions
//=============================================================================

//! \brief Monitor Rs online calibration timing
static inline void monitorRsOnlineTiming(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    if (obj->inputCtrl.flagEnableRsOnLine == true)
    {
        if (obj->statusVars.flagRsOnLineContinue == true)
        {
            obj->inputCtrl.flagStartRsOnLine = true;
        }
        else if (obj->statusVars.RsOnlineTimeCnt == 0)
        {
            if (EST_getFlag_enableRsOnLine(obj->estHandle) == true)
            {
                obj->statusVars.RsOnlineTimeCnt  = objSets->RsOnlineWaitTimeSet;
                obj->inputCtrl.flagStartRsOnLine = false;
            }
            else
            {
                obj->statusVars.RsOnlineTimeCnt  = objSets->RsOnlineWorkTimeSet;
                obj->inputCtrl.flagStartRsOnLine = true;
            }
        }
        else
        {
            obj->statusVars.RsOnlineTimeCnt--;
        }
    }
    else
    {
        obj->inputCtrl.flagStartRsOnLine = false;
        obj->statusVars.RsOnlineTimeCnt  = 0;
    }
}

void runRsOnLine(MOTOR_Handle handle)
{
    MOTOR_Vars_t    *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    monitorRsOnlineTiming(handle);

    // execute Rs OnLine code
    if (obj->statusVars.flagRunIdentAndOnLine == true)
    {
        if ((EST_getState(obj->estHandle) == EST_STATE_ONLINE) && (obj->inputCtrl.flagStartRsOnLine == true))
        {
            EST_setFlag_enableRsOnLine(obj->estHandle, true);

            EST_setRsOnLineId_mag_A(obj->estHandle, objSets->RsOnLineCurrent_A);

            float32_t RsError_Ohm = objSets->RsOnLine_Ohm - objSets->Rs_Ohm;

            if (fabsf(RsError_Ohm) < (objSets->Rs_Ohm * 0.15f))
            {
                EST_setFlag_updateRs(obj->estHandle, true);
            }
        }
        else
        {
            EST_setRsOnLineId_mag_A(obj->estHandle, 0.0f);
            EST_setRsOnLineId_A(obj->estHandle, 0.0f);
            EST_setRsOnLine_Ohm(obj->estHandle, EST_getRs_Ohm(obj->estHandle));

            EST_setFlag_enableRsOnLine(obj->estHandle, false);
            EST_setFlag_updateRs(obj->estHandle, false);
        }
    }

    return;
}  // end of runRsOnLine() function
// #TAG_FAST_OBS_END#

// update motor control variables
void updateGlobalVariables(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    MOTOR_SetVars_t *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);

    // #TAG_FAST_OBS_START#

    USER_Params *objUser = (USER_Params *)(handle->userParamsHandle);

    // get the states

    obj->statusVars.estState  = EST_getState(obj->estHandle);
    obj->statusVars.trajState = EST_getTrajState(obj->estHandle);

    // get the rotor resistance
    objSets->Rr_Ohm = EST_getRr_Ohm(obj->estHandle);

    // get the stator resistance
    objSets->Rs_Ohm = EST_getRs_Ohm(obj->estHandle);

    // get the stator inductance in the direct coordinate direction
    objSets->Ls_d_H = EST_getLs_d_H(obj->estHandle);

    // get the stator inductance in the quadrature coordinate direction
    objSets->Ls_q_H = EST_getLs_q_H(obj->estHandle);

    // get the flux, V/Hz
    if ((objUser->flag_bypassMotorId == true) || (obj->statusVars.estState >= EST_STATE_RAMPUP))
    {
        objSets->flux_Wb   = EST_getFlux_Wb(obj->estHandle);
        objSets->flux_VpHz = EST_getFlux_Wb(obj->estHandle) * MATH_TWO_PI;
    }

    // get the stator resistance estimate from RsOnLine
    objSets->RsOnLine_Ohm = EST_getRsOnLine_Ohm(obj->estHandle);

    // get the rated magnetizing current value
    objSets->magneticCurrent_A = EST_getIdRated_A(obj->estHandle);

    // get R/L
    objSets->RoverL_rps = EST_getRoverL_rps(obj->estHandle);

    // get the torque estimate
    obj->outVars.torque_Nm = EST_computeTorque_Nm(obj->estHandle);

    // Calculate the motor output power
    obj->statusVars.powerReal_W = obj->outVars.torque_Nm * obj->statusVars.speedAbs_Hz * obj->power_sf;

    // #TAG_FAST_OBS_END#

    // #TAG_ESMO_START#
    // Estimate torque and power for ESMO mode: T = (3/2) * flux_Wb * Iq
    // Uses rated flux from userParams and measured q-axis current
    if (obj->inputCtrl.estimatorMode == ESTIMATOR_MODE_ESMO)
    {
        const USER_Params *esmoUser = (const USER_Params *)(handle->userParamsHandle);

        obj->outVars.torque_Nm      = 1.5f * esmoUser->motor_ratedFlux_Wb * obj->outVars.Idq_in_A.value[1];
        obj->statusVars.powerReal_W = obj->outVars.torque_Nm * obj->statusVars.speedAbs_Hz * obj->power_sf;
    }
    // #TAG_ESMO_END#

    // Calculate the RMS stator current (AC phasor magnitude in DQ frame)
    obj->outVars.Is_A = sqrtf(obj->outVars.Idq_in_A.value[0] * obj->outVars.Idq_in_A.value[0] +
                              obj->outVars.Idq_in_A.value[1] * obj->outVars.Idq_in_A.value[1]);

    // Calculate the DC bus source current: Idc = Da*Ia + Db*Ib + Dc*Ic
    // This is the instantaneous current drawn from the supply (battery/DC link).
    // Differs from Is_A: Is_A is the AC stator phasor magnitude; Idc_A is the
    // real DC bus current = inverter input power / Vdc = sum(duty * phase_current).
    {
        const float32_t da = obj->pwmVabc_pu->value[0];
        const float32_t db = obj->pwmVabc_pu->value[1];
        const float32_t dc = obj->pwmVabc_pu->value[2];
        const float32_t ia = obj->halInterface->halFOCDataHandle->adcData.I_A.value[0];
        const float32_t ib = obj->halInterface->halFOCDataHandle->adcData.I_A.value[1];
        const float32_t ic = obj->halInterface->halFOCDataHandle->adcData.I_A.value[2];
        obj->outVars.Idc_A = (da * ia) + (db * ib) + (dc * ic);
    }

    // Calculate the RMS stator voltage
    obj->statusVars.Vs_V = sqrtf(obj->outVars.Vdq_out_V.value[0] * obj->outVars.Vdq_out_V.value[0] +
                                 obj->outVars.Vdq_out_V.value[1] * obj->outVars.Vdq_out_V.value[1]);

    // Calculate the motor input power
    obj->statusVars.powerInvtOut_W = obj->statusVars.Vs_V * obj->outVars.Is_A * 1.50f;

    // Add a filter to calculate the motor input power
    obj->outVars.powerActive_W = obj->statusVars.powerInvtOut_W * 0.125f + obj->outVars.powerActive_W * 0.875f;

    return;
}  // end of updateGlobalVariables() function

//! \brief Initialize motor control system (interrupts, PWM, gate driver, parameters, offset calibration)
//! \param[in] motorHandle The motor control handle
//! \param[in] isrFunc Pointer to the motor's interrupt service routine
void initMotorControlSystem(MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motorHandle;

    (void)memset(&obj->statusVars, 0, sizeof(MOTOR_StatusVars_t));

    // Initialize the interrupt vector table with motor-specific ISR
    // HAL_initIntVectorTableWithISR(&obj->halInterface->halFOCConfigHandle->intCfg, isrFunc);

    // Enable the ADC/PWM interrupts for control
    // Enable interrupts to trigger DMA
    HAL_enableCtrlInts(&obj->halInterface->halFOCConfigHandle->intCfg);

    // Configure PWM for motor control
    HAL_ConfigPWM(obj->halInterface);

    // Initialize gate driver
    HAL_initGateDriver(obj->gateDriveHandle);

    HAL_clearMtrFaultStatus(obj->halInterface);

    return;
}  // end of initMotorControlSystem() function
