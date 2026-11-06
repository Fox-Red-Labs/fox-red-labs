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
// Motor 1 Initialization - Consolidated motor-specific initialization
// This file contains all Motor 1 initialization including:
// - USER parameter configuration (from user_mtr1.h macros)
// - Control parameter setup
// - Optional feature object allocation and initialization
//
// ARM M33 compatible - uses TMU math functions via fastrts_tmu.h
//

#include "application.h"
#include "main.h"
#include "hal.h"
#include "mtr_config.h"
#include "motor_params.h"
#include "hal_hall_sen_interface.h"
// #TAG_HALL_CAL_START#
#include "hall_cal.h" /* HallCal_init() — called when hallInterface != NULL */
// #TAG_HALL_CAL_END#

//=============================================================================
// Global Data Allocation for Motor 1
//=============================================================================

//! Hardware abstraction layer object for motor control
volatile MOTOR_Handle motorHandle_M1;

//! Motor settings/configuration variables
MOTOR_SetVars_t motorSetVars_M1;

//! User parameters structure
USER_Params userParams_M1;

// #TAG_ESMO_START#
ESMO_Obj esmo_M1;

// #TAG_ESMO_END#

// #TAG_IPD_START#
IPD_Obj ipd_M1;

// #TAG_IPD_END#

// #TAG_MTPA_START#
MTPA_Obj mtpa_M1;

// #TAG_MTPA_END#

// #TAG_RECONST_INV_VOLT_START#
VOLREC_Obj volrec_M1;

// #TAG_RECONST_INV_VOLT_END#

// #TAG_TI_TUNE_START#
TiTune_Obj TiTune_M1;

// #TAG_TI_TUNE_END#

//=============================================================================
// Hall Sensor Interface Structures (Module-Level Statics)
//=============================================================================

// #TAG_HALL_CAL_START#
// Hall sensor HAL structures (reference from system_config.c)
extern HAL_Hall_Config_t hallConfig_M1;  /* FRL port: writable, see system_config.c */
extern HAL_Hall_Data_t         hallData_M1;

// Hall sensor application structures
HallCal_Config_t hallSensorConfig;
HallCal_Data_t   hallSensorData;

// Generic sensor interface structures
static HAL_Sensor_Interface_t halSensorInterface = {
    .sensorConfig = (void *)&hallConfig_M1,
    .sensorData   = (void *)&hallData_M1,
};

static APP_Sensor_Interface_t appSensorInterface = {
    .appSensorConfig = &hallSensorConfig,
    .appSensorData   = &hallSensorData,
};
// #TAG_HALL_CAL_END#

/**
 * \brief Motor control variables for Motor 1
 *
 * Instantiated here in mtr1_init.c. The gateDriveHandle and halInterface
 * reference HAL objects defined in system_config.c. hallInterface
 * references the Hall sensor interface for this hall-sensored project.
 */
MOTOR_Vars_t motorVars_M1 = {
    .gateDriveHandle    = &halGDInst,          /**< Gate driver interface handle */
    .halInterface       = &halFOCInterface,    /**< FOC interface handle */
    .halSensorInterface = &halSensorInterface, /**< Generic HAL sensor interface */
    .appSensorInterface = &appSensorInterface, /**< Generic app sensor interface */
};

//=============================================================================
// Motor 1 USER Parameter Configuration
//=============================================================================

