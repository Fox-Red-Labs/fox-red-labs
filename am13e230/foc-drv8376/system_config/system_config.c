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
 * \file motor_config.c
 * \brief Motor hardware configuration for the 3-shunt Hall FOC port to the DRV8376EVM
 *
 * Defines all HAL objects for Motor 1: FOC peripheral config, DRV8376 gate
 * driver, and Hall sensor interface. All SysConfig-generated symbols are
 * resolved from the project-local ti_sdk_dl_config.h. No preprocessor
 * conditionals — Hall support is unconditional in this project.
 */

#include "system_config.h"
#include "mtr_config.h"
#include "application.h"
#include "ti_sdk_dl_config.h"

/**
 * \anchor HALL_3SH_INT_DEFINES
 * \name Interrupt Configuration Defines
 * @{
 */
/** @} */

/* ============================================================================
 * FOC Peripheral Configuration
 * ============================================================================ */

/**
 * \brief FOC configuration structure for Motor 1 (three-shunt mode)
 *
 * Maps SysConfig-generated ADC, PWM, comparator, ECAP, and interrupt symbols
 * to the HAL_FOC_Config_t structure for three-shunt current sensing.
 */
const HAL_FOC_Config_t halFOCConfig_M1 = {
    /* ADC Current Sensing Configuration */
    .adcCurrentCfg[0].adcResultBase = (uint32_t)MTR1_IU_SEN_ADC_RESULT_BASE, /**< Phase U current ADC result */
    .adcCurrentCfg[1].adcResultBase = (uint32_t)MTR1_IV_SEN_ADC_RESULT_BASE, /**< Phase V current ADC result */
    .adcCurrentCfg[2].adcResultBase = (uint32_t)MTR1_IW_SEN_ADC_RESULT_BASE, /**< Phase W current ADC result */
    .adcCurrentCfg[0].socNum        = (uint16_t)MTR1_IU_SEN,                 /**< Phase U SOC number */
    .adcCurrentCfg[1].socNum        = (uint16_t)MTR1_IV_SEN,                 /**< Phase V SOC number */
    .adcCurrentCfg[2].socNum        = (uint16_t)MTR1_IW_SEN,                 /**< Phase W SOC number */

    /* ADC Voltage Sensing Configuration */
    .adcVoltageCfg[0].adcResultBase = (uint32_t)MTR1_VU_SEN_ADC_RESULT_BASE,  /**< Phase U voltage ADC result */
    .adcVoltageCfg[1].adcResultBase = (uint32_t)MTR1_VV_SEN_ADC_RESULT_BASE,  /**< Phase V voltage ADC result */
    .adcVoltageCfg[2].adcResultBase = (uint32_t)MTR1_VW_SEN_ADC_RESULT_BASE,  /**< Phase W voltage ADC result */
    .adcVoltageCfg[3].adcResultBase = (uint32_t)MTR1_VDC_SEN_ADC_RESULT_BASE, /**< DC bus voltage ADC result */
    .adcVoltageCfg[0].socNum        = (uint16_t)MTR1_VU_SEN,                  /**< Phase U voltage SOC number */
    .adcVoltageCfg[1].socNum        = (uint16_t)MTR1_VV_SEN,                  /**< Phase V voltage SOC number */
    .adcVoltageCfg[2].socNum        = (uint16_t)MTR1_VW_SEN,                  /**< Phase W voltage SOC number */
    .adcVoltageCfg[3].socNum        = (uint16_t)MTR1_VDC_SEN,                 /**< DC bus voltage SOC number */

    /* PWM Configuration */
    .pwmCfg.pwmHandle                    = (uint32_t)MTR1_MCPWM_BASE_BASE,    /**< PWM peripheral base address */
    .pwmCfg.MotorStoptripInputXBARSource = (uint16_t)DL_XBAR_INPUT1,          /**< Motor stop trip XBAR source */
    .pwmCfg.overCurrentTZSouce           = (uint32_t)DL_MCPWM_TZ_SIGNAL_OST3, /**< Overcurrent trip zone source */
    .pwmCfg.GateDriverFaultTZSource      = (uint32_t)DL_MCPWM_TZ_SIGNAL_OST2, /**< Gate driver fault TZ source */
    .pwmCfg.MotorStopTZSource            = (uint32_t)DL_MCPWM_TZ_SIGNAL_OST1, /**< Motor stop trip zone source */
    .pwmCfg.pwmDownCountTriggerSource = &((MCPWM_Regs *)MTR1_MCPWM_BASE_BASE)->CMPCS, /**< Down count trigger source */
    .pwmCfg.adcTrigPWMSource          = (DL_MCPWM_SOC_TYPE)DL_MCPWM_SOC_A,            /**< ADC trigger PWM source */

    /* Gate Driver propagation delay*/
    .pwmCfg.gateDriverDelay = DRV8376_GD_PROP_DELAY * USER_SYSTEM_FREQ_MHz,

    /* Comparator Configuration for Overcurrent Protection */
    .cmpCfg.cmpssHandle[0] = (uint32_t)MTR1_CMPSS_U_BASE_BASE, /**< Phase U comparator base */
    .cmpCfg.cmpssHandle[1] = (uint32_t)MTR1_CMPSS_V_BASE_BASE, /**< Phase V comparator base */
    .cmpCfg.cmpssHandle[2] = (uint32_t)MTR1_CMPSS_W_BASE_BASE, /**< Phase W comparator base */
    .cmpCfg.compType[0]    = COMP_HIGH,                        /**< Phase U comparator type */
    .cmpCfg.compType[1]    = COMP_HIGH,                        /**< Phase V comparator type */
    .cmpCfg.compType[2]    = COMP_HIGH,                        /**< Phase W comparator type */

    /* ECAP Configuration for IPD */
    .ecapIPDConfig.base      = (uint32_t)IPD_CAPTURE_BASE, /**< ECAP base for IPD pulse capture */
    .ecapIPDConfig.highEvent = DL_ECAP_EVENT_1,            /**< ECAP event for IPD measurement */

    /* Interrupt Configuration */
    .intCfg.intNVICNum        = motor1CtrlISR,                  /**< NVIC interrupt number */
    .intCfg.intPeripheralBase = (uint32_t)MTR1_VV_SEN_ADC_BASE, /**< Interrupt peripheral base: ADC2, whose INT1 fires after the last conversion (SOC2) */
    .intCfg.intPeripheralNum  = (uint16_t)DL_ADC_INT_NUMBER1,   /**< Interrupt number */

    /* DC Link Configuration (not used in 3-shunt) */
    .dclinkHandle = NULL,

    /* PWM Timing Configuration */
    .pwmCfg.pwmPeriod             = USER_M1_PWM_TBPRD_NUM,              /**< PWM period value */
    .pwmCfg.numPWMTicksPerISRTick = USER_M1_NUM_PWM_TICKS_PER_ISR_TICK, /**< PWM ticks per ISR */

    .dacInst = (uint32_t)MTR1_CMPSS_U_BASE_BASE,
};

