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

/*
 * New file, not part of TI's SDK. Enable order from drv8329a_interface.c (nSLEEP high, then DRVOFF low),
 * data object in the 3-shunt layout of drv8300drge_interface.c / drv8323rh_interface.c.
 *
 * DRV8376 power-up (data sheet SLVSHD4A): nSLEEP high wakes the device; tWAKE is 5.5 ms maximum before
 * the outputs may be driven, so the enable waits 6 ms between nSLEEP and DRVOFF. DRVOFF high keeps all
 * six outputs Hi-Z, so it stays high until the device is awake.
 */
#ifdef DRV8376EVM_REVA
#include "drv8376_interface.h"
#include "hal_gate_driver.h"

void HAL_DRV8376_Init(void const *handle)
{
}  // nothing to configure: the EVM's mode, gain and slew are fixed by resistors, no SPI

void HAL_enableDRV8376(void const *handle)
{
    HAL_DRV8376_Obj *obj = (HAL_DRV8376_Obj *)handle;

    // Keep the outputs off while the device wakes
    HAL_setGPIOHigh(obj->gateDriverOffGPIO, obj->gateDriverOffGPIONum);

    // Wake: nSLEEP high
    HAL_setGPIOHigh(obj->gateNSleepGPIO, obj->gateNSleepGPIONum);

    // tWAKE max 5.5 ms
    DEVICE_DELAY_MS(DRV8376_WAKE_WAIT_ms);

    // Commissioning runs TI calibration with the power stage disabled.
#if !FRL_COMMISSIONING
    HAL_setGPIOLow(obj->gateDriverOffGPIO, obj->gateDriverOffGPIONum);
#endif

    return;
}  // HAL_enableDRV8376() function

const HAL_GD_vTable_t halDRV8376FnTable = {
    .initGateDriver   = HAL_DRV8376_Init,
    .enableGateDriver = HAL_enableDRV8376,
};
const HAL_GD_DATA_Obj drv8376Data_M1 = {.gdCurrOffset           = DRV8376_IDC_OFFSET_AD,
                                        .gdCurrSampDelay        = 0,
                                        .gdCurrSensDelay        = 0,
                                        .gdCurrentSF            = DRV8376_CURRENT_SF,
                                        .gdMaxCurrentA          = DRV8376_ADC_FULL_SCALE_CURRENT_A,
                                        .gdVolSF                = DRV8376_VOLTAGE_SF,
                                        .gdPhVoltOffsetSF       = DRV8376_VA_OFFSET_SF,
                                        .gdVolFilterPoleFreqRPS = DRV8376_VOLTAGE_FILTER_POLE_rps,
                                        .gdOverVolVal           = DRV8376_OVER_VOLTAGE_FAULT_V,
                                        .gdUnderVolVal          = DRV8376_UNDER_VOLTAGE_FAULT_V,
                                        .gdCurrSign             = DRV8376_SIGN_CURRENT_SF};
#endif