//*****************************************************************************
//
//  USER_setMotor1Params - Set USER parameters for Motor 1 based on macros
//  in user_mtr1.h. This configures motor-specific electrical parameters,
//  timing parameters, and estimator wait times.
//
//*****************************************************************************
void USER_setMotor1Params(userParams_Handle handle, const HAL_GD_DATA_Obj *gdHandle,
                          const Motor_ParamsType *motorParams)
{
    USER_Params *objUser = (USER_Params *)handle;

    objUser->numIsrTicksPerCtrlTick = 1;
    objUser->numIsrTicksPerEstTick  = 1;
    objUser->numIsrTicksPerTrajTick = 1;

    objUser->numCtrlTicksPerCurrentTick = 1;
    objUser->numCtrlTicksPerSpeedTick   = USER_M1_NUM_ISR_TICKS_PER_SPEED_TICK;

    objUser->motor_type = MOTOR_TYPE_PM;

    objUser->motor_numPolePairs = motorParams->NumPolePairs;

    objUser->dcBus_nominal_V = gdHandle->gdVolSF * 4096.0;

    objUser->systemFreq_MHz = USER_SYSTEM_FREQ_MHz;

    objUser->pwmPeriod_usec = USER_M1_PWM_PERIOD_usec;

    objUser->voltage_sf = gdHandle->gdVolSF;

    objUser->current_sf = gdHandle->gdCurrentSF;

    objUser->dcBusPole_rps = USER_M1_DCBUS_POLE_rps;

    objUser->speedPole_rps = USER_M1_SPEED_POLE_rps;

    objUser->voltageFilterPole_rps = gdHandle->gdVolFilterPoleFreqRPS;

    objUser->maxVsMag_pu = USER_M1_MAX_VS_MAG_PU;

    objUser->motor_ratedFlux_Wb = motorParams->RatedFlux_VpHz / MATH_TWO_PI;

    objUser->motor_Rr_Ohm = 0;                    // Full Fast Library
    objUser->motor_Rs_Ohm = motorParams->Rs_Ohm;  // Full Fast Library

    objUser->motor_Ls_d_H = motorParams->Ls_d_H;
    objUser->motor_Ls_q_H = motorParams->Ls_q_H;

    objUser->maxCurrent_A = USER_M1_BENCH_MAX_CURRENT_A;

    objUser->Vd_sf      = USER_M1_VD_SF;
    objUser->maxVsMag_V = motorParams->RatedVoltage_V;

    objUser->angleDelayed_sf_sec = (float32_t)0.5f * USER_M1_CTRL_PERIOD_sec;

    objUser->IdRated_A = 0.0;

    objUser->fluxExcFreq_Hz = INSTASPIN_EST_FLUX_EXC_FREQ_Hz;

    if (objUser->flag_bypassMotorId == true)
    {
        objUser->BWc_rps = MATH_TWO_PI * (float32_t)200.0f;
        objUser->BWdelta = motorParams->SpdBwRatio;

        objUser->Kctrl_Wb_p_kgm2 = (float32_t)3.0f * objUser->motor_numPolePairs * objUser->motor_ratedFlux_Wb /
                                   (float32_t)(2.0f * motorParams->Inertia_Kgm2);
    }
    else
    {
        objUser->BWc_rps = MATH_TWO_PI * (float32_t)200.0f;
        objUser->BWdelta = motorParams->SpdBwRatio;
        objUser->Kctrl_Wb_p_kgm2 =
            (float32_t)3.0f * objUser->motor_numPolePairs * (float32_t)(0.001f) / (float32_t)(2.0f * 0.000001f);
    }

    objUser->estWaitTime[EST_STATE_ERROR]  = 0;
    objUser->estWaitTime[EST_STATE_IDLE]   = 0;
    objUser->estWaitTime[EST_STATE_ROVERL] = (int_least32_t)(5.0 * USER_M1_ISR_FREQ_Hz);
    objUser->estWaitTime[EST_STATE_RS]     = 0;
    objUser->estWaitTime[EST_STATE_RAMPUP] =
        (int_least32_t)((INSTASPIN_EST_FLUX_EXC_FREQ_Hz / USER_M1_MAX_ACCEL_Hzps + (float32_t)1.0) *
                        USER_M1_ISR_FREQ_Hz);

    objUser->estWaitTime[EST_STATE_CONSTSPEED]      = (int_least32_t)(1.0 * USER_M1_ISR_FREQ_Hz);
    objUser->estWaitTime[EST_STATE_IDRATED]         = (int_least32_t)(20.0 * USER_M1_ISR_FREQ_Hz);
    objUser->estWaitTime[EST_STATE_RATEDFLUX_OL]    = (int_least32_t)(1.0 * USER_M1_ISR_FREQ_Hz);
    objUser->estWaitTime[EST_STATE_RATEDFLUX]       = 0;
    objUser->estWaitTime[EST_STATE_RAMPDOWN]        = (int_least32_t)(0.0 * USER_M1_ISR_FREQ_Hz);
    objUser->estWaitTime[EST_STATE_LOCKROTOR]       = 0;
    objUser->estWaitTime[EST_STATE_LS]              = 0;
    objUser->estWaitTime[EST_STATE_RR]              = (int_least32_t)(5.0 * USER_M1_ISR_FREQ_Hz);
    objUser->estWaitTime[EST_STATE_MOTORIDENTIFIED] = 0;
    objUser->estWaitTime[EST_STATE_ONLINE]          = 0;

    objUser->RsWaitTime[EST_RS_STATE_ERROR]  = 0;
    objUser->RsWaitTime[EST_RS_STATE_IDLE]   = 0;
    objUser->RsWaitTime[EST_RS_STATE_RAMPUP] = (int_least32_t)(1.0 * USER_M1_ISR_FREQ_Hz);
    objUser->RsWaitTime[EST_RS_STATE_COARSE] = (int_least32_t)(2.0 * USER_M1_ISR_FREQ_Hz);
    objUser->RsWaitTime[EST_RS_STATE_FINE]   = (int_least32_t)(10.0 * USER_M1_ISR_FREQ_Hz);
    objUser->RsWaitTime[EST_RS_STATE_DONE]   = 0;

    // Full Fast Library
    objUser->RrWaitTime[EST_RR_STATE_ERROR]  = 0;
    objUser->RrWaitTime[EST_RR_STATE_IDLE]   = 0;
    objUser->RrWaitTime[EST_RR_STATE_RAMPUP] = (int_least32_t)(1.0 * USER_M1_ISR_FREQ_Hz);
    objUser->RrWaitTime[EST_RR_STATE_COARSE] = (int_least32_t)(10.0 * USER_M1_ISR_FREQ_Hz);
    objUser->RrWaitTime[EST_RR_STATE_FINE]   = (int_least32_t)(30.0 * USER_M1_ISR_FREQ_Hz);
    objUser->RrWaitTime[EST_RR_STATE_DONE]   = 0;
    // End of Full Fast Library

    objUser->FluxWaitTime[EST_FLUX_STATE_ERROR] = 0;
    objUser->FluxWaitTime[EST_FLUX_STATE_IDLE]  = 0;
    objUser->FluxWaitTime[EST_FLUX_STATE_CL1]   = (int_least32_t)(10.0 * USER_M1_ISR_FREQ_Hz);
    objUser->FluxWaitTime[EST_FLUX_STATE_CL2]   = (int_least32_t)(0.2 * USER_M1_ISR_FREQ_Hz);
    objUser->FluxWaitTime[EST_FLUX_STATE_FINE]  = (int_least32_t)(20.0 * USER_M1_ISR_FREQ_Hz);
    objUser->FluxWaitTime[EST_FLUX_STATE_DONE]  = 0;

    objUser->LsWaitTime[EST_LS_STATE_ERROR]  = 0;
    objUser->LsWaitTime[EST_LS_STATE_IDLE]   = 0;
    objUser->LsWaitTime[EST_LS_STATE_RAMPUP] = (int_least32_t)(10.0 * USER_M1_ISR_FREQ_Hz);
    objUser->LsWaitTime[EST_LS_STATE_COARSE] = (int_least32_t)(30.0 * USER_M1_ISR_FREQ_Hz);
    objUser->LsWaitTime[EST_LS_STATE_FINE]   = (int_least32_t)(30.0 * USER_M1_ISR_FREQ_Hz);
    objUser->LsWaitTime[EST_LS_STATE_DONE]   = 0;

    objUser->RoverL_excFreq_Hz   = USER_M1_R_OVER_L_EXC_FREQ_Hz;
    objUser->maxCurrent_indEst_A = INSTASPIN_EST_IND_CURRENT_A;

    objUser->ctrlFreq_Hz = USER_M1_ISR_FREQ_Hz;

    objUser->estFreq_Hz = USER_M1_ISR_FREQ_Hz;

    objUser->trajFreq_Hz = USER_M1_ISR_FREQ_Hz;

    objUser->ctrlPeriod_sec = USER_M1_CTRL_PERIOD_sec;

    objUser->maxAccel_Hzps = USER_M1_MAX_ACCEL_Hzps;

    objUser->maxCurrent_resEst_A = INSTASPIN_EST_RES_CURRENT_A;
    objUser->maxCurrentDelta_A   = INSTASPIN_EST_MAX_CURRENT_DELTA_A;

    // Full Fast Library
    objUser->maxCurrentDelta_pw_A = INSTASPIN_EST_MAX_CURRENT_DELTA_PW_A;
    objUser->IdRated_delta_A      = USER_M1_IDRATED_DELTA_A;
    // End of Full Fast Library

    objUser->forceAngleFreq_Hz = USER_M1_FORCE_ANGLE_FREQ_Hz;

    // Full Fast Library
    objUser->forceAngleAccel_Hzps = USER_M1_FORCE_ANGLE_ACCEL_Hzps;
    // End of Full Fast Library

    objUser->indEst_speedMaxFraction = USER_M1_SPEEDMAX_FRACTION_FOR_L_IDENT;
    objUser->Kp_min_VpA              = (float32_t)0.001f;
    objUser->Kp_max_VpA              = (float32_t)1000.0f;
    objUser->RoverL_Kp_sf            = USER_M1_R_OVER_L_KP_SF;
    objUser->RoverL_min_rps          = MATH_TWO_PI * (float32_t)5.0f;
    objUser->RoverL_max_rps          = MATH_TWO_PI * (float32_t)5000.0f;
    objUser->oneOverDcBus_min_invV   = (float32_t)1.0f / (float32_t)400.0f;
    objUser->oneOverDcBus_max_invV   = (float32_t)1.0f / (float32_t)10.0f;
    objUser->Rs_Ohm                  = (float32_t)0.0;
    objUser->Rs_coarseDelta_Ohm      = (float32_t)0.01f;
    objUser->Rs_fineDelta_Ohm        = (float32_t)0.00001f;
    objUser->Rs_min_Ohm              = (float32_t)0.001f;
    objUser->Rs_max_Ohm              = (float32_t)1000.0f;

    // Full Fast Library
    objUser->IdRatedFraction_indEst = USER_M1_IDRATED_FRACTION_FOR_L_IDENT;
    objUser->pwGain                 = USER_M1_PW_GAIN;
    objUser->Rr_Ohm                 = (float32_t)0.0;
    objUser->Rr_coarseDelta_Ohm     = (float32_t)0.0001f;
    objUser->Rr_fineDelta_Ohm       = (float32_t)0.00001f;
    objUser->Rr_min_Ohm             = (float32_t)0.0f;
    objUser->Rr_max_Ohm             = (float32_t)1000.0f;
    // End of Full Fast Library

    objUser->Ls_d_H                  = (float32_t)1.0e-6;
    objUser->Ls_q_H                  = (float32_t)1.0e-6;
    objUser->Ls_coarseDelta_H        = (float32_t)0.0000001f;
    objUser->Ls_fineDelta_H          = (float32_t)0.00000001f;
    objUser->Ls_min_H                = (float32_t)0.000001f;
    objUser->Ls_max_H                = (float32_t)100.0f;
    objUser->RsOnLine_DeltaInc_Ohm   = (float32_t)0.00002f;
    objUser->RsOnLine_DeltaDec_Ohm   = (float32_t)0.00002f;
    objUser->RsOnLine_min_Ohm        = (float32_t)0.001f;
    objUser->RsOnLine_max_Ohm        = (float32_t)1000.0f;
    objUser->RsOnLine_angleDelta_rad = (float32_t)0.0005f;             // unit=rad
    objUser->RsOnLine_pole_rps       = MATH_TWO_PI * (float32_t)0.1f;  // 0.1Hz

    objUser->freqNearZeroSpeedLimit_Hz = INSTASPIN_EST_FREQ_NEARZEROLIMIT_Hz;

    objUser->maxFrequency_Hz = motorParams->RatedSpeed_Hz;

    return;
}
// #TAG_FAST_OBS_END#

