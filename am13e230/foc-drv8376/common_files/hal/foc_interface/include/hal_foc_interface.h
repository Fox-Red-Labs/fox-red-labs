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
 * \file hal_foc_interface.h
 * \brief HAL FOC Interface definitions and functions
 *
 * \defgroup HAL_FOC_INTERFACE_MODULE HAL FOC Interface Module
 * \brief FOC hardware abstraction layer interface
 * @{
 */

#ifndef HAL_CONTROLLERINTERFACE_INCLUDE_HAL_FOCINTERFACE_H_
#define HAL_CONTROLLERINTERFACE_INCLUDE_HAL_FOCINTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_obj.h"
#include "hal.h"
#include "userParams.h"
#include "dclink_ss.h"
#include "hal_gate_driver.h"

/**
 * \anchor HALL_TUNING_CONSTANTS
 * \name Hall Sensor Tuning Constants for Motor 1
 * \brief Tune these values at commissioning for the specific motor and mounting.
 *        Moved here from user_mtr1.h — these are hall-project-specific, not shared.
 * @{
 */

/*! \brief Hall sensor speed filter pole frequency in rad/s (50 Hz cutoff) */
#define USER_M1_HALL_SPEED_FILTER_POLE_rps (MATH_TWO_PI * 10.0f)

/*! \brief Hall sensor electrical angle mounting offset in radians
 *         Motor-specific — tune at commissioning to align Hall sector boundaries
 *         with the actual rotor electrical angle. */
#define USER_M1_HALL_ANGLE_OFFSET_rad (0.0f)

/*! \brief Hall sensor pole pair count — must match motor nameplate */
#define USER_M1_HALL_NUM_POLE_PAIRS (4U) /* TODO: set to match motor pole pair count */

/** @} */

/**
 * \brief HAL FOC data object
 */
typedef struct _HAL_FOC_Data_Obj_
{
    /** \brief Motor instance number */
    MotorNum_e motorNum;

    /** \brief Number of phase current sensors */
    uint16_t numCurrentSensors;
    /** \brief Number of DC-bus voltage sensors */
    uint16_t numVoltageSensors;

    /** \brief Board kit variant identifier */
    Board_Kit_e boardKit;

    /** \brief Current sensing topology (three-shunt, single-shunt, etc.) */
    CURRENTSEN_Type_e currentSenseType;

    /** \brief PWM duty-cycle data written by SetPhaseVoltages */
    HAL_PWMData_t pwmData;

    /** \brief Set TRUE to enable ADC offset calibration */
    bool flagEnableOffsetCalc;

    /** \brief Pointer to the over-current trip threshold in Amps */
    float32_t    *pOverCurLimA;
    /** \brief IPD injection current limit in Amps */
    float32_t     ipdCurrentLimit;
    /** \brief ADC phase-current and bus-voltage results */
    HAL_ADCData_t adcData;

    /**
     * \brief Comparator DAC trip-point offsets in raw DAC counts (0–4095).
     * Index mapping: [0]=phase-A/DC-link, [1]=phase-B, [2]=phase-C.
     * For single-shunt only index [0] is used.
     */
    uint16_t compOffsetData[3];

    /** \brief Read-only handle to gate driver data (fault flags, status) */
    const HAL_GD_DATA_Obj *gdDataHandle;

    /** \brief ECAP capture value for the last IPD pulse width */
    uint32_t ecapIPDPulseTime;

} HAL_FOC_Data_Obj;

//! \brief Handle to HAL FOC Data Object
typedef struct _HAL_FOC_Data_Obj_ *HAL_FOC_Data_Handle;

/**
 * \brief HAL FOC configuration structure
 */
typedef struct _HAL_FOC_Config_t
{
    /** \brief PWM interrupt and ADC SOC trigger configuration */
    HAL_INT_Config_t intCfg;

    /** \brief PWM peripheral configuration (period, dead-band, trip zone) */
    const HAL_PWM_Config_t pwmCfg;

    /** \brief ADC result configuration for up to 4 phase-current channels */
    HAL_ADC_Result_Config_t adcCurrentCfg[4];

    /** \brief ADC result configuration for up to 4 bus-voltage channels */
    const HAL_ADC_Result_Config_t adcVoltageCfg[4];

    /** \brief Comparator configuration for hardware over-current protection */
    const HAL_3S_CMP_Config_t cmpCfg;

    /** \brief DC-link single-shunt reconstruction object handle */
    const DCLINK_SS_Handle dclinkHandle;

    /** \brief ECAP configuration used to measure IPD injection pulse width */
    const HAL_ECAP_Config_t ecapIPDConfig;

    /** \brief DAC instance index used for comparator threshold debug output */
    uint32_t dacInst;
} HAL_FOC_Config_t;

//! \brief Handle to HAL FOC Configuration
typedef struct _HAL_FOC_Config_t *HAL_FOC_Config_Handle;

/**
 * \brief HAL FOC virtual function table
 */
typedef struct _HAL_FOC_vTable_t_
{
    /** \brief Write PWM compare values for the three inverter phases */
    void (*SetPhaseVoltages)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Sample and scale DC-bus and phase voltages into adcData */
    void (*ReadInverterVoltages)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Sample and scale phase currents into adcData */
    void (*ReadPhaseCurrents)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Program comparator DAC thresholds for hardware over-current trip */
    void (*SetCompOCLimits)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief One-time peripheral initialization (PWM, ADC, CMP, ECAP) */
    void (*Init)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Prepare offset calibration accumulators before the calibration loop */
    void (*RunOffsetCalInit)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Accumulate one ADC sample for offset calibration; returns 1 when complete */
    uint8_t (*RunOffsetCalStep)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle,
                                uint32_t calibTickCnt, uint32_t calibTotalCnt);
    /** \brief Update PWM period and compare registers for the current control period */
    void (*ConfigPWM)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Clear latched motor fault flags in the gate driver and peripherals */
    void (*clearMtrFaultStatus)(const HAL_FOC_Config_t *configHandle);
    /** \brief Apply the specified voltage vector for an IPD injection pulse */
    void (*ipdPulseConfig)(VOLTAGE_VECTOR voltageVector, const HAL_FOC_Config_t *configHandle,
                           HAL_FOC_Data_Obj *dataHandle);
    /** \brief Check whether the IPD injection pulse has completed; returns 1 when done */
    uint8_t (*ipdPulseDone)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Initialize IPD peripherals and state before the injection sequence */
    void (*ipdInit)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
    /** \brief Restore normal PWM operation after the IPD injection sequence */
    void (*ipdExit)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);

    /**
     * \brief One-time comparator DAC offset calibration setup.
     *
     * Sets each comparator DAC to the sweep start value, clears latches,
     * and captures the initial comparator status for each channel.
     * Must be called once before the first call to compOffsetCalStep.
     */
    void (*compOffsetCalInit)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);

    /**
     * \brief Per-call comparator DAC sweep step.
     *
     * Waits 2 ms, reads comparator status for each unresolved channel,
     * and increments the DAC until a trip-point transition is detected.
     * The found DAC value is stored in dataHandle->compOffsetData[index].
     *
     * \return 1 when all channels are resolved, 0 while still sweeping.
     */
    uint8_t (*compOffsetCalStep)(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);

} HAL_FOC_vTable_t;

