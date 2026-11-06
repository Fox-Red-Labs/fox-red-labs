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
 * \file hal_sensor_interface.h
 * \brief Generic HAL sensor interface for position/speed sensing
 *
 * This interface provides a type-erased wrapper for sensor-specific
 * HAL implementations (Hall, encoder, resolver, etc.). Each sensor
 * type provides concrete implementations of sensorConfig, sensorData, and
 * hwHandle pointers.
 *
 * \defgroup HAL_SENSOR_INTERFACE HAL Sensor Interface
 * \brief Generic interface for sensor HAL implementations
 * @{
 */

#ifndef HAL_SENSOR_INTERFACE_H
#define HAL_SENSOR_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * \brief Generic HAL sensor interface structure
 *
 * This structure provides type-erased pointers to sensor-specific
 * HAL configuration, data, and handle structures. Each sensor type
 * (Hall, encoder, resolver, CAN) implements these pointers with
 * concrete types.
 *
 * **Type Erasure Pattern**:
 * - sensorConfig: Points to sensor-specific config struct (e.g., HAL_Hall_Config_t)
 * - sensorData: Points to sensor-specific data struct (e.g., HAL_Hall_Data_t)
 * - hwHandle: Points to sensor-specific handle struct (e.g., HAL_Hall_Handle_t)
 *
 * **Casting in Sensor-Specific Files**:
 * Sensor-specific application files (e.g., hall_sen_foc_app.c) cast these
 * pointers to concrete types:
 *
 * ```c
 * HAL_Hall_Data_t *hallData = (HAL_Hall_Data_t *)obj->halSensorInterface->sensorData;
 * uint32_t hallState = hallData->hallState;
 * ```
 *
 * **Usage in Solutions**:
 * - Hall sensor projects: Wire to HAL_Hall_Config_t, HAL_Hall_Data_t, HAL_Hall_Handle_t
 * - Sensorless projects: Set to NULL_PTR
 * - Encoder projects (future): Wire to HAL_Encoder_Config_t, HAL_Encoder_Data_t, etc.
 * - Resolver projects (future): Wire to HAL_Resolver_Config_t, HAL_Resolver_Data_t, etc.
 * - CAN projects (future): Wire to HAL_CAN_Config_t, HAL_CAN_Data_t, etc.
 */
typedef struct _HAL_Sensor_Interface_t_
{
    void *sensorConfig; /**< Sensor-specific HAL config (e.g., HAL_Hall_Config_t) */
    void *sensorData;   /**< Sensor-specific HAL data (e.g., HAL_Hall_Data_t) */
} HAL_Sensor_Interface_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* HAL_SENSOR_INTERFACE_H */