//=============================================================================
// Motor 1 Control Parameter Initialization
//=============================================================================

//*****************************************************************************
//
// initMotor1Handles - Initialize motor control handles and HAL interface
//
//*****************************************************************************
void initMotor1Handles(MOTOR_Handle handle)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)handle;

    obj->statusVars.motorNum = MTR_1;

    //    motorHandles[obj->inputCtrl.motorNum] = obj;

    obj->halInterface->halFOCDataHandle->gdDataHandle = obj->gateDriveHandle->gdDataHandle;

    HAL_Init(obj->halInterface);

    obj->motorSetsHandle = &motorSetVars_M1;

    obj->userParamsHandle            = &userParams_M1;
    // false - enables identification, true - disables identification
    userParams_M1.flag_bypassMotorId = true;

    // set the control parameters for motor 1
    motorHandle_M1 = (MOTOR_Handle)(obj);

    // set the reference speed, this can be replaced or removed
    obj->inputCtrl.speedRef_Hz = 40.0f;

    /* Hall sensor init — only runs when halSensorInterface is non-NULL.
     * The hall project's mtr1_init.c wires halSensorInterface = &halSensorInterface
     * at static init time. Sensorless projects set halSensorInterface = NULL_PTR.
     * No #if guard needed — the NULL check is the runtime discriminator. */
    if (obj->halSensorInterface != NULL)
    {
        HAL_Hall_Config_t *hallCfg  = (HAL_Hall_Config_t *)(obj->halSensorInterface->sensorConfig);
        HAL_Hall_Data_t   *hallData = (HAL_Hall_Data_t *)(obj->halSensorInterface->sensorData);
        HAL_Hall_Init(hallCfg, hallData);
    }

    return;
}