/* ============================================================================
 * Gate Driver Objects
 * ============================================================================ */

/**
 * \brief DRV8376 gate driver instance for Motor 1
 */
const HAL_DRV8376_Obj drv8376Inst_M1 = {
    .gateNSleepGPIO       = (uint32_t)MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT_BASE, /**< nSLEEP GPIO port (PA23) */
    .gateNSleepGPIONum    = (uint32_t)MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN,       /**< nSLEEP GPIO pin */
    .gateDriverOffGPIO    = (uint32_t)MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT_BASE, /**< DRVOFF GPIO port (PB18) */
    .gateDriverOffGPIONum = (uint32_t)MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN,       /**< DRVOFF GPIO pin */
};

/**
 * \brief Gate driver interface instance for Motor 1
 */
const HAL_GD_Interface_t halGDInst = {
    .vGDTable     = &halDRV8376FnTable, /**< Gate driver function table */
    .halGDHandle  = &drv8376Inst_M1,    /**< Gate driver instance handle */
    .gdDataHandle = &drv8376Data_M1,    /**< Gate driver data handle */
};

/**
 * \brief FOC data object for Motor 1
 */
HAL_FOC_Data_Obj halFOCDataObj_M1;

/**
 * \brief FOC interface instance for Motor 1
 */
const HAL_FOC_Interface_t halFOCInterface = {
    .vFOCTable          = &hal3SFOCFnTable,  /**< FOC function table */
    .halFOCDataHandle   = &halFOCDataObj_M1, /**< FOC data handle */
    .halFOCConfigHandle = &halFOCConfig_M1,  /**< FOC configuration handle */
};

