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

#include "ti_tune.h"

/* Forward declarations for setParams functions defined later in this file */
void TiTune_LDQ_setParams(TiTune_LDQ_Handle handle);
void TiTune_Rs_setParams(TiTune_Rs_Handle handle);
void TiTune_J_Coast_setParams(TiTune_J_Coast_Handle handle);

//*****************************************************************************
//
//! brief Object declarations
//
//*****************************************************************************

//*****************************************************************************
//
// TiTune_init
//
//*****************************************************************************
TiTune_Handle TiTune_init(void *pMemory, const size_t numBytes)
{
    TiTune_Handle handle;

    if ((int16_t)numBytes < (int16_t)sizeof(TiTune_Obj))
    {
        return ((TiTune_Handle)NULL);
    }

    //
    // Assign the handle
    //
    handle = (TiTune_Handle)pMemory;

    return (handle);
}  // end of TiTune_init() function

//*****************************************************************************
//
// TiTune_setParams
//
//*****************************************************************************
void TiTune_setParams(TiTune_Handle handle)
{
    TiTune_Obj *obj = (TiTune_Obj *)handle;

    /* Copy motor base values from const flash table into writable RAM fields.
     * These can be overwritten via GUI/debugger after this call. */
    obj->RatedCurrent_A = motorParams_M1->RatedCurrent_A;
    obj->RatedSpeed_Hz  = motorParams_M1->RatedSpeed_Hz;

    obj->Status.state = TiTune_STATE_IDLE;

    obj->TiTune_enable = 0;

    obj->Status.count = 0;

    obj->Status.Rs_IPD_Angle = 0.0f;  // TODO: to be initialized with the IPD angle

    /* Initialize speed output for ESMO */
    obj->Status.Speed_out_Hz = 0.0f;

    /* Initialize fault flag */
    obj->Status.faultObserverTimeout = false;

    obj->TiTune_LDQ_handler = TiTune_LDQ_init(&obj->TiTune_LDQ, sizeof(obj->TiTune_LDQ));
    TiTune_LDQ_setParams(obj->TiTune_LDQ_handler);

    obj->TiTune_J_Coast_handler = TiTune_J_Coast_init(&obj->TiTune_J_Coast, sizeof(obj->TiTune_J_Coast));
    TiTune_J_Coast_setParams(obj->TiTune_J_Coast_handler);

    obj->TiTune_Rs_handler = TiTune_Rs_init(&obj->TiTune_Rs, sizeof(obj->TiTune_Rs));
    TiTune_Rs_setParams(obj->TiTune_Rs_handler);

    obj->TiTune_OL_handler = TiTune_OL_init(&obj->TiTune_OL, sizeof(obj->TiTune_OL));
    TiTune_OL_setParams(obj->TiTune_OL_handler, obj->RatedCurrent_A, obj->RatedSpeed_Hz, 0.0f, USER_M1_CTRL_PERIOD_sec);

    return;
}  // end of TiTune_setParams() function
////*****************************************************************************
////
//// TiTune_stateMachineHandler
////
////*****************************************************************************
void TiTune_Run_stateMachine(TiTune_Handle handle)
{
    TiTune_Obj       *obj     = (TiTune_Obj *)handle;
    TiTune_LDQ_Handle obj_ldq = handle->TiTune_LDQ_handler;
    float32_t         RbyLd;
    float32_t         RbyLq;

    PARK_Obj TiTune_park_I;  // Handle for computing the D-Q currents from I A-B

    /* Run the Park Transform on Alpha beta Currents */

    if (obj->Status.state >= TiTune_STATE_CL_TORQUE_SETTLE)
    {
        /* From CL_TORQUE_SETTLE onward the observer angle is the FOC angle.
         * Pre-compute the phasor here so the Park transform uses the latest value. */
        obj->Status.phasor.value[0] = __cos(obj->Config.Observer_Angle_rad);
        obj->Status.phasor.value[1] = __sin(obj->Config.Observer_Angle_rad);
    }
    // For Rs and Ls Estimation , the angles are pre computed */

    // TODO: the Phasor to be updated with next estimated angle after Tpwm.

    // Park & inverse Park transform working Local variables
    IPARK_Obj TiTune_ipark_V;  // Handle for computing the iPark voltages

    PARK_setPhasor(&TiTune_park_I, &obj->Status.phasor);

    // run the Park transform on the Alpha beta Currents.

    PARK_run(&TiTune_park_I, &obj->Config.Iab_A, &(obj->Status.Idq_A));

    switch (obj->Status.state)
    {
        case TiTune_STATE_IDLE:
            /* Re-initialize all sub-modules so the next run starts clean.
             * Results from the previous run remain readable until this point
             * (not wiped on exit, only here when a new run is triggered). */
            TiTune_LDQ_setParams(obj->TiTune_LDQ_handler);
            TiTune_Rs_setParams(obj->TiTune_Rs_handler);
            TiTune_J_Coast_setParams(obj->TiTune_J_Coast_handler);
            TiTune_OL_setParams(obj->TiTune_OL_handler, obj->RatedCurrent_A, obj->RatedSpeed_Hz, 0.0f,
                                USER_M1_CTRL_PERIOD_sec);

            /* Clear PI integrators -- not reset by sub-module setParams */
            PI_setUi(&obj->TiTune_PI_Id, 0.0f);
            PI_setUi(&obj->TiTune_PI_Iq, 0.0f);
            PI_setUi(&obj->TiTune_PI_Speed, 0.0f);

            /* Zero Vdq working output so IPARK produces zero Vab on the first tick */
            obj->Status.Out_Vdq_V.value[0] = 0.0f;
            obj->Status.Out_Vdq_V.value[1] = 0.0f;

            obj->Status.state = TiTune_STATE_LDQ_SETUP;
            break;

            /**********************Start of Ls Estimation ************************************************/

        case TiTune_STATE_LDQ_SETUP:
            obj->Status.state              = TiTune_STATE_LDQ_EST;
            obj->Status.Out_Vdq_V.value[1] = 0.0f;

            break;
        case TiTune_STATE_LDQ_EST:

            /* Read the Feedback currents and estimate the L based on Phase and magnitude*/

            TiTune_LDQ_run(obj->TiTune_LDQ_handler, obj->Config.VdcBus_V, obj->Status.Idq_A.value[0],
                           obj->Status.Idq_A.value[1]);

            /* Run the forced Angle Open loop model with increasing Frequency */

            obj->Config.FOC_Angle_rad = obj_ldq->ElecTheta * MATH_TWO_PI;

            obj->Status.Out_Vdq_V.value[0] = obj->TiTune_LDQ_handler->InjVol;

            obj->Status.Out_Vdq_V.value[1] = 0.0f;

            obj_ldq->InjVol = 0.0f;

            if (obj_ldq->Finish == true)
            {
                obj->Status.state = TiTune_STATE_LDQ_DONE;
                obj_ldq->ld_Final = obj_ldq->LminApprox * obj_ldq->inductance_base;
                obj_ldq->lq_Final = obj_ldq->LmaxApprox * obj_ldq->inductance_base;
            }

            break;

        case TiTune_STATE_LDQ_DONE:

            obj->Status.IdqRef_A.value[0] = 0;
            obj->Status.IdqRef_A.value[1] = 0;

            obj->Status.Out_Vdq_V.value[0] = 0.0f;
            obj->Status.Out_Vdq_V.value[1] = 0.0f;

            /* Publish Ld/Lq immediately so GUI shows results as soon as available */
            obj->Results.Ld_H = obj_ldq->ld_Final;
            obj->Results.Lq_H = obj_ldq->lq_Final;

            obj->Status.state = TiTune_STATE_RS_SETUP;
            break;

            /**********************Start of Rs Estimation ************************************************/
        case TiTune_STATE_RS_SETUP:

            if (++obj->Status.count > TI_TUNE_STATE_DELAY_COUNT)
            {
                obj->Status.count    = 0;
                obj->TiTune_PI_Id.Kp = obj_ldq->ld_Final * TI_TUNE_TORQUE_LOOP_BW_RADPS;

                obj->TiTune_PI_Id.Ki = obj->TiTune_PI_Id.Kp * TI_TUNE_TORQUE_LOOP_BW_RADPS * USER_M1_CTRL_PERIOD_sec;

                obj->TiTune_PI_Iq.Kp = obj_ldq->lq_Final * TI_TUNE_TORQUE_LOOP_BW_RADPS;

                obj->TiTune_PI_Iq.Ki = obj->TiTune_PI_Iq.Kp * TI_TUNE_TORQUE_LOOP_BW_RADPS * USER_M1_CTRL_PERIOD_sec;

                /* Initial Rotor angle based on the LMin */

                float32_t angle_rad;

                angle_rad = obj_ldq->InitialRotorAngle * MATH_TWO_PI;

                if (angle_rad > MATH_PI)
                {
                    angle_rad -= MATH_TWO_PI;
                }

                obj->Status.Rs_IPD_Angle = angle_rad;

                obj->TiTune_Rs_handler->VdistGain1 =
                    (2.0f / 3.0f) * (MATH_abs(cosf(obj->Status.Rs_IPD_Angle)) +
                                     MATH_abs(cosf(obj->Status.Rs_IPD_Angle - (1.0f / 3.0f) * MATH_TWO_PI)) +
                                     MATH_abs(cosf(obj->Status.Rs_IPD_Angle - (2.0f / 3.0f) * MATH_TWO_PI)));

                obj->TiTune_Rs_handler->VariableInit = true;

                /* Publish current PI gains as soon as they are computed */
                obj->Results.Id_Kp = obj->TiTune_PI_Id.Kp;
                obj->Results.Id_Ki = obj->TiTune_PI_Id.Ki;
                obj->Results.Iq_Kp = obj->TiTune_PI_Iq.Kp;
                obj->Results.Iq_Ki = obj->TiTune_PI_Iq.Ki;

                obj->Status.state = TiTune_STATE_RS_EST;
            }
            break;
        case TiTune_STATE_RS_EST:
            //            TiTune_Rs_Align_Count

            TiTune_Rs_run(obj->TiTune_Rs_handler, obj->TiTune_Rs_handler->VdistGain1, obj->Status.Idq_A.value[0],
                          obj->Status.Out_Vdq_V.value[0]);

            /* Set the Reference Idq currents for Current Controller */
            obj->Status.IdqRef_A.value[0] = obj->TiTune_Rs.IdRef * obj->RatedCurrent_A;
            obj->Status.IdqRef_A.value[1] = 0;

            TiTune_Run_Current_Control(handle);

            obj->Config.FOC_Angle_rad = obj->Status.Rs_IPD_Angle;

            /* When Rs Estimation is completed Switch to Rs Done State */
            if (obj->TiTune_Rs_handler->Finish == true)
            {
                obj->Status.state                = TiTune_STATE_RS_DONE;
                obj->TiTune_Rs_handler->Rs_Final = obj->TiTune_Rs_handler->Rs * obj->TiTune_Rs_handler->impedance_base;
            }
            break;
        case TiTune_STATE_RS_DONE:

            /* Publish Rs immediately so GUI shows result as soon as available */
            obj->Results.Rs_Ohm = obj->TiTune_Rs_handler->Rs_Final;

            obj->Status.state = TiTune_STATE_OL_SETUP;
            TiTune_Run_Current_Control(handle);

            break;
            /**************************End of Rs Estimation********************************************/

            /**************************Start of Open-Loop Startup********************************************/
        case TiTune_STATE_OL_SETUP:
            /* Setup current controller gains using estimated Rs and Ld/Lq */
            obj->TiTune_PI_Id.Kp = obj->TiTune_LDQ_handler->ld_Final * TI_TUNE_TORQUE_LOOP_BW_RADPS;
            obj->TiTune_PI_Iq.Kp = obj->TiTune_LDQ_handler->lq_Final * TI_TUNE_TORQUE_LOOP_BW_RADPS;

            /* Setup current controller Ki using pole-zero cancellation: Ki = (Rs/L) * Ts * factor */
            RbyLd = (obj->TiTune_Rs_handler->Rs_Final) / (obj->TiTune_LDQ_handler->ld_Final);
            RbyLq = (obj->TiTune_Rs_handler->Rs_Final) / (obj->TiTune_LDQ_handler->lq_Final);

            obj->TiTune_PI_Id.Ki = TI_TUNE_CURRENT_CTRL_KI_FACTOR * RbyLd * USER_M1_CTRL_PERIOD_sec;
            obj->TiTune_PI_Iq.Ki = TI_TUNE_CURRENT_CTRL_KI_FACTOR * RbyLq * USER_M1_CTRL_PERIOD_sec;

            /* Initialize OL module with Rs IPD angle as starting angle */
            TiTune_OL_setParams(obj->TiTune_OL_handler, obj->RatedCurrent_A, obj->RatedSpeed_Hz,
                                obj->Status.Rs_IPD_Angle, USER_M1_CTRL_PERIOD_sec);

            obj->Status.state = TiTune_STATE_OL_RUN;

            TiTune_Run_Current_Control(handle);

            break;

        case TiTune_STATE_OL_RUN:
            /* Run OL startup library -- angle blending and current profiling are hidden */
            TiTune_OL_run(obj->TiTune_OL_handler, obj->Config.Observer_Angle_rad);

            obj->Config.FOC_Angle_rad     = TiTune_OL_getFOCAngle(obj->TiTune_OL_handler);
            obj->Status.IdqRef_A.value[0] = TiTune_OL_getIdRef(obj->TiTune_OL_handler);
            obj->Status.IdqRef_A.value[1] = TiTune_OL_getIqRef(obj->TiTune_OL_handler);
            obj->Status.Speed_out_Hz      = TiTune_OL_getSpeed(obj->TiTune_OL_handler);

            /* Pre-load integrators for bumpless transfer */
            PI_setUi(&obj->TiTune_PI_Id, obj->Status.Out_Vdq_V.value[0]);
            PI_setUi(&obj->TiTune_PI_Iq, obj->Status.Out_Vdq_V.value[1]);

            TiTune_Run_Current_Control(handle);

            if (TiTune_OL_isDone(obj->TiTune_OL_handler))
            {
                /* Initialize CL torque-settle variables from OL handoff values */
                obj->CL.IqRef_A      = TiTune_OL_getHandoffIqRef(obj->TiTune_OL_handler);
                obj->CL.IdRef_A      = TiTune_OL_getHandoffIdRef(obj->TiTune_OL_handler);
                obj->CL.IdRampDown_A = obj->CL.IdRef_A / (TI_TUNE_CL_ID_RAMP_TIME_SEC / USER_M1_CTRL_PERIOD_sec);
                obj->CL.SettleCount  = 0U;
                obj->CL.SettleTarget = (uint32_t)(TI_TUNE_CL_ID_RAMP_TIME_SEC / USER_M1_CTRL_PERIOD_sec);
                obj->Status.state    = TiTune_STATE_CL_TORQUE_SETTLE;
            }
            break;

        case TiTune_STATE_CL_TORQUE_SETTLE:
            /**************************Closed-Loop Torque Settling*************************************/
            /* Use estimated angle from observer (fed via Config.FOC_Angle_rad from application).
             * Iq is held constant at the OL handoff value -- no ramp.
             * Id is ramped from OL injection level to zero over TI_TUNE_CL_ID_RAMP_TIME_SEC. */

            /* Ramp Id toward zero */
            if (obj->CL.IdRef_A > 0.0f)
            {
                obj->CL.IdRef_A -= obj->CL.IdRampDown_A;
                if (obj->CL.IdRef_A < 0.0f)
                {
                    obj->CL.IdRef_A = 0.0f;
                }
                else
                {
                    /* IdRef_A still positive -- ramp continues next cycle */
                }
            }
            else
            {
                /* Id already at zero -- no ramp needed */
            }

            obj->Config.FOC_Angle_rad = obj->Config.Observer_Angle_rad;

            /* Set current references: Id ramps to zero, Iq stays constant */
            obj->Status.IdqRef_A.value[0] = obj->CL.IdRef_A;
            obj->Status.IdqRef_A.value[1] = obj->CL.IqRef_A;

            /* Feed back estimated speed for ESMO speed reference */
            obj->Status.Speed_out_Hz = obj->Config.Speed_Hz;

            /* Run current control */
            TiTune_Run_Current_Control(handle);

            /* Increment settling counter */
            obj->CL.SettleCount++;

            /* Wait for fixed settling time before transitioning to inertia estimation */
            if (obj->CL.SettleCount >= obj->CL.SettleTarget)
            {
                obj->Status.state = TiTune_STATE_INERTIA_SETUP;
            }
            break;

            /**************************Start of Inertia Estimation (Coast-Down)************************/
        case TiTune_STATE_INERTIA_SETUP:  //!< motor inertia set-up state

            obj->Config.FOC_Angle_rad = obj->Config.Observer_Angle_rad;

            if (++obj->Status.count > TI_TUNE_STATE_DELAY_COUNT)
            {
                obj->Status.count = 0;

                /* Pass Rs from estimation to coast-down module for Ke computation */
                obj->TiTune_J_Coast_handler->Rs_ohm = obj->TiTune_Rs_handler->Rs_Final;

                /* Coast-down algorithm will auto-initialize on first run() call */
                obj->Status.state = TiTune_STATE_INERTIA_EST;
            }

            /* Keep running current control during setup delay */
            obj->Status.IdqRef_A.value[0] = 0.0f;
            obj->Status.IdqRef_A.value[1] = obj->CL.IqRef_A;

            /* Feed back estimated speed for ESMO speed reference */
            obj->Status.Speed_out_Hz = obj->Config.Speed_Hz;

            obj->TiTune_J_Coast_handler->variableInit = true;
            TiTune_Run_Current_Control(handle);

            break;

        case TiTune_STATE_INERTIA_EST:  //!< estimation of motor inertia state, J (coast-down)

            obj->Config.FOC_Angle_rad = obj->Config.Observer_Angle_rad;

            /* Run coast-down inertia and damping estimation
             * This method:
             * 1. Ramps current adaptively until target speed is reached
             * 2. Measures steady-state torque-speed at multiple points to estimate B
             * 3. Performs coast-down test to estimate J from exponential decay
             * 4. Calculates PI gains from identified B and J parameters
             *
             * Pass settled current for bumpless transfer
             * Convert speed from Hz to per-unit: speed_pu = speed_Hz / F_Base */
            TiTune_J_Coast_run(obj->TiTune_J_Coast_handler, obj->Config.Speed_Hz / obj->RatedSpeed_Hz,
                               obj->CL.IqRef_A / obj->RatedCurrent_A,
                               obj->Status.Out_Vdq_V.value[1] / (TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V / MATH_SQRT_THREE));

            /* Get Iq reference from coast-down algorithm and convert to Amperes */
            obj->Status.IdqRef_A.value[1] = TiTune_J_Coast_getIqRef(obj->TiTune_J_Coast_handler) * obj->RatedCurrent_A;
            obj->Status.IdqRef_A.value[0] = 0.0f;

            /* Feed back estimated speed for ESMO speed reference */
            obj->Status.Speed_out_Hz = obj->Config.Speed_Hz;

            TiTune_Run_Current_Control(handle);

            /* Check if coast-down estimation is complete */
            if (TiTune_J_Coast_isFinished(obj->TiTune_J_Coast_handler))
            {
                obj->Status.state = TiTune_STATE_INERTIA_DONE;
            }
            break;

        case TiTune_STATE_INERTIA_DONE:  //!< motor inertia done state
        {
            obj->Config.FOC_Angle_rad = obj->Config.Observer_Angle_rad;

            /* Apply speed PI gains from coast-down identification first so that
             * the Results snapshot below captures the freshly computed values. */
            obj->TiTune_PI_Speed.Kp = TiTune_J_Coast_getKp(obj->TiTune_J_Coast_handler);
            obj->TiTune_PI_Speed.Ki = TiTune_J_Coast_getKi(obj->TiTune_J_Coast_handler);

            /* Publish remaining results: Ke, J, and speed PI gains.
             * Rs, Ld, Lq, and current PI gains were already published at their
             * respective phase-completion states (LDQ_DONE, RS_SETUP, RS_DONE).
             *
             * Ke [V/Hz] = lambda_PM [V*s/rad_e] * 2*pi  (matches USER_MOTOR1_RATED_FLUX_VpHz)
             *
             * J [kg*m^2] = tau * (3/2) * P^2 * lambda_PM / G_DC
             *   where tau = coast-down time constant (s)
             *         P = motor pole pairs
             *         G_DC = DC plant gain delta_we/delta_Iq (rad_e/s per A) */
            {
                float32_t lambdaPM  = TiTune_J_Coast_getLambdaPM(obj->TiTune_J_Coast_handler);
                float32_t polePairs = (float32_t)motorParams_M1->NumPolePairs;

                obj->Results.Ke_VpHz = lambdaPM * MATH_TWO_PI;
                obj->Results.J_Kgm2  = obj->TiTune_J_Coast_handler->tau_estimated *
                                      (1.5f * polePairs * polePairs * lambdaPM) / obj->TiTune_J_Coast_handler->G_DC;
                obj->Results.Spd_Kp = obj->TiTune_PI_Speed.Kp;
                obj->Results.Spd_Ki = obj->TiTune_PI_Speed.Ki;
            }

            obj->Status.Out_Vdq_V.value[1] = 0.0;
            obj->Status.Out_Vdq_V.value[0] = 0.0;

            /* Pre-load integrators with current voltage output for bumpless transfer */
            PI_setUi(&obj->TiTune_PI_Id, 0.0);
            PI_setUi(&obj->TiTune_PI_Iq, 0.0);

            obj->Status.state = TiTune_STATE_DONE;

            break;
        }

        case TiTune_STATE_DONE:

            obj->Status.Out_Vdq_V.value[1] = 0.0;
            obj->Status.Out_Vdq_V.value[0] = 0.0;

            /* Stop outputting speed reference */
            obj->Status.Speed_out_Hz = 0.0f;
            break;

        case TiTune_STATE_FAULT:
            /* Observer failed to stabilize within timeout */

            obj->Status.Out_Vdq_V.value[1] = 0.0;
            obj->Status.Out_Vdq_V.value[0] = 0.0;

            /* Stop outputting speed reference */
            obj->Status.Speed_out_Hz = 0.0f;
            break;

        default:
            /* Default: feed back estimated speed for ESMO */
            obj->Status.Speed_out_Hz = obj->Config.Speed_Hz;
            break;
    }

    // Set the Phasor Values
    obj->Status.phasor.value[0] = __cos(obj->Config.FOC_Angle_rad);
    obj->Status.phasor.value[1] = __sin(obj->Config.FOC_Angle_rad);
    //
    // set the phasor in the inverse Park transform
    IPARK_setPhasor(&TiTune_ipark_V, &obj->Status.phasor);

    // run the inverse Park module
    IPARK_run(&TiTune_ipark_V, &obj->Status.Out_Vdq_V, &obj->Status.Out_Vab_V);
}

