/*
 * Copyright (c) 2021 Texas Instruments Incorporated - http://www.ti.com
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
 * \file hal_data.h
 * \brief Hardware Abstraction Layer (HAL) data structures
 *
 * \defgroup HAL_DATA HAL Data Structures
 * \brief Data structures for PWM, ADC, and DAC operations
 * @{
 */

#ifndef HAL_DATA_H
#define HAL_DATA_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#include "math_types.h"

/**
 * \brief PWM data structure for motor control
 *
 * Contains PWM timing values, compare values, and configuration
 * parameters for three-phase PWM generation.
 */
typedef struct _HAL_PWMData_t_
{
    MATH_Vec3  Vabc_pu;       /**< \brief PWM time-durations for each motor phase in per-unit */
    uint16_t   cmpValue[3];   /**< \brief Compare values for PWM channels */
    uint16_t   cmpAValue[3];  /**< \brief Compare A values for PWM channels */
    uint16_t   cmpBValue[3];  /**< \brief Compare B values for PWM channels */
    uint16_t   upSoc[2];      /**< \brief Up-count SOC trigger values */
    uint16_t   downSoc[2];    /**< \brief Down-count SOC trigger values */
    uint16_t   deadband;      /**< \brief Deadband value in PWM clock cycles */
    uint16_t   noiseWindow;   /**< \brief Noise window for current sampling */
    uint16_t   minCMPValue;   /**< \brief Minimum compare value */
    uint16_t   adcSample;     /**< \brief ADC sample timing value */
    uint16_t   period;        /**< \brief PWM period value */
    uint16_t   socCMP;        /**< \brief SOC compare value */
    bool       flagEnablePwm; /**< \brief Flag to enable/disable PWM output */
    MATH_Vec2 *pVab_out_V;    /**< \brief Pointer to alpha-beta output voltages */
} HAL_PWMData_t;

/**
 * \brief ADC data structure for single-shunt current sensing
 *
 * Contains measured values and calibration parameters for
 * single-shunt current reconstruction.
 */
typedef struct _HAL_1S_ADCData_t_
{
    float32_t VdcBus_V;        /**< \brief DC bus voltage in Volts */
    MATH_Vec3 I_A;             /**< \brief Three-phase currents in Amps */
    MATH_Vec3 V_V;             /**< \brief Three-phase voltages in Volts */
    MATH_Vec2 Idc1_A;          /**< \brief DC-link current up-count samples in Amps */
    MATH_Vec2 Idc2_A;          /**< \brief DC-link current down-count samples in Amps */
    float32_t offset_Idc_ad;   /**< \brief DC current offset in ADC counts */
    MATH_Vec3 offset_V_sf;     /**< \brief Voltage offset scale factors */
    float32_t current_sf;      /**< \brief Current scale factor in Amps/count */
    float32_t voltage_sf;      /**< \brief Phase voltage scale factor in Volts/count */
    float32_t dcBusvoltage_sf; /**< \brief DC bus voltage scale factor in Volts/count */
} HAL_1S_ADCData_t;

/**
 * \brief ADC data structure for three-shunt current sensing
 *
 * Contains measured values and calibration parameters for
 * three-shunt current measurement.
 */
typedef struct _HAL_3S_ADCData_t_
{
    float32_t VdcBus_V;        /**< \brief DC bus voltage in Volts */
    MATH_Vec3 I_A;             /**< \brief Three-phase currents in Amps */
    MATH_Vec3 V_V;             /**< \brief Three-phase voltages in Volts */
    MATH_Vec3 offset_I_ad;     /**< \brief Current offsets in ADC counts */
    MATH_Vec3 offset_V_sf;     /**< \brief Voltage offset scale factors */
    float32_t current_sf;      /**< \brief Current scale factor in Amps/count */
    float32_t voltage_sf;      /**< \brief Phase voltage scale factor in Volts/count */
    float32_t dcBusvoltage_sf; /**< \brief DC bus voltage scale factor in Volts/count */
} HAL_3S_ADCData_t;

/**
 * \brief Generic ADC data structure supporting both single and three-shunt
 *
 * Contains measured values and calibration parameters for
 * both single-shunt and three-shunt current sensing configurations.
 */
typedef struct _HAL_ADCData_t_
{
    float32_t VdcBus_V;        /**< \brief DC bus voltage in Volts */
    MATH_Vec3 I_A;             /**< \brief Three-phase currents in Amps */
    MATH_Vec3 V_V;             /**< \brief Three-phase voltages in Volts */
    MATH_Vec2 Idc1_A;          /**< \brief DC-link current up-count samples in Amps */
    MATH_Vec2 Idc2_A;          /**< \brief DC-link current down-count samples in Amps */
    float32_t offset_Idc_ad;   /**< \brief DC current offset in ADC counts */
    MATH_Vec3 offset_I_ad;     /**< \brief Phase current offsets in ADC counts */
    MATH_Vec3 offset_V_sf;     /**< \brief Voltage offset scale factors */
    float32_t current_sf;      /**< \brief Current scale factor in Amps/count */
    float32_t voltage_sf;      /**< \brief Phase voltage scale factor in Volts/count */
    float32_t dcBusvoltage_sf; /**< \brief DC bus voltage scale factor in Volts/count */
} HAL_ADCData_t;

/**
 * \brief Buffered DAC data structure
 *
 * Contains pointers to data sources and scaling parameters
 * for buffered DAC output.
 */
typedef struct _HAL_BuffDACData_t_
{
    volatile float32_t *ptrData[2]; /**< \brief Pointers to data sources */
    float32_t           offset[2];  /**< \brief DAC offset values */
    float32_t           gain[2];    /**< \brief DAC gain values */
} HAL_BuffDACData_t;

/**
 * \brief PWM-based DAC data structure
 *
 * Contains pointers to data sources and scaling parameters
 * for PWM-based DAC output with up to 4 channels.
 */
typedef struct _HAL_PwmDacData_t_
{
    volatile float32_t *ptrData[4]; /**< \brief Pointers to data sources */
    float32_t           offset[4];  /**< \brief DAC offset values */
    float32_t           gain[4];    /**< \brief DAC gain values */
    uint16_t            periodMax;  /**< \brief Maximum PWM period for DAC */
} HAL_PWMDACData_t;

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

/** @} */ /* End of HAL_DATA group */

#endif /* HAL_DATA_H */
