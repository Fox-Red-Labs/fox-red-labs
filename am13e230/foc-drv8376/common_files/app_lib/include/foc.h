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
 * \file foc.h
 * \brief Field-Oriented Control (FOC) implementation
 *
 * \defgroup FOC_MODULE FOC Module
 * \brief Field-Oriented Control algorithms for motor control
 * @{
 */

#ifndef FOC_H
#define FOC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ARM M33 math library with TMU support
 * Provides __sin, __cos, __sqrt macros mapped to TMU-accelerated functions
 * math_types.h internally includes <math.h> and fastrts_tmu.h for TMU acceleration
 */
#include "math_types.h"
#include "motor_common.h"
#include "frl_guard.h"  /* FRL port: the hold test hooks (revision H5) */

/* ============================================================================
 * SECTION 1: FOC FEEDBACK - Input Acquisition and Transforms
 * ============================================================================ */

/*
 * Acquires sensor data and performs feedback transforms (Clarke on currents/voltages)
 *
 * This function handles:
 * - ADC interrupt acknowledgment
 * - Three-phase current sensing
 * - DC bus and phase voltage sensing
 * - Voltage reconstruction (if phase sensing unavailable)
 * - Clarke transform on currents (abc -> alpha-beta)
 */
static inline void FOC_runFeedback(MOTOR_Vars_t *obj)
{
    const HAL_FOC_Interface_t *const halInterface = obj->halInterface;

    // Read three-phase currents
    HAL_ReadPhaseCurrents(halInterface);

    // #TAG_FAST_OBS_START#

    // #TAG_READ_INV_VOLT_START#

    // Read DC bus voltage and three-phase voltages
    HAL_ReadInverterVoltages(halInterface);

    // #TAG_READ_INV_VOLT_END#

    // #TAG_RECONST_INV_VOLT_START#

    // Reconstruct voltage if phase voltage sensing is not available

    VOLREC_run(obj->volrecHandle, obj->adcData->VdcBus_V, obj->pwmVabc_pu, &(obj->estInputData.Vab_V));

    // #TAG_RECONST_INV_VOLT_END#
    // #TAG_FAST_OBS_END#

    // Clarke transform on currents (abc -> alpha-beta)
    CLARKE_run_threeInput(obj->adcData->I_A.value[0], obj->adcData->I_A.value[1], obj->adcData->I_A.value[2],
                          &obj->estInputData.Iab_A.value[0], &obj->estInputData.Iab_A.value[1]);

    // CLARKE_run_threeInput(obj->adcData->V_V.value[0], obj->adcData->V_V.value[1], obj->adcData->V_V.value[2],
    //                       &obj->estInputData.Vab_V.value[0], &obj->estInputData.Vab_V.value[1]);

    obj->oneOverDcBus_invV = 1.0f / obj->adcData->VdcBus_V;
}

/* ============================================================================
 * SECTION 2: OBSERVER/ESTIMATOR - Angle and Speed Estimation
 * ============================================================================ */

// #TAG_FAST_OBS_START#
/* Runs FAST estimator for angle and speed estimation */
static inline void FOC_runFASTEstimator(MOTOR_Vars_t *obj)
{
    USER_Params *objUser = (USER_Params *)(obj->userParamsHandle);

    obj->estInputData.dcBus_V = obj->adcData->VdcBus_V;

    obj->estInputData.speed_ref_Hz = obj->statusVars.speed_int_Hz;

    // Run FAST estimator
    EST_run(obj->estHandle, &obj->estInputData, &obj->estOutputData);

    // Compute angle with delay compensation
    obj->angleDelta_rad          = objUser->angleDelayed_sf_sec * obj->estOutputData.fm_lp_rps;
    obj->statusVars.angleEST_rad = MATH_incrAngle(obj->estOutputData.angle_rad, obj->angleDelta_rad);

    // Get estimated speed
    obj->statusVars.speedEST_Hz = EST_getFm_lp_Hz(obj->estHandle);
    //    obj->oneOverDcBus_invV = obj->estOutputData.oneOverDcBus_invV;

    obj->outVars.speed_Hz        = obj->statusVars.speedEST_Hz;
    obj->statusVars.angleObs_rad = obj->statusVars.angleEST_rad;

    obj->IdRated_A = EST_getIdRated_A(obj->estHandle);
}
// #TAG_FAST_OBS_END#

// #TAG_ESMO_START#
/* Runs ESMO (Enhanced Sliding Mode Observer) for sensorless control */
static inline void FOC_runESMOEstimator(MOTOR_Vars_t *obj)
{
    // Set speed reference for ESMO
    ESMO_setSpeedRef(obj->esmoHandle, obj->statusVars.speedPLL_Hz);

    ESMO_run(obj->esmoHandle, obj->adcData->VdcBus_V, obj->pwmVabc_pu, &(obj->estInputData.Iab_A));

    // Compute PLL angle with delay compensation
    obj->anglePLLComp_rad        = obj->statusVars.speedPLL_Hz * obj->angleDelayed_sf;
    obj->statusVars.anglePLL_rad = MATH_incrAngle(ESMO_getAnglePLL(obj->esmoHandle), obj->anglePLLComp_rad);

    // Run speed calculation from PLL angle
    SPDFR_run(&obj->spdfr, obj->statusVars.anglePLL_rad);
    obj->statusVars.speedPLL_Hz = SPDFR_getSpeedHz(&obj->spdfr);

    obj->outVars.speed_Hz        = obj->esmoHandle->speedFlt * obj->userParamsHandle->maxFrequency_Hz;
    obj->statusVars.angleObs_rad = obj->statusVars.anglePLL_rad;
}
// #TAG_ESMO_END#
// #TAG_FLUX_OBS_START#
/* Runs Flux Observer for sensorless control
 *
 * All speed signals are in physical units (rad/s):
 *
 *  velocity (rad/s)  →  [speedFiltConst IIR, inside FLUX_OBS_run]  →  velocityFilt / speedFlt (rad/s)
 *  speedFlt (rad/s)  →  [LPF lpf_b0/lpf_a1, computed here]         →  speedEst (rad/s, doubly filtered)
 *  speedEst (rad/s)  →  FLUX_OBS_setFilteredRefSpeed() next cycle   →  bemfFiltConst
 *
 * Voltage input: unfiltered Vαβ computed directly from pwmVabc_pu × VdcBus_V.
 * VOLREC_run() applies a biquad IIR filter to the phase voltages before Clarke
 * transform — that filtered output is unsuitable for the flux observer which
 * needs the instantaneous applied voltage.  Instead, Vαβ is computed here
 * using the same line-to-neutral formula as PHVOLC_run() but without filtering.
 */
static inline void FOC_runFluxObsEstimator(MOTOR_Vars_t *obj)
{
    /* Step A — Feed speed reference back into observer BEFORE run.
     * Used to compute adaptive bemfFiltConst inside the library. */
    FLUX_OBS_setFilteredRefSpeed(obj->fluxObsHandle, obj->inputCtrl.speedRef_Hz);

    /* Step B — Compute unfiltered Vab from PWM duty cycles x VdcBus.
     * Va = Vdc/3 * (2*da - db - dc),  Vb = Vdc/3 * (2*db - da - dc)
     * Va = Va,  Vb = (Va + 2*Vb) / sqrt(3)
     * This matches PHVOLC_run() exactly but avoids the VOLREC biquad filter. */
    {
        float32_t Vdc_third = obj->adcData->VdcBus_V * MATH_ONE_OVER_THREE;
        float32_t da        = obj->pwmVabc_pu->value[0];
        float32_t db        = obj->pwmVabc_pu->value[1];
        float32_t dc        = obj->pwmVabc_pu->value[2];

        float32_t Va = Vdc_third * (da * 2.0f - db - dc);
        float32_t Vb = Vdc_third * (db * 2.0f - da - dc);

        obj->estInputData.Vab_V.value[0] = Va;
        obj->estInputData.Vab_V.value[1] = (Va + Vb * 2.0f) * MATH_ONE_OVER_SQRT_THREE;
    }

    /* Step C — Run Flux Observer via public API.
     * Vab_V: unfiltered ab voltage (V) computed above.
     * Iab_A: ab current in physical amperes from Clarke transform.
     * Second-stage LPF (speedEst) is computed inside the library. */
    {
        FLUX_OBS_VoltageInput_t vabIn;
        vabIn.alpha_V = obj->estInputData.Vab_V.value[0];
        vabIn.beta_V  = obj->estInputData.Vab_V.value[1];

        FLUX_OBS_CurrentInput_t iabIn;
        iabIn.alpha_A = obj->estInputData.Iab_A.value[0];
        iabIn.beta_A  = obj->estInputData.Iab_A.value[1];

        FLUX_OBS_Output_t fluxObsOut;
        FLUX_OBS_run(obj->fluxObsHandle, &vabIn, &iabIn, &fluxObsOut);

        /* Step D — Export speed (Hz) and angle (rad) from observer output */
        obj->outVars.speed_Hz        = fluxObsOut.speed_Hz;
        obj->statusVars.angleObs_rad = fluxObsOut.angle_rad;
    }
}
// #TAG_FLUX_OBS_END#

/* Runs both estimators (FAST and ESMO) and manages speed references */
static inline void FOC_runObservers(MOTOR_Vars_t *obj)
{
    if (obj->inputCtrl.estimatorMode == ESTIMATOR_MODE_FAST)
    {
        // #TAG_FAST_OBS_START#
        // Start Tag used to identify FAST - Observer Code

        FOC_runFASTEstimator(obj);

        // End Tag used to identify FAST - Observer Code
        // #TAG_FAST_OBS_END#   End Tag used to identify FAST - Observer Code
    }
    // #TAG_ESMO_START#
    else if (obj->inputCtrl.estimatorMode == ESTIMATOR_MODE_ESMO)
    {
        FOC_runESMOEstimator(obj);
    }
    // #TAG_ESMO_END#
    // #TAG_FLUX_OBS_START#
    else if (obj->inputCtrl.estimatorMode == ESTIMATOR_MODE_FLUX_OBS)
    {
        FOC_runFluxObsEstimator(obj);
    }
    // #TAG_FLUX_OBS_END#
}

/* ============================================================================
 * SECTION 3: STATE MACHINE - Motor Control State Management
 * ============================================================================ */

/* Runs angle generation for open-loop control */
static inline void FOC_runAngleGeneration(MOTOR_Vars_t *obj)
{
    TRAJ_run(&obj->traj_spd);
    obj->statusVars.speed_int_Hz = TRAJ_getIntValue(&obj->traj_spd);

    // Ramp angle based on speed (used in open-loop)
    ANGLE_GEN_run(&obj->angleGen, obj->statusVars.speed_int_Hz);
    obj->angleGen_rad = ANGLE_GEN_getAngle(&obj->angleGen);
}

/* ============================================================================
 * SECTION 4: FOC FEEDFORWARD - Control and PWM Generation
 * ============================================================================ */

/*
 * Computes sin/cos phasor from FOC angle
 *
 * This function calculates sine and cosine of the FOC angle.
 * Computing it once and reusing for both Park and iPark eliminates redundant calculations.
 */
static inline void FOC_computePhasor(MOTOR_Vars_t *obj)
{
    // Compute sin/cos phasor for Park/iPark transforms
    obj->phasor.value[0] = __cos(obj->outVars.angleFOC_rad);
    obj->phasor.value[1] = __sin(obj->outVars.angleFOC_rad);
}

/* Runs Park transform on currents (alpha-beta -> dq) */
static inline void FOC_runParkTransform(MOTOR_Vars_t *obj)
{
    // Set phasor and run Park transform (phasor already computed)
    PARK_setPhasor(&obj->park_I, &obj->phasor);
    PARK_run(&obj->park_I, &(obj->estInputData.Iab_A), (MATH_vec2 *)&(obj->outVars.Idq_in_A));
}

/* Runs speed controller and current reference generation */
static inline void FOC_runSpeedController(MOTOR_Vars_t *obj)
{
    USER_Params *objUser = (USER_Params *)(obj->userParamsHandle);

    obj->counterSpeed++;

    if (obj->counterSpeed >= objUser->numCtrlTicksPerSpeedTick)
    {
        obj->counterSpeed = 0;

        if (obj->statusVars.enableSpeedCtrl == true)
        {
            PI_run(&obj->pi_spd, obj->statusVars.speed_int_Hz, obj->outVars.speed_Hz, (float32_t *)&obj->IsRef_A);
        }

        obj->outVars.Idq_out_A.value[0] = 0.0;
        obj->outVars.Idq_out_A.value[1] = obj->IsRef_A;
    }
    /* Execute FW / MTPA only in Closed Loop State */
    if (((obj->inputCtrl.flagEnableFWC == true) || (obj->inputCtrl.flagEnableMTPA == true)) &&
        (obj->outVars.motorState >= MOTOR_CL_RUNNING))
    {
        // #TAG_FIELD_WEAKENING_START#
        if (obj->counterSpeed == 1)
        {
            // Compute output voltage magnitude
            obj->statusVars.Vs_V = __sqrt((obj->outVars.Vdq_out_V.value[0] * obj->outVars.Vdq_out_V.value[0]) +
                                          (obj->outVars.Vdq_out_V.value[1] * obj->outVars.Vdq_out_V.value[1]));
            obj->VsRef_V         = obj->VsRef_pu * obj->adcData->VdcBus_V;
        }
        else if (obj->counterSpeed == 2)
        {
            // Field Weakening Control
            if (obj->inputCtrl.flagEnableFWC == true)
            {
                float32_t angleFWC;
                PI_run(&obj->pi_fwc, obj->VsRef_V, obj->statusVars.Vs_V, (float32_t *)&angleFWC);
                obj->outVars.angleFWC_rad = MATH_PI_OVER_TWO - angleFWC;
            }
            else
            {
                PI_setUi(&obj->pi_fwc, 0.0f);
                obj->outVars.angleFWC_rad = MATH_PI_OVER_TWO;
            }
        }
        else if (obj->counterSpeed == 3)
        {
            // FWC/MTPA angle calculation
            float32_t fwcCos = __cos(obj->statusVars.angleCurrent_rad);
            float32_t fwcSin = __sin(obj->statusVars.angleCurrent_rad);

            obj->statusVars.angleCurrent_rad = (obj->outVars.angleFWC_rad > obj->outVars.angleMTPA_rad)
                                                   ? obj->outVars.angleFWC_rad
                                                   : obj->outVars.angleMTPA_rad;

            fwcCos = __cos(obj->statusVars.angleCurrent_rad);
            fwcSin = __sin(obj->statusVars.angleCurrent_rad);

            obj->outVars.Idq_out_A.value[0] = obj->IsRef_A * fwcCos;
            obj->outVars.Idq_out_A.value[1] = obj->IsRef_A * fwcSin;
        }
        // #TAG_FIELD_WEAKENING_END#

        // #TAG_MTPA_START#
        else if (obj->counterSpeed == 4)
        {
            // MTPA (Maximum Torque Per Ampere)
            if (obj->inputCtrl.flagEnableMTPA == true)
            {
                obj->outVars.angleMTPA_rad = MTPA_computeCurrentAngle(obj->mtpaHandle, obj->IsRef_A);
            }
            else
            {
                obj->outVars.angleMTPA_rad = MATH_PI_OVER_TWO;
            }
        }
        // #TAG_MTPA_END#
    }
}

/* Runs current controllers (Id and Iq PI controllers) */
static inline void FOC_runCurrentControllers(MOTOR_Vars_t *obj)
{
    USER_Params *objUser = (USER_Params *)(obj->userParamsHandle);

    obj->outVars.IdqRef_A.value[0] = obj->outVars.Idq_out_A.value[0] + obj->IdRated_A;

    // #TAG_FAST_OBS_START#
    EST_updateId_ref_A(obj->estHandle, &obj->outVars.IdqRef_A.value[0]);
    // #TAG_FAST_OBS_END#

    obj->outVars.IdqRef_A.value[1] = obj->outVars.Idq_out_A.value[1];

    if (obj->statusVars.enableCurrentCtrl == true)
    {
        // Maximum voltage output
        objUser->maxVsMag_V = objUser->maxVsMag_pu * obj->adcData->VdcBus_V;

        PI_setMinMax(&obj->pi_Id, -objUser->maxVsMag_V, objUser->maxVsMag_V);

        // Run Id controller
        PI_run(&obj->pi_Id, obj->outVars.IdqRef_A.value[0], obj->outVars.Idq_in_A.value[0],
               (float32_t *)&obj->outVars.Vdq_out_V.value[0]);

        // Calculate Iq controller limits (circular limit)
        float32_t outMax_V = __sqrt((objUser->maxVsMag_V * objUser->maxVsMag_V) -
                                    (obj->outVars.Vdq_out_V.value[0] * obj->outVars.Vdq_out_V.value[0]));

        PI_setMinMax(&obj->pi_Iq, -outMax_V, outMax_V);

        // Run Iq controller
        PI_run(&obj->pi_Iq, obj->outVars.IdqRef_A.value[1], obj->outVars.Idq_in_A.value[1],
               (float32_t *)&obj->outVars.Vdq_out_V.value[1]);

        // #TAG_DEAD_TIME_COMP_START#
        //  Dead time compensation
        if (DTC_isEnabled(obj->deadTimeCompHandle) == true)
        {
            DTC_run(obj->deadTimeCompHandle, obj->outVars.Idq_in_A.value[0], obj->outVars.Idq_in_A.value[1]);
            obj->outVars.Vdq_out_V.value[0] += DTC_getVd(obj->deadTimeCompHandle);
            obj->outVars.Vdq_out_V.value[1] += DTC_getVq(obj->deadTimeCompHandle);
        }
        // #TAG_DEAD_TIME_COMP_END#

        // #TAG_FAST_OBS_START#
        //  Set reference values in estimator
        EST_setId_ref_A(obj->estHandle, obj->outVars.IdqRef_A.value[0]);
        EST_setIq_ref_A(obj->estHandle, obj->outVars.IdqRef_A.value[1]);
        // #TAG_FAST_OBS_END#
    }
    if (obj->inputCtrl.forceOpenLoopVoltageMode)
    {
        obj->outVars.Vdq_out_V.value[0] = obj->Vdq_OpenLoop.value[0];
        obj->outVars.Vdq_out_V.value[1] = obj->Vdq_OpenLoop.value[1];
    }
}

/* Runs inverse Park transform and SVPWM generation */
static inline void FOC_runPWMGeneration(MOTOR_Vars_t *obj)
{
    // Set phasor for inverse Park transform
    IPARK_setPhasor(&obj->ipark_V, &obj->phasor);

    // Run inverse Park transform (dq -> alpha-beta)
    IPARK_run(&obj->ipark_V, &obj->outVars.Vdq_out_V, &obj->outVars.Vab_out_V);

    // #TAG_TI_TUNE_START#
    if (obj->statusVars.flagMotorTiTuneRun)
    {
        /* Link the output Alpha Beta Voltages */
        obj->outVars.Vab_out_V.value[0] = obj->tiTuneHandle->Status.Out_Vab_V.value[0];

        obj->outVars.Vab_out_V.value[1] = obj->tiTuneHandle->Status.Out_Vab_V.value[1];
    }
    // #TAG_TI_TUNE_END#

    if (obj->inputCtrl.svmMode == SVM_COM_C)
    {
        SVGEN_runCom(obj->oneOverDcBus_invV, obj->outVars.Vab_out_V.value[0], obj->outVars.Vab_out_V.value[1],
                     &obj->pwmVabc_pu->value[0], &obj->pwmVabc_pu->value[1], &obj->pwmVabc_pu->value[2]);
    }
    else if (obj->inputCtrl.svmMode == SVM_MIN_C)
    {
        SVGEN_runMin(obj->oneOverDcBus_invV, obj->outVars.Vab_out_V.value[0], obj->outVars.Vab_out_V.value[1],
                     &obj->pwmVabc_pu->value[0], &obj->pwmVabc_pu->value[1], &obj->pwmVabc_pu->value[2]);
    }

    // Clear PWM data if PWMs are disabled
    if (HAL_getPwmEnableStatus(obj->pwmData) == false)
    {
        obj->pwmVabc_pu->value[0] = 0.0f;
        obj->pwmVabc_pu->value[1] = 0.0f;
        obj->pwmVabc_pu->value[2] = 0.0f;
    }

    // Write PWM values to hardware
    HAL_SetPhaseVoltages(obj->halInterface);
}

/*
 * Main FOC feedforward execution
 *
 * OPTIMIZATION: Phasor computed ONCE and reused for both Park and iPark transforms
 */
static inline void FOC_runFeedforward(MOTOR_Vars_t *obj)
{
    // Compute sin/cos phasor ONCE for both Park and iPark transforms
    FOC_computePhasor(obj);

    // Run Park transform on currents (uses phasor computed above)
    FOC_runParkTransform(obj);

    // Run speed controller and generate current references
    FOC_runSpeedController(obj);
    frl_test_ref(obj);   // FRL (revision H5): the hold test's current reference, test mode only (frl_guard.c)

    // Run current controllers
    FOC_runCurrentControllers(obj);
    frl_test_volt(obj);  // FRL (revision H5): the voltage hold, test mode 2 only

    // Run inverse Park and PWM generation (reuses same phasor)
    FOC_runPWMGeneration(obj);
}
#ifdef __cplusplus
}
#endif

#endif /* FOC_H */

/** @} */