//! \brief Handle to HAL FOC Virtual Function Table
typedef struct _HAL_FOC_vTable_t_ *HAL_FOC_vTable_Handle_t;

/**
 * \brief HAL FOC interface — binds vtable, config, and data object
 */
typedef struct _HAL_FOC_Interface_t_
{
    /** \brief Pointer to the topology-specific virtual function table (const address) */
    const HAL_FOC_vTable_t *const vFOCTable;
    /** \brief Pointer to the read-only hardware configuration in flash (const address) */
    const HAL_FOC_Config_t *const halFOCConfigHandle;
    /** \brief Pointer to the mutable runtime data object in RAM (const address) */
    HAL_FOC_Data_Obj *const       halFOCDataHandle;

} HAL_FOC_Interface_t;

// #TAG_THREE_SHUNT_START#
#if defined(MOTOR_THREE_SHUNT)

#include "hal_3sh_foc_interface.h"

/**
 * \brief Set phase voltages (three-shunt)
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_SetPhaseVoltages(const HAL_FOC_Interface_t *interfaceHandle)
{
    HAL_3S_FOC_SetPhaseVoltages_inline(interfaceHandle->halFOCConfigHandle, interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Read inverter voltages (three-shunt)
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_ReadInverterVoltages(const HAL_FOC_Interface_t *interfaceHandle)
{
    HAL_3S_FOC_ReadInverterVoltages_inline(interfaceHandle->halFOCConfigHandle, interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Read phase currents (three-shunt)
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_ReadPhaseCurrents(const HAL_FOC_Interface_t *interfaceHandle)
{
    HAL_3S_FOC_ReadPhaseCurrents_inline(interfaceHandle->halFOCConfigHandle, interfaceHandle->halFOCDataHandle);
}
// #TAG_THREE_SHUNT_END#

// #TAG_SINGLE_SHUNT_START#
#elif defined(MOTOR_SINGLE_SHUNT)

#include "hal_1sh_foc_interface.h"

/**
 * \brief Set phase voltages (single-shunt)
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_SetPhaseVoltages(const HAL_FOC_Interface_t *interfaceHandle)
{
    HAL_1S_FOC_SetPhaseVoltages_inline(interfaceHandle->halFOCConfigHandle, interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Read inverter voltages (single-shunt)
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_ReadInverterVoltages(const HAL_FOC_Interface_t *interfaceHandle)
{
    HAL_1S_FOC_ReadInverterVoltages_inline(interfaceHandle->halFOCConfigHandle, interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Read phase currents (single-shunt)
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_ReadPhaseCurrents(const HAL_FOC_Interface_t *interfaceHandle)
{
    HAL_1S_FOC_ReadPhaseCurrents_inline(interfaceHandle->halFOCConfigHandle, interfaceHandle->halFOCDataHandle);
}
// #TAG_SINGLE_SHUNT_END#

// #TAG_MULTI_TOPOLOGY_START#
#elif defined(MOTOR_MULTI_TOPOLOGY)

/**
 * \brief Set phase voltages — vtable dispatch for mixed topologies
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_SetPhaseVoltages(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->SetPhaseVoltages(interfaceHandle->halFOCConfigHandle,
                                                 interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Read inverter voltages — vtable dispatch for mixed topologies
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_ReadInverterVoltages(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->ReadInverterVoltages(interfaceHandle->halFOCConfigHandle,
                                                     interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Read phase currents — vtable dispatch for mixed topologies
 * \param[in] interfaceHandle  FOC interface handle
 */
