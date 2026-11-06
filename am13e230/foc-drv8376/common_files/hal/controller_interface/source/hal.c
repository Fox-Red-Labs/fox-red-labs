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

//
// the includes
//

// platforms
#include "hal.h"

// **************************************************************************
// the defines

// **************************************************************************
// the globals
HAL_Handle halHandle;  //!< the handle for the hardware abstraction layer
HAL_Obj    hal;

// **************************************************************************
// the functions

HAL_Handle HAL_init(void *pMemory, const size_t numBytes)
{
    HAL_Handle handle;

    if (numBytes < sizeof(HAL_Obj))
    {
        return ((HAL_Handle)NULL);
    }

    // assign the handle
    handle = (HAL_Handle)pMemory;

    return (handle);
}
void HAL_enableCtrlInts(const HAL_INT_Config_t *handle)
{
    // // enable the ADC interrupts
    NVIC_SetPriority(handle->intNVICNum, 0);

    NVIC_EnableIRQ(handle->intNVICNum);

    // return;
}  // end of HAL_enableCtrlInts() function

void HAL_disableGlobalInts()
{
    // enable global interrupts
    __disable_irq();

}  // end of HAL_enableGlobalInts() function

void HAL_enableGlobalInts()
{
    // enable global interrupts
    __enable_irq();

}  // end of HAL_enableGlobalInts() function

void HAL_setParams(HAL_Handle handle)
{
    //     // // disable global interrupts
    /* Power-up adc converter core */
    DL_ADC_powerUp(ADC0);
    DL_ADC_powerUp(ADC1);
    DL_ADC_powerUp(ADC2);

    DL_Common_delayCycles(200U * 5000U); /* Internal Reference Mode */

    DL_MCPWM_enableTBCLK();

    // #ifdef _FLASH
    //     //
    //     // Copy time critical code and flash setup code to RAM. This includes the
    //     // following functions: InitFlash();
    //     //
    //     // The RamfuncsLoadStart, RamfuncsLoadSize, and RamfuncsRunStart symbols
    //     // are created by the linker. Refer to the device .cmd file.
    //     //
    //     memcpy(&runStart_ctrlfuncs, &loadStart_ctrlfuncs, (size_t)&loadSize_ctrlfuncs);
    // #endif  // _FLASH

    //     // Sets up the timers for CPU usage diagnostics
    //     HAL_setupCPUUsageTimer(handle);

    //     return;
}  // end of HAL_setParams() function

void HAL_SetupThreeShuntPWMConfig(const HAL_PWM_Config_t *handle)
{
    DL_MCPWM_setADCTriggerEventPrescale((MCPWM_Regs *)handle->pwmHandle, (DL_MCPWM_SOC_TYPE)handle->adcTrigPWMSource,
                                        handle->numPWMTicksPerISRTick);

    *handle->pwmDownCountTriggerSource = handle->gateDriverDelay;
}
void HAL_SetupSingleShuntPWMConfig(const HAL_PWM_Config_t *handle)
{
    // /* Configure the Trigger Rate for the ADC , Single Shunt sampling based on the PWM Frequency */

    /* Stop PWM counter before reconfiguration */
    DL_MCPWM_setTimeBaseCounterMode((MCPWM_Regs *)handle->pwmTrigHandle, DL_MCPWM_COUNTER_MODE_STOP_FREEZE);

    // create the PWM trigger sources for all the four SOC events
    DL_MCPWM_setADCTriggerEventPrescale((MCPWM_Regs *)handle->pwmTrigHandle,
                                        (DL_MCPWM_SOC_TYPE)handle->adcTrigPWMUpCountASource,
                                        handle->numPWMTicksPerISRTick);

    DL_MCPWM_setADCTriggerEventPrescale((MCPWM_Regs *)handle->pwmTrigHandle,
                                        (DL_MCPWM_SOC_TYPE)handle->adcTrigPWMUpCountBSource,
                                        handle->numPWMTicksPerISRTick);

    DL_MCPWM_setADCTriggerEventPrescale((MCPWM_Regs *)handle->pwmTrigHandle,
                                        (DL_MCPWM_SOC_TYPE)handle->adcTrigPWMDownCountASource,
                                        handle->numPWMTicksPerISRTick);

    DL_MCPWM_setADCTriggerEventPrescale((MCPWM_Regs *)handle->pwmTrigHandle,
                                        (DL_MCPWM_SOC_TYPE)handle->adcTrigPWMDownCountBSource,
                                        handle->numPWMTicksPerISRTick);

    // /* Set the Trigger Points to the Center of PWM's */

    *handle->pwmUpCountTriggerSourceA   = ((uint32_t)(handle->pwmPeriod >> 1) & 0xFFFF);
    *handle->pwmUpCountTriggerSourceB   = ((uint32_t)(handle->pwmPeriod >> 1) & 0xFFFF);
    *handle->pwmDownCountTriggerSourceA = ((uint32_t)(handle->pwmPeriod >> 1) & 0xFFFF);
    *handle->pwmDownCountTriggerSourceB = ((uint32_t)(handle->pwmPeriod >> 1) & 0xFFFF);

    // Configure time base period parameters
    DL_MCPWM_setTimeBasePeriodActive((MCPWM_Regs *)handle->pwmTrigHandle, handle->pwmPeriod);
    DL_MCPWM_setTimeBasePeriodShadow((MCPWM_Regs *)handle->pwmTrigHandle, handle->pwmPeriod);

    DL_MCPWM_setTimeBaseCounterMode((MCPWM_Regs *)handle->pwmTrigHandle, DL_MCPWM_COUNTER_MODE_UP_DOWN);
}

