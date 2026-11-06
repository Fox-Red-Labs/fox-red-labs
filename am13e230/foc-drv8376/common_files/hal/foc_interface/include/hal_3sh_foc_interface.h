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
 * \file hal_3sh_foc_interface.h
 * \brief Three-shunt FOC HAL — ISR hot-path inline bodies and non-ISR declarations.
 *
 * \defgroup HAL_3SH_FOC_INTERFACE Three-Shunt FOC HAL Interface
 * \brief Three-shunt current sensing topology
 * @{
 */

#ifndef HAL_3SH_FOC_INTERFACE_H_
#define HAL_3SH_FOC_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Included by hal_foc_interface.h after HAL_FOC_Config_t / HAL_FOC_Data_Obj are defined. */

/**
 * \brief Write PWM compare registers (three-shunt)
 * \param[in]    configHandle  FOC configuration handle
 * \param[inout] dataHandle    FOC data handle
 */
__attribute__((always_inline)) static inline void HAL_3S_FOC_SetPhaseVoltages_inline(
    const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_writePWMData(&configHandle->pwmCfg, &dataHandle->pwmData);
}

/**
 * \brief Read three-phase currents from ADC, apply offset and scale
 * \param[in]    configHandle  FOC configuration handle
 * \param[inout] dataHandle    FOC data handle
 */
__attribute__((always_inline)) static inline void HAL_3S_FOC_ReadPhaseCurrents_inline(
    const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    float32_t value;

    value = HAL_ControllerReadMtrCurrentAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcCurrentCfg[0]);
    dataHandle->adcData.I_A.value[0] =
        (value - dataHandle->adcData.offset_I_ad.value[0]) * dataHandle->adcData.current_sf;

    value = HAL_ControllerReadMtrCurrentAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcCurrentCfg[1]);
    dataHandle->adcData.I_A.value[1] =
        (value - dataHandle->adcData.offset_I_ad.value[1]) * dataHandle->adcData.current_sf;

    value = HAL_ControllerReadMtrCurrentAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcCurrentCfg[2]);
    dataHandle->adcData.I_A.value[2] =
        (value - dataHandle->adcData.offset_I_ad.value[2]) * dataHandle->adcData.current_sf;
}

/**
 * \brief Read three-phase and DC-bus voltages from ADC, apply scale
 * \param[in]    configHandle  FOC configuration handle
 * \param[inout] dataHandle    FOC data handle
 */
__attribute__((always_inline)) static inline void HAL_3S_FOC_ReadInverterVoltages_inline(
    const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    float32_t value;

    value = HAL_ControllerReadMtrVoltageAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcVoltageCfg[0]);
    dataHandle->adcData.V_V.value[0] = value * dataHandle->adcData.voltage_sf;

    value = HAL_ControllerReadMtrVoltageAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcVoltageCfg[1]);
    dataHandle->adcData.V_V.value[1] = value * dataHandle->adcData.voltage_sf;

    value = HAL_ControllerReadMtrVoltageAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcVoltageCfg[2]);
    dataHandle->adcData.V_V.value[2] = value * dataHandle->adcData.voltage_sf;

    value = HAL_ControllerReadMtrVoltageAdcData((HAL_ADC_Result_Config_Handle)&configHandle->adcVoltageCfg[3]);
    dataHandle->adcData.VdcBus_V = value * dataHandle->adcData.voltage_sf;
}

/* Non-ISR function declarations — bodies in hal_3sh_foc_interface.c */

extern void    HAL_3S_FOC_SetCompOCLimits(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
extern void    HAL_3S_FOC_Init(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
extern void    HAL_3S_FOC_RunOffsetCalInit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
extern uint8_t HAL_3S_FOC_RunOffsetCalStep(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle,
                                           uint32_t calibTickCnt, uint32_t calibTotalCnt);
extern void    HAL_3S_FOC_ConfigPWM(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
extern void    HAL_3S_FOC_clearMtrFaultStatus(const HAL_FOC_Config_t *configHandle);
extern void    HAL_3S_IPDInit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
extern void    HAL_3S_IPDPulseConfig(VOLTAGE_VECTOR voltageVector, const HAL_FOC_Config_t *configHandle,
                                     HAL_FOC_Data_Obj *dataHandle);
extern uint8_t HAL_3S_IPDPulseDone(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);
extern void    HAL_3S_IPDExit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);

/**
 * \brief One-time comparator DAC offset calibration setup (three-shunt).
 *
 * Sets each of the three phase comparator DACs to the sweep start value,
 * clears the filter latches, and records the initial comparator status.
 * Must be called once before the first call to HAL_3S_CompOffsetCalStep().
 *
 * \param[in]    configHandle  FOC configuration handle
 * \param[inout] dataHandle    FOC data handle
 */
extern void HAL_3S_CompOffsetCalInit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);

/**
 * \brief Per-call comparator DAC sweep step (three-shunt).
 *
 * Waits 2 ms, reads the comparator filter output status for each unresolved
 * channel, and increments the DAC by one count until a trip-point transition
 * is detected.  The found DAC value is stored in
 * dataHandle->compOffsetData[index] for each phase.
 *
 * \param[in]    configHandle  FOC configuration handle
 * \param[inout] dataHandle    FOC data handle
 *
 * \return 1 when all three channels are resolved, 0 while still sweeping.
 */
extern uint8_t HAL_3S_CompOffsetCalStep(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle);

/* Vtable instance — defined in hal_3sh_foc_interface.c */
extern const HAL_FOC_vTable_t hal3SFOCFnTable;

#ifdef __cplusplus
}
#endif

#endif /* HAL_3SH_FOC_INTERFACE_H_ */

/** @} */
