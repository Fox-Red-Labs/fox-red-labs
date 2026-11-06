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
 * \file app_sensor_interface.h
 * \brief Generic application-layer sensor interface for calibration and state
 *
 * This interface provides a type-erased wrapper for sensor-specific
 * application implementations (Hall calibration, encoder calibration, etc.).
 * Each sensor type provides concrete implementations of appSensorConfig, appSensorData,
 * and calHandle pointers.
 *
 * \defgroup APP_SENSOR_INTERFACE Application Sensor Interface
 * \brief Generic interface for sensor application implementations
 * @{
 */

#ifndef APP_SENSOR_INTERFACE_H
#define APP_SENSOR_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * \brief Generic application-layer sensor interface structure
 *
 * This structure provides type-erased pointers to sensor-specific
 * application configuration, data, and handle structures. Each sensor type
 * (Hall, encoder, resolver, CAN) implements these pointers with
 * concrete types.
 *
 * **Type Erasure Pattern**:
 * - appSensorConfig: Points to sensor-specific calibration config (e.g., HallCal_Config_t)
 * - appSensorData: Points to sensor-specific calibration data (e.g., HallCal_Data_t)
 * - calHandle: Points to sensor-specific calibration handle (e.g., HallCal_Handle_t)
 *
 * **Casting in Sensor-Specific Files**:
 * Sensor-specific application files (e.g., hall_sen_foc_app.c) cast these
 * pointers to concrete types:
 *
 * ```c
 * HallCal_Config_t *hallCalCfg = (HallCal_Config_t *)obj->appSensorInterface->appSensorConfig;
 * HallCal_Data_t *hallSensorData = (HallCal_Data_t *)obj->appSensorInterface->appSensorData;
 * ```
 *
 * **Usage in Solutions**:
 * - Hall sensor projects: Wire to HallCal_Config_t, HallCal_Data_t, HallCal_Handle_t
 * - Sensorless projects: Set to NULL_PTR
 * - Encoder projects (future): Wire to EncoderCal_Config_t, EncoderCal_Data_t, etc.
 * - Resolver projects (future): Wire to ResolverCal_Config_t, ResolverCal_Data_t, etc.
 * - CAN projects (future): Wire to CANSensor_Config_t, CANSensor_Data_t, etc.
 */
typedef struct _APP_Sensor_Interface_t_
{
    void *appSensorConfig; /**< Sensor-specific cal config (e.g., HallCal_Config_t) */
    void *appSensorData;   /**< Sensor-specific cal data (e.g., HallCal_Data_t) */
} APP_Sensor_Interface_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_INTERFACE_H */
