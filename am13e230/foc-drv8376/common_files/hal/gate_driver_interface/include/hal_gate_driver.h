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
 * \file halGateDriver.h
 * \brief Hardware Abstraction Layer for Gate Driver Interface
 *
 * \defgroup HAL_GATE_DRIVER HAL Gate Driver Interface
 * \brief Gate driver abstraction layer for motor control applications
 * @{
 */

#ifndef HAL_GATEDRIVERINTERFACE_INCLUDE_HALGATEDRIVER_H_
#define HAL_GATEDRIVERINTERFACE_INCLUDE_HALGATEDRIVER_H_

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Syscfg includes */
#include "ti_sdk_dl_config.h"

#include "hal.h"

/* math_types.h internally includes <math.h> and fastrts_tmu.h for TMU acceleration */
#include "math_types.h"

/**
 * \brief Defines a float32_t type for ARM M33 compatibility
 */
#ifndef float32_t
typedef float float32_t;
#endif

/**
 * \brief Virtual function table for gate driver operations
 *
 * This structure defines the interface for gate driver initialization
 * and enable operations, allowing different gate driver implementations
 * to be used interchangeably.
 */
typedef struct _HAL_GD_vTable_t_
{
    void (*initGateDriver)(void const *handle);
    void (*enableGateDriver)(void const *handle);

} HAL_GD_vTable_t;

/**
 * \brief Gate driver data object containing configuration parameters
 *
 * This structure holds all the configuration parameters for the gate driver
 * including current sensing, voltage sensing, and protection thresholds.
 */
typedef struct _HAL_GD_DATA_Obj_
{
    float32_t gdMaxCurrentA;          /**< \brief Maximum current limit in Amps */
    float32_t gdCurrentSF;            /**< \brief Current scale factor */
    float32_t gdCurrSensDelay;        /**< \brief Current sensing delay */
    float32_t gdCurrSampDelay;        /**< \brief Current sampling delay */
    float32_t gdCurrOffset;           /**< \brief Current offset value */
    float32_t gdCurrSign;             /**< \brief Current sign correction */
    float32_t gdOverVolVal;           /**< \brief Over voltage threshold */
    float32_t gdUnderVolVal;          /**< \brief Under voltage threshold */
    float32_t gdVolSF;                /**< \brief Voltage scale factor */
    float32_t gdPhVoltOffsetSF;       /**< \brief Phase voltage offset scale factor */
    float32_t gdVolFilterPoleFreqRPS; /**< \brief Voltage filter pole frequency in rad/s */

} HAL_GD_DATA_Obj;

/**
 * \brief Handle to gate driver data object
 */
typedef struct _HAL_GD_DATA_Obj_ *HAL_GD_DATA_Handle;

/**
 * \brief Gate driver interface structure
 *
 * This structure represents the interface between the motor controller
 * and the gate driver. It provides access to the virtual function table
 * and configuration data for gate driver operations.
 */
typedef struct _HAL_GD_Interface_t_
{
    /**
     * \brief Pointer to the virtual function table
     */
    const HAL_GD_vTable_t *const vGDTable;

    /**
     * \brief Handle to the gate driver instance
     */
    const void *const halGDHandle;

    /**
     * \brief Pointer to gate driver data object
     */
    const HAL_GD_DATA_Obj *const gdDataHandle;

} HAL_GD_Interface_t;

/* Static inline functions use regular C comments per coding guidelines */

/* Initializes the gate driver using the virtual function table */
static inline void HAL_initGateDriver(const HAL_GD_Interface_t *interfaceHandle)
{
    interfaceHandle->vGDTable->initGateDriver(interfaceHandle->halGDHandle);
}

/* Enables the gate driver using the virtual function table */
static inline void HAL_enableGateDriver(const HAL_GD_Interface_t *interfaceHandle)
{
    interfaceHandle->vGDTable->enableGateDriver(interfaceHandle->halGDHandle);
}

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

/** @} */ /* End of HAL_GATE_DRIVER group */

#endif /* HAL_GATEDRIVERINTERFACE_INCLUDE_HALGATEDRIVER_H_ */