void HAL_setupPWMs(const HAL_PWM_Config_t *handle)
{
    DL_XBAR_setInputXBAR((DL_XBAR_InputNum)handle->MotorStoptripInputXBARSource, DL_XBAR_DRIVE_LOW);

    /* Re-configure PWM */
    /* Stop PWM counter before reconfiguration */
    DL_MCPWM_setTimeBaseCounterMode((MCPWM_Regs *)handle->pwmHandle, DL_MCPWM_COUNTER_MODE_STOP_FREEZE);

    // Configure time base period parameters
    DL_MCPWM_setTimeBasePeriodActive((MCPWM_Regs *)handle->pwmHandle, handle->pwmPeriod);
    DL_MCPWM_setTimeBasePeriodShadow((MCPWM_Regs *)handle->pwmHandle, handle->pwmPeriod);

    /* Run PWM counter after reconfiguration */
    DL_MCPWM_setTimeBaseCounterMode((MCPWM_Regs *)handle->pwmHandle, DL_MCPWM_COUNTER_MODE_UP_DOWN);

    DL_XBAR_setInputXBAR((DL_XBAR_InputNum)handle->MotorStoptripInputXBARSource, DL_XBAR_DRIVE_HIGH);
    return;
}

void HAL_setupCPUUsageTimer(HAL_Handle handle)
{
    //     HAL_Obj  *obj = (HAL_Obj *)handle;

    // use timer 2 for CPU usage diagnostics
    // CPUTimer_setPreScaler(obj->timerHandle[2], 0);

    // CPUTimer_setEmulationMode(obj->timerHandle[2],
    //                          CPUTIMER_EMULATIONMODE_RUNFREE);

    //     CPUTimer_setPeriod(obj->timerHandle[2], 0xFFFFFFFF);
    //
    //     CPUTimer_startTimer(obj->timerHandle[2]);

    return;
}  // end of HAL_setupCPUUsageTimer() function

void HAL_clearDataRAM(void *pMemory, uint32_t lengthMemory)
{
    uint32_t *pMemoryStart;
    uint32_t  loopCount, loopLength;

    pMemoryStart = pMemory;
    loopLength   = lengthMemory;

    for (loopCount = 0U; loopCount < loopLength; loopCount++)
    {
        *(pMemoryStart + loopCount) = 0x0000;
    }
}  // end of HAL_clearDataRAM() function

void HAL_setTriggerParams(HAL_PWMData_t *pPWMData, const float32_t systemFreq_MHz, const float32_t deadband_us,
                          const float32_t noiseWindow_us, const float32_t adcSample_us)
{
    uint16_t deadband    = (uint16_t)(deadband_us * systemFreq_MHz);
    uint16_t noiseWindow = (uint16_t)(noiseWindow_us * systemFreq_MHz);
    uint16_t adcSample   = (uint16_t)(adcSample_us * systemFreq_MHz);

    // TODO: Only applicable for the 3 Shunt , in Single shunt, the min sample is taken care by AV duration and sample
    // delay.
    pPWMData->deadband    = deadband;
    pPWMData->noiseWindow = noiseWindow;
    pPWMData->adcSample   = adcSample;

    /* Sampled at Center of PWM*/
    pPWMData->minCMPValue = (deadband + noiseWindow + adcSample);

    return;
}  // end of HAL_setTriggerParams() function
