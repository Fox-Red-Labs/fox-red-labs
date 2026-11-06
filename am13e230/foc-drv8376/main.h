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

#ifndef MAIN_H
#define MAIN_H

//**************************************************************************
//! \file  solutions/universal_motorcontrol_lab/common/include/sys_main.h
//! \brief  header file to be included in all labs
//!
//**************************************************************************

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
//
//! \defgroup SYS MAIN
//! @{
//
//*****************************************************************************
// modules

#include "math.h"

#include "motor_common.h"
#include "application.h"

#include "datalogIF.h"
#include "cpu_time.h"
#include "hal_debug_interface.h"

#define LED_BLINK_FREQ_Hz        (0.5f)  // 1Hz
#define POWER_RELAY_WAIT_TIME_ms (1000)  // 1s

/* ISR ticks between low-priority passes. The SDK expression 1000 / PWM_kHz gives 20 ticks at 50 kHz, a pass about
 * every 0.42 ms, and the fault and wait counters are counted in passes. 0.4 ms x PWM_kHz gives the same 20 ticks at
 * 50 kHz and 8 ticks at 20 kHz, so those timers keep their wall-clock values. */
#define LOW_PRIORITY_WAIT_TIME_1ms (uint16_t)(0.4f * USER_M1_PWM_FREQ_kHz)

extern CPU_TIME_Obj    cpuTime;
extern CPU_TIME_Handle cpuTimeHandle;

/**
 * \anchor MAIN_SYSTEM_VARS
 * \name System Variables Structure and Declarations
 * @{
 */

/**
 * \brief System-level variables structure for motor control application
 *
 * Application-level definition moved from system_config.h to main.h.
 * Only fields used by the application are included.
 * The instance (systemVars) is instantiated in main.c.
 * isr.c increments timerCntFOCCtrlISR each ISR cycle.
 */
typedef struct _SYSTEM_Vars_t_
{
    float32_t speedRef_Hz;  //!< System speed reference in Hz

    /* Timer counters */
    uint32_t mainLoopCnt;    //!< Main loop execution counter
    uint32_t timerCnt_1min;  //!< 1 minute timer counter

    uint16_t timerBase_1ms;  //!< 1 millisecond timer base
    uint16_t timerCnt_5ms;   //!< 5 millisecond timer counter
    uint16_t timerCnt_1s;    //!< 1 second timer counter

    volatile uint16_t timerCntFOCCtrlISR;  //!< FOC control ISR timer counter

    uint16_t powerRelayWaitTime_ms;  //!< Power relay wait time in milliseconds

    /* System flags */
    bool flagInitializeDone;  //!< System initialization complete flag

    volatile bool flagEnableSystem;  //!< System enable flag (set by ISR on calib completion)

    /* DAC control structures */
    volatile RAM_DAC_CNTRL_T dacCtrl1;  //!< DAC channel 1 control structure
    volatile RAM_DAC_CNTRL_T dacCtrl2;  //!< DAC channel 2 control structure

    /* Debug configuration */
    HAL_DebugConfig_t debugConfig;  //!< Pre-configured HAL debug configuration

    /* Debug variables */
    uint8_t  serialPlotBuffer[10];  //!< Serial plot buffer array
    uint32_t data1;                 //!< Debug data variable 1
    uint32_t data2;                 //!< Debug data variable 2
    uint16_t idx;                   //!< General purpose index
    uint32_t dacInst;               //!< DAC peripheral instance
    uint32_t commUartInstPtr;       //!< MotorVision GUI UART peripheral instance (cast of UART_Regs*)

} SYSTEM_Vars_t;

/** \brief Motor control variables for Motor 1 (instantiated in main.c) */
extern MOTOR_Vars_t motorVars_M1;

/** \brief System-level runtime variables (instantiated in main.c) */
extern SYSTEM_Vars_t systemVars;

/** @} */

/**
 * \anchor MAIN_DEBUG_FUNCTIONS
 * \name Application-Level Debug Functions
 * @{
 */

/**
 * \brief Initialize DAC outputs for debug monitoring
 *
 * Configures DAC channels for oscilloscope monitoring of motor variables.
 * Sets up DAC control structures with scaling factors and output addresses.
 * Initializes the HAL_DebugConfig_t structure once to avoid re-configuration
 * on every execution of runMotorDataLogging.
 *
 * \param[inout] sysVarsPtr Pointer to system variables structure
 * \param[in] motorHandle Handle to motor instance
 */
static inline void initDACOutputs(SYSTEM_Vars_t *sysVarsPtr, MOTOR_Handle motorHandle)
{
    MOTOR_Vars_t *obj     = (MOTOR_Vars_t *)motorHandle;
    USER_Params  *objUser = (USER_Params *)(motorHandle->userParamsHandle);

    sysVarsPtr->dacInst        = (uint32_t)motorHandle->halInterface->halFOCConfigHandle->dacInst;
    sysVarsPtr->dacCtrl1.dacEn = 0U;  /* DRV8376 port: the debug DAC output (PA4) is dropped, nothing to drive */

    sysVarsPtr->dacCtrl1.pDacOutAddr      = (float32_t *)&obj->outVars.angleFOC_rad;
    sysVarsPtr->dacCtrl1.dacScalingFactor = (DAC_MAX_VALUE / MATH_TWO_PI);

    sysVarsPtr->dacCtrl2.pDacOutAddr      = (float32_t *)&obj->outVars.IdqRef_A.value[0];
    sysVarsPtr->dacCtrl2.dacScalingFactor = (DAC_MAX_VALUE / objUser->maxCurrent_A);

    /* Initialize debug configuration once */
    sysVarsPtr->debugConfig.dacInst           = sysVarsPtr->dacInst;
    sysVarsPtr->debugConfig.pDacCtrl1         = (volatile RAM_DAC_CNTRL_T *)&sysVarsPtr->dacCtrl1;
    sysVarsPtr->debugConfig.pDacCtrl2         = (volatile RAM_DAC_CNTRL_T *)&sysVarsPtr->dacCtrl2;
    sysVarsPtr->debugConfig.pSerialPlotBuffer = &sysVarsPtr->serialPlotBuffer[0U];
    sysVarsPtr->debugConfig.pData1            = &sysVarsPtr->data1;
    sysVarsPtr->debugConfig.pData2            = &sysVarsPtr->data2;
    sysVarsPtr->debugConfig.pIdx              = &sysVarsPtr->idx;
}

/**
 * \brief Run motor data logging and debug output
 *
 * Orchestrates debug data output via DAC and serial plot interfaces.
 * Called from main loop to update oscilloscope and serial plot displays.
 *
 * Uses pre-configured HAL_DebugConfig_t from initDACOutputs to avoid
 * unnecessary pointer re-configuration on every execution.
 *
 * \param[inout] sysVarsPtr Pointer to system variables structure
 */
static inline void runMotorDataLogging(SYSTEM_Vars_t *sysVarsPtr)
{
    /* Call HAL functions with pre-configured debug configuration */
    HAL_updateDACOutputs(&sysVarsPtr->debugConfig);
    HAL_updateSerialPlot(&sysVarsPtr->debugConfig);
}

/** @} */

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif  // extern "C"

#endif  // end of SYS_MAIN_H definition