__attribute__((always_inline)) static inline void HAL_ReadPhaseCurrents(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->ReadPhaseCurrents(interfaceHandle->halFOCConfigHandle,
                                                  interfaceHandle->halFOCDataHandle);
}
// #TAG_MULTI_TOPOLOGY_END#

#else
#error "hal_foc_interface.h: define MOTOR_THREE_SHUNT, MOTOR_SINGLE_SHUNT, or MOTOR_MULTI_TOPOLOGY"
#endif /* topology selector */

/* Sets the comparator overcurrent limits */
static inline void HAL_SetCompOCLimits(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->SetCompOCLimits((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                                (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Initializes the FOC HAL */
static inline void HAL_Init(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->Init((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                     (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Runs one-time offset calibration setup */
static inline void HAL_RunOffsetCalInit(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->RunOffsetCalInit((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                                 (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Runs one ISR-tick step of offset calibration; returns 1 when done */
static inline uint8_t HAL_RunOffsetCalStep(const HAL_FOC_Interface_t *interfaceHandle, uint32_t calibTickCnt,
                                           uint32_t calibTotalCnt)
{
    return interfaceHandle->vFOCTable->RunOffsetCalStep((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                                        (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle,
                                                        calibTickCnt, calibTotalCnt);
}

/* Configures PWM period and compare units */
static inline void HAL_ConfigPWM(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->ConfigPWM((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                          (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Clears motor fault status */
static inline void HAL_clearMtrFaultStatus(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->clearMtrFaultStatus((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle);
}

/* Forces PWM outputs low for IPD */
static inline void HAL_IPDForcePWMLow(const HAL_PWM_Config_t *pwmCfg)
{
    HAL_forcePWMLow(pwmCfg);
}

/* Initializes IPD */
static inline void HAL_ipdInit(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->ipdInit((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                        (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Configures IPD pulse for the specified voltage vector */
static inline void HAL_ipdPulseConfig(VOLTAGE_VECTOR voltageVector, const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->ipdPulseConfig((VOLTAGE_VECTOR)voltageVector,
                                               (HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                               (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Exits IPD mode */
static inline void HAL_ipdExit(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->ipdExit((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                        (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/* Checks if IPD pulse is done; returns 1 if done */
static inline uint8_t HAL_ipdPulseDone(const HAL_FOC_Interface_t *interfaceHandle)
{
    return (interfaceHandle->vFOCTable->ipdPulseDone((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                                     (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle));
}

/**
 * \brief One-time comparator DAC offset calibration setup.
 *
 * Sets each comparator DAC to the sweep start value, clears latches,
 * and captures the initial comparator status for each channel.
 * Must be called once before the first call to HAL_CompOffsetCalStep().
 *
 * \param[in] interfaceHandle  FOC interface handle
 */
static inline void HAL_CompOffsetCalInit(const HAL_FOC_Interface_t *interfaceHandle)
{
    interfaceHandle->vFOCTable->compOffsetCalInit((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                                  (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

/**
 * \brief Per-call comparator DAC sweep step.
 *
 * Waits 2 ms, reads comparator status for each unresolved channel,
 * and increments the DAC until a trip-point transition is detected.
 * The found DAC value is stored in halFOCDataHandle->compOffsetData[index].
 *
 * \param[in] interfaceHandle  FOC interface handle
 *
 * \return 1 when all channels are resolved, 0 while still sweeping.
 */
static inline uint8_t HAL_CompOffsetCalStep(const HAL_FOC_Interface_t *interfaceHandle)
{
    return interfaceHandle->vFOCTable->compOffsetCalStep((HAL_FOC_Config_t *)interfaceHandle->halFOCConfigHandle,
                                                         (HAL_FOC_Data_Obj *)interfaceHandle->halFOCDataHandle);
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_CONTROLLERINTERFACE_INCLUDE_HAL_FOCINTERFACE_H_ */

/** @} */