//*****************************************************************************
//
// initMotor1CtrlParameters - Initialize all control parameters for Motor 1
// This includes:
// - USER parameter initialization
// - Motor settings configuration
// - Core FOC object initialization (Clarke, Park, PI, SVGEN, etc.)
// - Optional feature initialization (MTPA, VOLREC, DTC, IPD, ESMO)
// - EST estimator setup
//
//*****************************************************************************
void initMotor1CtrlParameters(MOTOR_Handle handle, const Motor_ParamsType *motorParams)
{
    MOTOR_Vars_t          *obj     = (MOTOR_Vars_t *)handle;
    MOTOR_SetVars_t       *objSets = (MOTOR_SetVars_t *)(handle->motorSetsHandle);
    const HAL_GD_DATA_Obj *gdObj   = (HAL_GD_DATA_Obj *)(obj->gateDriveHandle->gdDataHandle);

    USER_Params *objUser = (USER_Params *)(handle->userParamsHandle);

    // Initialize Motor 1 specific parameters
    USER_setMotor1Params(obj->userParamsHandle, obj->gateDriveHandle->gdDataHandle, motorParams);

    // #TAG_FAST_OBS_START#
    // Initialize the user parameters (FAST estimator-specific fields)
    USER_setParams_priv(obj->userParamsHandle);
    // #TAG_FAST_OBS_END#

    // Turn on the gate driver if present
    HAL_enableGateDriver(obj->gateDriveHandle);

    //=========================================================================
    // Motor Settings Configuration
    //=========================================================================

    objSets->Kp_spd = 0.05f;
    objSets->Ki_spd = 0.005f;

    objSets->Kp_fwc = USER_M1_FWC_KP;
    objSets->Ki_fwc = USER_M1_FWC_KI;

    objSets->angleFWCMax_rad = USER_M1_FWC_MAX_ANGLE_RAD;
    objSets->overModulation  = USER_M1_MAX_VS_MAG_PU;

    objSets->RsOnLineCurrent_A = 0.1f * motorParams->RatedCurrent_A;

    objSets->lostPhaseSet_A    = USER_M1_LOST_PHASE_CURRENT_A;
    objSets->unbalanceRatioSet = USER_M1_UNBALANCE_RATIO;
    objSets->overLoadSet_W     = USER_M1_OVER_LOAD_POWER_W;

    objSets->toqueFailMinSet_Nm = USER_M1_TORQUE_FAILED_SET;
    objSets->speedFailMaxSet_Hz = USER_M1_FAIL_SPEED_MAX_HZ;
    objSets->speedFailMinSet_Hz = USER_M1_FAIL_SPEED_MIN_HZ;

    objSets->stallCurrentSet_A = USER_M1_STALL_CURRENT_A;
    objSets->IsFailedChekSet_A = USER_M1_FAULT_CHECK_CURRENT_A;

    objSets->overCurrent_A = motorParams->OverCurrentLim_A;

    objSets->overVoltageFault_V  = gdObj->gdOverVolVal;
    objSets->overVoltageNorm_V   = gdObj->gdOverVolVal * 0.9;
    objSets->underVoltageFault_V = gdObj->gdUnderVolVal;
    objSets->underVoltageNorm_V  = gdObj->gdUnderVolVal * 1.1;

    objSets->overCurrentTimesSet = USER_M1_OVER_CURRENT_TIMES_SET;
    objSets->voltageFaultTimeSet = USER_M1_VOLTAGE_FAULT_TIME_SET;
    objSets->motorStallTimeSet   = USER_M1_STALL_TIME_SET;
    objSets->startupFailTimeSet  = USER_M1_STARTUP_FAIL_TIME_SET;

    objSets->overSpeedTimeSet = USER_M1_OVER_SPEED_TIME_SET;
    objSets->overLoadTimeSet  = USER_M1_OVER_LOAD_TIME_SET;
    objSets->unbalanceTimeSet = USER_M1_UNBALANCE_TIME_SET;
    objSets->lostPhaseTimeSet = USER_M1_LOST_PHASE_TIME_SET;

    objSets->stopWaitTimeSet    = USER_M1_STOP_WAIT_TIME_SET;
    objSets->restartWaitTimeSet = USER_M1_RESTART_WAIT_TIME_SET;
    objSets->restartTimesSet    = USER_M1_START_TIMES_SET;

    //=========================================================================
    // HAL Interface Configuration
    //=========================================================================

    obj->halInterface->halFOCDataHandle->pOverCurLimA            = &objSets->overCurrent_A;
    obj->halInterface->halFOCDataHandle->adcData.current_sf      = objUser->current_sf * gdObj->gdCurrSign;
    obj->halInterface->halFOCDataHandle->pwmData.pVab_out_V      = &obj->outVars.Vab_out_V;
    obj->halInterface->halFOCDataHandle->adcData.voltage_sf      = objUser->voltage_sf;
    obj->halInterface->halFOCDataHandle->adcData.dcBusvoltage_sf = objUser->voltage_sf;

    obj->halFOCData = obj->halInterface->halFOCDataHandle;
    obj->adcData    = &obj->halFOCData->adcData;
    obj->pwmData    = &obj->halFOCData->pwmData;
    obj->pwmVabc_pu = &obj->pwmData->Vabc_pu;

    //=========================================================================
    // Motor Control Parameters
    //=========================================================================

    objSets->olSpeedThr          = motorParams->OlSpeedThr_Hz;
    objSets->speedFlyingStart_Hz = motorParams->FlyingStartMinSpeed_Hz;

    objSets->accelerationMax_Hzps   = motorParams->ClAccelRate_Hzps;
    objSets->accelerationStart_Hzps = motorParams->OlAccelRate_Hzps;

    obj->VsRef_pu = 0.98f * USER_M1_MAX_VS_MAG_PU;

    objSets->alignCurrent_A = motorParams->AlignCurrent_A;
    objSets->startCurrent_A = motorParams->OlTorqueCurrent_A;
    objSets->maxCurrent_A   = USER_M1_BENCH_MAX_CURRENT_A;

    obj->angleDelayed_sf = 0.5f * MATH_TWO_PI * USER_M1_CTRL_PERIOD_sec;

    obj->power_sf                 = MATH_TWO_PI / (float32_t)motorParams->NumPolePairs;
    obj->statusVars.VIrmsIsrScale = objUser->ctrlFreq_Hz;

    obj->statusVars.stopWaitTimeCnt  = 0;
    obj->inputCtrl.flagEnableRestart = false;

    obj->statusVars.faultMtrMask.all = MTR1_FAULT_MASK_SET;
    obj->inputCtrl.operateMode       = OPERATE_MODE_SPEED;

    objSets->flyingStartTimeDelay_cnt = (uint32_t)(objUser->ctrlFreq_Hz * motorParams->FlyingStartDelay_sec);

    //=========================================================================
    // SVM Mode Configuration
    //=========================================================================

    if (objUser->flag_bypassMotorId == true)
    {
        if (obj->halInterface->halFOCDataHandle->currentSenseType == CURSEN_TYPE_SINGLE_SHUNT)
        {
            obj->inputCtrl.svmMode = SVM_COM_C;
        }
        else
        {
            obj->inputCtrl.svmMode = SVM_COM_C;
        }
        obj->inputCtrl.flagEnableFWC = true;
    }
    else
    {
        obj->inputCtrl.svmMode       = SVM_COM_C;
        obj->inputCtrl.flagEnableFWC = false;
    }

    obj->statusVars.flagEnableForceAngle = false;
    obj->inputCtrl.flagEnableFlyingStart = true;

    obj->statusVars.flagEnableSpeedCtrl   = true;
    obj->statusVars.flagEnableCurrentCtrl = true;

    // obj->IsSet_A = 0.0f;

    obj->statusVars.estState  = EST_STATE_IDLE;
    obj->statusVars.trajState = EST_TRAJ_STATE_IDLE;

    obj->inputCtrl.estimatorMode = ESTIMATOR_MODE_FAST;

    obj->inputCtrl.flagEnableAlignment = true;

    objSets->alignTimeDelay_cnt    = (uint32_t)(objUser->ctrlFreq_Hz * motorParams->AlignDelay_sec);
    obj->angleAlign_rad            = 0.0;
    objSets->forceRunTimeDelay_cnt = (uint32_t)(objUser->ctrlFreq_Hz * motorParams->ForceRunDelay_sec);

    objSets->fwcTimeDelay_cnt = (uint32_t)(objUser->ctrlFreq_Hz * motorParams->FwcDelay_sec);

    objSets->offsetCalibTimeSet_cnt = (uint32_t)(objUser->ctrlFreq_Hz * USER_M1_OFFSET_CALIB_TIME_SEC);

    //=========================================================================
    // Core FOC Object Initialization (Embedded Objects)
    //=========================================================================

    // Initialize the FWC PI controller (embedded object)
    PI_init(&obj->pi_fwc, sizeof(obj->pi_fwc));

    // Set the FWC controller
    PI_setGains(&obj->pi_fwc, USER_M1_FWC_KP, USER_M1_FWC_KI);
    PI_setUi(&obj->pi_fwc, 0.0);
    PI_setMinMax(&obj->pi_fwc, USER_M1_FWC_MAX_ANGLE_RAD, USER_M1_FWC_MIN_ANGLE_RAD);

    // Initialize the angle generate module (embedded object)
    ANGLE_GEN_init(&obj->angleGen, sizeof(obj->angleGen));
    ANGLE_GEN_setParams(&obj->angleGen, objUser->ctrlPeriod_sec);

    // Initialize the inverse Park module (embedded object)
    IPARK_init(&obj->ipark_V, sizeof(obj->ipark_V));

    // Initialize the Park modules (embedded objects)
    PARK_init(&obj->park_I, sizeof(obj->park_I));
    PARK_init(&obj->park_V, sizeof(obj->park_V));

    // Initialize the PI controllers (embedded objects)
    PI_init(&obj->pi_Id, sizeof(obj->pi_Id));
    PI_init(&obj->pi_Iq, sizeof(obj->pi_Iq));
    PI_init(&obj->pi_spd, sizeof(obj->pi_spd));

    // Initialize the speed reference trajectory (embedded object)
    TRAJ_init(&obj->traj_spd, sizeof(obj->traj_spd));

    // Configure the speed reference trajectory (Hz)
    TRAJ_setTargetValue(&obj->traj_spd, 0.0f);
    TRAJ_setIntValue(&obj->traj_spd, 0.0f);
    TRAJ_setMinValue(&obj->traj_spd, -objUser->maxFrequency_Hz);
    TRAJ_setMaxValue(&obj->traj_spd, objUser->maxFrequency_Hz);
    TRAJ_setMaxDelta(&obj->traj_spd, (objUser->maxAccel_Hzps * objUser->ctrlPeriod_sec));

    // Initialize the space vector generator module (embedded object)
    obj->inputCtrl.svmMode = SVM_COM_C;

    // Initialize the spdfr (embedded object)
    SPDFR_init(&obj->spdfr, sizeof(obj->spdfr));
    SPDFR_setParams(&obj->spdfr, obj->userParamsHandle);

    obj->frswPos_sf = 0.5f;

    //=========================================================================
    // HAL Trigger Parameters
    //=========================================================================

    HAL_setTriggerParams(&obj->halInterface->halFOCDataHandle->pwmData, USER_SYSTEM_FREQ_MHz, SYS_M1_PWM_DEADTIME_uS,
                         SYS_M1_ADC_NOISE_WIN_uS, SYS_M1_ADC_SAMPTIME_uS);

    //=========================================================================
    // Optional Feature: MTPA (Maximum Torque Per Ampere)
    //=========================================================================
    // #TAG_MTPA_START#
    // Initialize the Maximum torque per ampere (MTPA) handle
    obj->mtpaHandle = MTPA_init(&mtpa_M1, sizeof(MTPA_Obj));

    // Compute the motor constant for MTPA
    MTPA_computeParameters(obj->mtpaHandle, objUser->motor_Ls_d_H, objUser->motor_Ls_q_H, objUser->motor_ratedFlux_Wb);
    // #TAG_MTPA_END#

    // #TAG_READ_INV_VOLT_START#
    //=========================================================================
    // Optional Feature: VOLREC (Voltage Reconstruction)
    //=========================================================================

    // Initialize the Voltage reconstruction handle
    obj->volrecHandle = VOLREC_init(&volrec_M1, sizeof(VOLREC_Obj));

    // Configure the Voltage reconstruction
    VOLREC_setParams(obj->volrecHandle, objUser->voltageFilterPole_rps, objUser->ctrlFreq_Hz);

    VOLREC_disableFlagEnableSf(obj->volrecHandle);

    // #TAG_READ_INV_VOLT_END#

    // #TAG_ESMO_START#
    //=========================================================================
    // Optional Feature: ESMO (Enhanced Sliding Mode Observer)
    //=========================================================================

    // Initialize the esmo handle
    obj->esmoHandle = ESMO_init(&esmo_M1, sizeof(ESMO_Obj));

    // Set parameters for ESMO controller
    ESMO_setKslideParams(obj->esmoHandle, motorParams->EsmoKslideMax, motorParams->EsmoKslideMin);

    ESMO_setPLLParams(obj->esmoHandle, motorParams->EsmoPllKpMax, motorParams->EsmoPllKpMin, motorParams->EsmoPllKpSf);

    ESMO_setPLLKi(obj->esmoHandle, motorParams->EsmoPllKi);  // Optional

    ESMO_setBEMFThreshold(obj->esmoHandle, motorParams->EsmoBemfThreshold);
    ESMO_setOffsetCoef(obj->esmoHandle, motorParams->EsmoThetaOffsetSf);
    ESMO_setBEMFKslfFreq(obj->esmoHandle, motorParams->EsmoBemfKslfFcSf);
    ESMO_setSpeedFilterFreq(obj->esmoHandle, motorParams->EsmoSpeedLpfFc_Hz);

    // Set the ESMO controller parameters
    ESMO_setParams(obj->esmoHandle, obj->userParamsHandle);

    // #TAG_ESMO_END#

    // #TAG_FAST_OBS_START#
    //=========================================================================
    //  EST Estimator Initialization
    //=========================================================================

    // Initialize the estimator
    obj->estHandle = EST_initEst(MTR_1);

    // Set the default estimator parameters
    EST_setParams(obj->estHandle, obj->userParamsHandle);
    EST_setFlag_enableForceAngle(obj->estHandle, obj->statusVars.flagEnableForceAngle);
    EST_setFlag_enableRsRecalc(obj->estHandle, obj->inputCtrl.flagEnableRsRecalc);

    // Set the scale factor for high frequency motor
    EST_setOneOverFluxGain_sf(obj->estHandle, obj->userParamsHandle, USER_M1_EST_FLUX_HF_SF);
    EST_setFreqLFP_sf(obj->estHandle, obj->userParamsHandle, USER_M1_EST_FREQ_HF_SF);
    EST_setBemf_sf(obj->estHandle, obj->userParamsHandle, USER_M1_EST_BEMF_HF_SF);

    objSets->Ls_d_comp_H = EST_getLs_d_H(obj->estHandle);
    objSets->Ls_q_comp_H = EST_getLs_q_H(obj->estHandle);

    objSets->Ls_d_Icomp_coef = INSTASPIN_EST_Ls_d_COMP_COEF / objSets->maxCurrent_A;
    objSets->Ls_q_Icomp_coef = INSTASPIN_EST_Ls_q_COMP_COEF / objSets->maxCurrent_A;

    objSets->Ls_min_H = objSets->Ls_d_comp_H * INSTASPIN_EST_Ls_MIN_NUM_COEF;

    obj->inputCtrl.flagEnableLsUpdate = false;

    //=========================================================================
    // Rs Recalculation and Online Calibration
    //=========================================================================

    // For Rs re-calculation
    obj->inputCtrl.flagEnableRsRecalc = false;

    // For Rs online calibration
    obj->statusVars.flagRsOnLineContinue = false;
    obj->inputCtrl.flagStartRsOnLine     = false;

    objSets->RsOnlineWaitTimeSet = INSTASPIN_EST_RSONLINE_WAIT_TIME_MS;
    objSets->RsOnlineWorkTimeSet = INSTASPIN_EST_RSONLINE_WORK_TIME_MS;

    // #TAG_FAST_OBS_END#

    //=========================================================================
    // Braking Configuration
    //=========================================================================

    obj->inputCtrl.flagEnableBraking = false;
    obj->inputCtrl.brakingMode       = HARDSWITCH_BRAKE_MODE;

    // #TAG_IPD_START#
    //=========================================================================
    // Optional Feature: IPD (Initial Position Detection)
    //=========================================================================

    obj->inputCtrl.startupMode = STARTUP_HALL;

    // Initialize the IPD module handle
    obj->ipdHandle = IPD_memInit(&ipd_M1, sizeof(IPD_Obj));

    obj->ipdHandle->halInterface = obj->halInterface;

    obj->ipdHandle->halInterface->halFOCDataHandle->ipdCurrentLimit = motorParams->IpdPulseCurrent_A;

    objSets->ipdCycleCountMax     = motorParams->IpdCycleCountMax;
    obj->ipdHandle->cycleCountMax = objSets->ipdCycleCountMax;

    obj->ipdHandle->flags.b.highResAngleEnable = 1;

    obj->ipdHandle->noMotorFaultCount = (uint32_t)(objUser->ctrlFreq_Hz * motorParams->IpdTimeoutFault_sec);

    // #TAG_IPD_END#

    /* open Loop Configurations */

    obj->angleOLStep_rad = (motorParams->OlThetaRampRate_dps * MATH_TWO_PI) / (360.0f * objUser->ctrlFreq_Hz);

    /* End of open Loop Configurations */

    // #TAG_HALL_CAL_START#
    /*
     * Wires the embedded sample buffer into hallSensorDataonverts ms timing
     * to ISR ticks using ctrlPeriod_sec, and sets state = HALL_CAL_IDLE.
     * Default config values are set here; tune alignHoldTime_ms,
     * ditherHoldTime_ms, stepSize_rad, and numMechCycles as needed. */

    /* Get motor parameters */
    const Motor_ParamsType *motorParamsPtr = motorParams_M1;

    /* Initialize Hall calibration config from motor parameters */
    hallSensorConfig.alignCurrent_A     = motorParamsPtr->HallCalAlignCurrent_A;
    hallSensorConfig.calCurrent_A       = motorParamsPtr->HallCalSweepCurrent_A;
    hallSensorConfig.stepSize_rad       = motorParamsPtr->HallCalStepSize_rad;
    hallSensorConfig.ditherFwdAngle_rad = (float32_t)(MATH_PI / 2.0 + MATH_PI / 180.0); /* 91° */
    hallSensorConfig.ditherRevAngle_rad = (float32_t)(MATH_PI / 180.0);                 /* 90° */
    hallSensorConfig.alignHoldTime_ms   = 400.0f;
    hallSensorConfig.ditherHoldTime_ms  = 200.0f;
    hallSensorConfig.numMechCycles      = 1U;

    hallSensorData.statesRequired =
        6 * hallSensorConfig.numMechCycles *
        (uint16_t)((HAL_Hall_Config_t *)(obj->halSensorInterface->sensorConfig))->numPolePairs;

    /* Initialize Hall calibration */
    HallCal_init(&hallSensorConfig, &hallSensorData, obj->userParamsHandle->ctrlPeriod_sec * 1000000.0f); /* sec → µs */

    // #TAG_HALL_CAL_END#
    //=========================================================================
    // Setup Controllers
    //=========================================================================

    // Setup the controllers, speed, d/q-axis current pid regulator
    setupControllers(handle);

    //=========================================================================
    // Disable PWM
    //=========================================================================

    // Disable the PWM
    HAL_disablePWM(&obj->halInterface->halFOCConfigHandle->pwmCfg, &obj->halInterface->halFOCDataHandle->pwmData);

    // #TAG_DEAD_TIME_COMP_START#
    //=========================================================================
    // Optional Feature: DTC (Dead Time Compensation)
    //=========================================================================

    // Get the DTC handle for Motor 1 instance
    // Memory is allocated internally by the library - no user allocation needed
    obj->deadTimeCompHandle = DTC_getInstance(DTC_MOTOR_1);

    // Configure DTC parameters using the opaque API
    {
        DTC_InitParams_t dtcParams;

        // Set default parameters first
        DTC_setDefaultParams(&dtcParams);

        // Configure DTC module parameters (gains are set separately)
        dtcParams.maxOutFactor       = DTC_DEFAULT_MAX_OUT_FACTOR;
        obj->inputCtrl.flagEnableDTC = false;
        dtcParams.enableDTC          = obj->inputCtrl.flagEnableDTC;

        // Apply parameters to DTC module
        DTC_initParams(obj->deadTimeCompHandle, &dtcParams);

        // Update compensation gains from current controller Ki values
        // The compensation gain is calculated as: Ki * scaleFactor
        DTC_updateGains(obj->deadTimeCompHandle, objSets->Ki_Id, objSets->Ki_Iq, DTC_DEFAULT_GAIN_ID_SF,
                        DTC_DEFAULT_GAIN_IQ_SF);
    }

    // #TAG_DEAD_TIME_COMP_END#
    // #TAG_TI_TUNE_START#
    obj->tiTuneHandle = TiTune_init(&TiTune_M1, sizeof(TiTune_M1));

    TiTune_setParams(obj->tiTuneHandle);
    // #TAG_TI_TUNE_END#

    return;
}

//=============================================================================
// Motor 1 — Low-priority parameter re-initialisation
//=============================================================================

/*
 * Flag polled by FOC_runLowPriorityStateMachine().
 * Set to TRUE from the GUI or debugger (while motor is stopped) to trigger a
 * full re-application of motorParams_M1 without a system reset.
 */
volatile bool flagReInitMotorParams = false;

/*
 * Re-apply all Motor 1 control parameters from the active motor constant table.
 * Called by FOC_runLowPriorityStateMachine() when flagReInitMotorParams is TRUE.
 * Clears the flag on completion.
 */
void Motor_Params_ReInit(void)
{
    if (flagReInitMotorParams == true)
    {
        initMotor1CtrlParameters((MOTOR_Handle)motorHandle_M1, motorParams_M1);
        flagReInitMotorParams = false;
    }

    return;
}