void TiTune_Run_Current_Control(TiTune_Handle handle)
{
    TiTune_Obj *obj = (TiTune_Obj *)handle;

    float32_t maxVsMag_V = USER_M1_MAX_VS_MAG_PU * obj->Config.VdcBus_V;

    PI_setMinMax(&obj->TiTune_PI_Id, -maxVsMag_V, maxVsMag_V);

    // run the Id controller
    PI_run(&obj->TiTune_PI_Id, obj->Status.IdqRef_A.value[0], obj->Status.Idq_A.value[0],
           (float32_t *)&obj->Status.Out_Vdq_V.value[0]);

    // calculate Iq controller limits
    float32_t outMax_V =
        __sqrt((maxVsMag_V * maxVsMag_V) - (obj->Status.Out_Vdq_V.value[0] * obj->Status.Out_Vdq_V.value[0]));

    PI_setMinMax(&obj->TiTune_PI_Iq, -outMax_V, outMax_V);

    // run the Iq controller
    PI_run(&obj->TiTune_PI_Iq, obj->Status.IdqRef_A.value[1], obj->Status.Idq_A.value[1],
           (float32_t *)&obj->Status.Out_Vdq_V.value[1]);

    // TODO Update the Idq reference to Fast Library
}

//
// end of file
//

void TiTune_LDQ_setParams(TiTune_LDQ_Handle handle)
{
    TiTune_LDQ_Obj *obj = (TiTune_LDQ_Obj *)handle;

    obj->Vdc_Base = TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V;
    obj->Vph_Base = TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V / MATH_SQRT_THREE; /* Per-phase peak voltage */
    obj->I_Base   = motorParams_M1->RatedCurrent_A;
    obj->F_Base   = motorParams_M1->RatedSpeed_Hz; /* Per-unit base frequency = rated electrical speed */

    obj->X_base          = obj->Vph_Base / obj->I_Base;
    obj->flux_base       = obj->Vph_Base / (MATH_TWO_PI * obj->F_Base);
    obj->inductance_base = obj->X_base / (MATH_TWO_PI * obj->F_Base);

    obj->VariableInit              = true;
    obj->InjVolHalfPrdSampleNoMax  = TI_TUNE_LDQ_INJ_HALF_PRD_MAX;
    obj->InjVolHalfPrdSampleNoInit = TI_TUNE_LDQ_INJ_HALF_PRD_INIT;
    obj->InjSettlePrd              = TI_TUNE_LDQ_INJ_SETTLE_PRD;
    obj->AngIntgGain               = obj->F_Base * USER_M1_CTRL_PERIOD_sec;
    obj->ElecThetaStep             = TI_TUNE_LDQ_ELEC_THETA_STEP;
    obj->InjVolMagnInit            = TI_TUNE_LDQ_INJ_VOL_MAGN_INIT;
    obj->IsMagnMinLim              = TI_TUNE_LDQ_IS_MAGN_MIN_LIM;
    obj->IsMagnMaxLim              = TI_TUNE_LDQ_IS_MAGN_MAX_LIM;
    obj->InjVolMagnMaxLim          = TI_TUNE_LDQ_INJ_VOL_MAGN_MAX_LIM;

    return;
}  // end of TiTune_LDQ_setParams() function
/* Sets up parameters for Rs estimation */
void TiTune_Rs_setParams(TiTune_Rs_Handle handle)
{
    TiTune_Rs_Obj *obj = (TiTune_Rs_Obj *)handle;

    obj->Vdc_Base = TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V;
    obj->Vph_Base = TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V / MATH_SQRT_THREE; /* Per-phase peak voltage */
    obj->I_Base   = motorParams_M1->RatedCurrent_A;
    obj->F_Base   = motorParams_M1->RatedSpeed_Hz; /* Per-unit base frequency = rated electrical speed */

    obj->impedance_base = obj->Vph_Base / obj->I_Base;

    obj->VariableInit       = true;
    obj->AveragingSize      = (uint32_t)(TI_TUNE_RS_AVERAGING_TIME_SEC / USER_M1_CTRL_PERIOD_sec);
    obj->Wait4SettleCurrent = (uint32_t)(TI_TUNE_RS_SETTLE_TIME_SEC / USER_M1_CTRL_PERIOD_sec);
    obj->MaxTime4Settle     = (uint32_t)(TI_TUNE_RS_MAX_SETTLE_TIME_SEC / USER_M1_CTRL_PERIOD_sec);
    obj->TestCurrent1       = TI_TUNE_RS_TEST_CURRENT1_FRACTION;
    obj->TestCurrent2       = TI_TUNE_RS_TEST_CURRENT2_FRACTION;
    obj->IdErrLim4Settle    = TI_TUNE_RS_ID_ERR_LIM_FRACTION;

    {
        float32_t wcTs      = MATH_TWO_PI * obj->F_Base * USER_M1_CTRL_PERIOD_sec;
        obj->LPF_IdErr_Gain = wcTs / (1.0f + wcTs);
    }

    obj->i12 = 0;

    return;
}  // end of TiTune_Rs_setParams() function
/* Sets up parameters for coast-down inertia and damping estimation */
void TiTune_J_Coast_setParams(TiTune_J_Coast_Handle handle)
{
    TiTune_J_Coast_Obj *obj = (TiTune_J_Coast_Obj *)handle;

    /* Sample period */
    obj->samplePeriod_sec = USER_M1_CTRL_PERIOD_sec;

    /* Base values for per-unit conversion */
    obj->I_Base              = motorParams_M1->RatedCurrent_A;
    obj->F_Base              = motorParams_M1->RatedSpeed_Hz;
    obj->Vph_Base            = TI_TUNE_ADC_FULL_SCALE_VOLTAGE_V / MATH_SQRT_THREE;
    obj->Rs_ohm              = 0.0f; /* Will be updated after Rs estimation */
    obj->speedCtrlPeriod_sec = USER_M1_CTRL_PERIOD_sec * (float32_t)USER_M1_NUM_ISR_TICKS_PER_SPEED_TICK;

    /* Configurable tuning parameters - fed from user macros in ti_tune.h */
    obj->crossoverFreqFactor   = TI_TUNE_J_COAST_CROSSOVER_FREQ_FACTOR;
    obj->currentRampRate_pu    = TI_TUNE_J_COAST_CURRENT_RAMP_RATE;
    obj->coastdownStopFraction = TI_TUNE_J_COAST_COASTDOWN_STOP_FRACTION;
    obj->maxSteadyStatePoints  = (uint32_t)(TI_TUNE_J_COAST_NUM_STEPS) + 1U; /* point 0 + numSteps ramp captures */

    /* Upward ramp parameters */
    obj->targetSpeed_pu = TI_TUNE_J_COAST_TARGET_SPEED_FRACTION;
    obj->maxCurrent_pu  = TI_TUNE_J_COAST_MAX_CURRENT_FRACTION;
    obj->numSteps       = TI_TUNE_J_COAST_NUM_STEPS;
    /* speedStepThreshold_pu is computed at variableInit time (depends on initialSpeed_pu) */

    /* IIR filter coefficients for INITIAL_SETTLE and SPEED_STEP_SETTLE phases
     * alpha = wc*Ts / (1 + wc*Ts)  where wc = 2*pi*fc
     * Iq/Vq: fc = TI_TUNE_J_COAST_IQ_FILTER_CUTOFF_HZ  (electrical domain, ~10 Hz)
     * Speed: fc = TI_TUNE_J_COAST_SPD_FILTER_CUTOFF_HZ (mechanical domain, ~2 Hz) */
    {
        float32_t wcIq     = MATH_TWO_PI * TI_TUNE_J_COAST_IQ_FILTER_CUTOFF_HZ * obj->samplePeriod_sec;
        float32_t wcSpd    = MATH_TWO_PI * TI_TUNE_J_COAST_SPD_FILTER_CUTOFF_HZ * obj->samplePeriod_sec;
        obj->lpfGain_Iq    = wcIq / (1.0f + wcIq);
        obj->lpfGain_Speed = wcSpd / (1.0f + wcSpd);
    }

    /* Convert time-based parameters to sample counts using ISR period */
    obj->settleTarget          = (uint32_t)(TI_TUNE_J_COAST_SETTLING_TIME_SEC / obj->samplePeriod_sec);
    obj->coastdownSamplePeriod = (uint32_t)(TI_TUNE_J_COAST_COASTDOWN_SAMPLE_PERIOD_SEC / obj->samplePeriod_sec);
    obj->maxCoastdownTicks     = (uint32_t)(TI_TUNE_J_COAST_MAX_COASTDOWN_SEC / obj->samplePeriod_sec);
    obj->maxCoastdownRegressionSamples =
        (uint32_t)(TI_TUNE_J_COAST_MAX_COASTDOWN_REGRESSION_SEC / TI_TUNE_J_COAST_COASTDOWN_SAMPLE_PERIOD_SEC);

    /* Adaptive decrement: test points will be determined dynamically */
    obj->numSteadyStatePoints = 0U;

    /* Initialize state machine */
    obj->variableInit = true;
    obj->state        = TI_TUNE_J_COAST_STATE_IDLE;
    obj->finish       = false;
    obj->error        = false;

    return;
} /* end of TiTune_J_Coast_setParams() function */
