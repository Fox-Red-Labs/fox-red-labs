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
 * \file hall_foc.h
 * \brief Hall sensor FOC observer inline functions
 *
 * \defgroup HALL_FOC_MODULE Hall Sensor FOC Observer
 * \brief Inline FOC observer functions for Hall sensor position feedback
 *
 * This header is included ONLY by hall_sen_foc_app.c.
 * It must NOT be included by foc.h or motor_common.h.
 *
 * By isolating Hall-specific inline functions here, foc.h and motor_common.h
 * remain free of any preprocessor conditionals. The full HAL_Hall_Interface_t
 * type and HAL_ReadHallState() are only visible to translation units that
 * include this header — i.e. the hall sensor application file.
 *
 * Prerequisites (must be included before this header):
 *   - motor_common.h  (provides MOTOR_Vars_t)
 *   - hal_hall_sen_interface.h is included by this header directly
 *
 * @{
 */

#ifndef HALL_FOC_H
#define HALL_FOC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Full Hall type definitions and HAL_ReadHallState() declaration.
 * Included here so that foc.h and motor_common.h do not need to pull in
 * Hall-specific GPIO/ECAP symbols for sensorless builds. */
#include "hal_hall_sen_interface.h"

/* MOTOR_Vars_t definition — must already be included by the including .c file
 * via application.h -> motor_common.h, but included here for completeness. */
#include "motor_common.h"

/**
 * \brief Read Hall sensor state and update FOC angle and speed.
 *
 * Reads the 3-bit Hall GPIO state, decodes the electrical angle via the
 * sector lookup table, and updates ECAP-based speed estimation.
 * Writes angleHall_rad and speedHall_Hz to statusVars, then sets
 * angleFOC_rad and speed_Hz as the primary FOC sources, overriding
 * the FAST/ESMO observer outputs for this ISR cycle.
 *
 * Before calling HAL_Hall_readState(), the current motor direction is copied
 * from inputCtrl.direction into the Hall data handle so that
 * HAL_Hall_updateEcapInput_inline() can select the correct next-edge XBAR
 * without requiring a signature change to HAL_Hall_readState().
 *
 * Called unconditionally from hall_sen_foc_app.c after FOC_runObservers().
 * No #if guard needed — this file is only compiled into the hall project.
 *
 * \param[inout] obj  Pointer to motor control variables structure
 */
static inline void FOC_runHallObserver(MOTOR_Vars_t *obj)
{
    /* Propagate direction into the Hall data handle before reading state.
     * HAL_Hall_readState() -> HAL_Hall_updateEcapInput_inline() reads this
     * field to select the next-edge XBAR on every Hall state transition. */
    HAL_Hall_Data_t   *hallData = (HAL_Hall_Data_t *)(obj->halSensorInterface->sensorData);
    HAL_Hall_Config_t *hallCfg  = (HAL_Hall_Config_t *)(obj->halSensorInterface->sensorConfig);

    hallData->direction = obj->inputCtrl.direction;

    HAL_Hall_readState(hallCfg, hallData);
    obj->statusVars.angleHall_rad = hallData->angleHall_rad;
    obj->statusVars.speedHall_Hz  = hallData->speedHall_Hz;
    obj->statusVars.angleObs_rad  = obj->statusVars.angleHall_rad;
    obj->outVars.speed_Hz         = obj->statusVars.speedHall_Hz;
}

/**
 * \brief Update Hall speed filter bandwidth dynamically from motor reference speed.
 *
 * Sets the Hall LPF pole to max(2 * |speedRef_Hz| * 2*pi, USER_M1_HALL_SPEED_FILTER_POLE_rps)
 * so the filter bandwidth tracks the motor speed while always maintaining the
 * minimum bandwidth defined by USER_M1_HALL_SPEED_FILTER_POLE_rps.
 *
 * Called from the low-priority state machine (background loop) once per 1 ms tick.
 * No-op when hallInterface is NULL (sensorless projects).
 *
 * \param[inout] obj  Pointer to motor control variables structure
 */
static inline void FOC_updateHallSpeedFilter(MOTOR_Vars_t *obj)
{
    if (obj->halSensorInterface != NULL)
    {
        HAL_Hall_Data_t *hallData          = (HAL_Hall_Data_t *)(obj->halSensorInterface->sensorData);
        /* FRL port (2026-10-17): capped. TI's pole is 2 x speedFailMaxSet_Hz (3800 Hz) x 2 pi = 47,752 rad/s, and
         * the filter gain is pole x ISR period: 0.955 at the solution's 50 kHz, 2.39 at this port's 20 kHz. Above 2 a
         * first-order filter diverges, and speedHall_Hz and angleHall_rad went to NaN during the Hall calibration, so
         * TI's FOC stalled at the closed-loop handover (startupFailed). The cap holds the gain at 1, next to TI's 0.955.
         * The filter is applied once per Hall edge, so a pole from the speed reference (as the description says)
         * lags by about a second at 30 Hz electrical; this keeps TI's shipped behaviour instead. */
        float32_t        speedRef_abs_Hz   = fabsf(obj->motorSetsHandle->speedFailMaxSet_Hz);
        float32_t        dynamicFilter_rps = 2.0f * speedRef_abs_Hz * MATH_TWO_PI;
        float32_t        maxFilter_rps     = 1.0f / (USER_M1_ISR_PERIOD_usec * 1.0e-6f);

        hallData->speedFilter_rps = fminf(fmaxf(dynamicFilter_rps, USER_M1_HALL_SPEED_FILTER_POLE_rps), maxFilter_rps);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HALL_FOC_H */

/** @} */