/* ============================================================================
 * Hall Sensor Objects
 * ============================================================================ */

/**
 * \brief Hall sensor hardware configuration for Motor 1 (const, placed in flash)
 *
 * Maps SysConfig-generated GPIO and ECAP symbols to HAL_Hall_Config_t.
 * GPIO port and pin names are generated from the MTR1_HALL_INP GPIO group.
 * ECAP base is generated from the HALL_SPEED_CAP ECAP instance.
 */
/* FRL port (2026-10-17): not const. After a valid Hall calibration, hall_sen_foc_app.c writes the table pointers
 * into this object; as const it is linked into flash (.rodata) and that store hard-faults. */
HAL_Hall_Config_t hallConfig_M1 = {
    .hallAGpioPort = (uint32_t)MTR1_HALL_A_PORT_BASE,
    .hallBGpioPort = (uint32_t)MTR1_HALL_B_PORT_BASE,
    .hallCGpioPort = (uint32_t)MTR1_HALL_C_PORT_BASE,
    .hallPinA      = MTR1_HALL_A_PIN,
    .hallPinB      = MTR1_HALL_B_PIN,
    .hallPinC      = MTR1_HALL_C_PIN,
    .ecapBase      = (uint32_t)
        HALL_SPEED_CAP_BASE, /**< Use _BASE (integer constant) not _INST (pointer) for IAR const initializer */
    /* ECAP input XBAR selects for dynamic next-edge capture.
     * ecapInputHall[0] = Hall A, [1] = Hall B, [2] = Hall C.
     * Each entry holds the DL_ECAP_INPUT enum value (cast to uint32_t) for the
     * Input XBAR routed to the corresponding Hall GPIO pin.
     * Defines generated by SysConfig from the three HALL_XBAR_A/B/C instances. */
    .ecapInputHall =
        {
            (uint32_t)HALL_SPEED_CAP_ECAP_INPUT_HALL_A, /* [0] Hall A */
            (uint32_t)HALL_SPEED_CAP_ECAP_INPUT_HALL_B, /* [1] Hall B */
            (uint32_t)HALL_SPEED_CAP_ECAP_INPUT_HALL_C  /* [2] Hall C */
        },
    /* ECAP capture event — must match SysConfig HALL_SPEED_CAP wrapOrStopAtEvent.
     * DL_ECAP_EVENT_1 = 0U. Stored as uint32_t; cast to DL_ECAP_EVENT at use site. */
    .ecapCaptureEvent    = (uint32_t)DL_ECAP_EVENT_1,
    .calAngleFwdTablePtr = NULL,
    .calAngleRevTablePtr = NULL,
    .numPolePairs        = (uint16_t)USER_M1_HALL_NUM_POLE_PAIRS,
    .angleOffset_rad     = USER_M1_HALL_ANGLE_OFFSET_rad,
    .sysclkFreq_MHz      = USER_SYSTEM_FREQ_MHz,   /**< System clock — from user_mtr1.h, passed to HAL */
    .isrPeriod_usec      = USER_M1_ISR_PERIOD_usec /**< ISR period — from user_mtr1.h, passed to HAL */
};

/**
 * \brief Hall sensor runtime data for Motor 1 (mutable, placed in RAM)
 *
 * Zero-initialized at startup; mutated by HAL_Hall_readState_inline()
 * and HAL_Hall_computeAngle_inline() every ISR cycle.
 */
HAL_Hall_Data_t hallData_M1 = {0};

/**
 * \brief Hall sensor interface for Motor 1
 *
 * Binds the const config (flash) and mutable data (RAM) into a single
 * const-pointer interface object, enabling LTO const-propagation.
 */
const HAL_Hall_Interface_t hallInterface_M1 = {.hallConfigHandle = &hallConfig_M1, .hallDataHandle = &hallData_M1};
