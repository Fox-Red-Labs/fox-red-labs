/*
 * hal_3sh_foc_interface.c
 *
 */

/* Define MOTOR_THREE_SHUNT so hal_foc_interface.h includes hal_3sh_foc_interface.h
 * and brings the _inline bodies into scope. */
#ifndef MOTOR_THREE_SHUNT
#define MOTOR_THREE_SHUNT
#endif

#include "hal_foc_interface.h"
/* Syscfg includes */
#include "ti_sdk_dl_config.h"

/* FRL port (guard revision H5): two-sided comparator trips. The SDK used one comparator per phase, so with the sense's
 * sign on this board only current into the low side (motor-negative) tripped. Phases whose bit is set here also use the
 * low comparator, with its DAC mirrored about the calibrated offset (foc_drv8376.syscfg: inverted low comparator, its
 * CTRIPL in PWM crossbar 3). Phase V (index 1, CMPSS2) cannot: its low DAC drives PA4, the DRV8376 ILIMIT reference,
 * and a DAC that drives a pin cannot also be a comparator reference. The driver's own limit covers that side. */
#define FRL_CMP_TWO_SIDED_MASK 0x5U   /* index 0 (U, CMPSS0) and index 2 (W, CMPSS1) */

static inline void HAL_3S_ClearCompStatus(const HAL_3S_CMP_Config_t *cmpCfg)
{
    uint8_t index = 0;
    for (; index <= 2; index++)
    {
        if (cmpCfg->compType[index] == COMP_HIGH)
        {
            HAL_clear_CMPHigh_FaultStatus(cmpCfg->cmpssHandle[index]);
        }
        else
        {
            HAL_clear_CMPLow_FaultStatus(cmpCfg->cmpssHandle[index]);
        }
        if (FRL_CMP_TWO_SIDED_MASK & (1U << index))
        {
            HAL_clear_CMPLow_FaultStatus(cmpCfg->cmpssHandle[index]);
        }
    }
}

/* Comparator DAC offset calibration context — type defined in hal_obj.h */
static HAL_3S_CompCalCtx_t HAL_3S_CompCalCtx;

static inline void HAL_3S_setCompIlimConfig(float32_t currentLimit, const HAL_FOC_Config_t *configHandle,
                                            const HAL_FOC_Data_Obj *dataHandle)
{
    uint8_t index = 0U;

    for (; index <= 2U; index++)
    {
        int16_t cmpValue =
            (int16_t)dataHandle->compOffsetData[index] - (int16_t)(currentLimit / dataHandle->adcData.current_sf);

        if (cmpValue > 4095)
        {
            cmpValue = 4095;
        }
        else if (cmpValue <= 10)
        {
            cmpValue = 10;
        }
        else
        {
            /* value is within valid range — no adjustment needed */
        }

        if (configHandle->cmpCfg.compType[index] == COMP_HIGH)
        {
            HAL_Set_PhHighCompVal(configHandle->cmpCfg.cmpssHandle[index], (uint16_t)cmpValue);
        }
        else
        {
            HAL_Set_PhLowCompVal(configHandle->cmpCfg.cmpssHandle[index], (uint16_t)cmpValue);
        }
        if (FRL_CMP_TWO_SIDED_MASK & (1U << index))   /* FRL port: the mirror threshold on the low comparator */
        {
            int16_t lowValue =
                (int16_t)dataHandle->compOffsetData[index] + (int16_t)(currentLimit / dataHandle->adcData.current_sf);
            if (lowValue < 10) lowValue = 10;
            if (lowValue > 4095) lowValue = 4095;
            HAL_Set_PhLowCompVal(configHandle->cmpCfg.cmpssHandle[index], (uint16_t)lowValue);
        }
    }
}

/* Thin vtable wrappers — delegate to _inline bodies in hal_3sh_foc_interface.h */

void HAL_3S_FOC_SetPhaseVoltages(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_3S_FOC_SetPhaseVoltages_inline(configHandle, dataHandle);
}

void HAL_3S_FOC_ReadPhaseCurrents(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_3S_FOC_ReadPhaseCurrents_inline(configHandle, dataHandle);
}

void HAL_3S_FOC_ReadInverterVoltages(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_3S_FOC_ReadInverterVoltages_inline(configHandle, dataHandle);
}

void HAL_3S_FOC_SetCompOCLimits(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_3S_setCompIlimConfig(*dataHandle->pOverCurLimA, configHandle, dataHandle);

    return;
}

void HAL_3S_FOC_Init(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
}

/*
 * One-time offset calibration setup.
 *
 * \param[in]    configHandle  FOC configuration handle
 * \param[inout] dataHandle    FOC data handle
 */
void HAL_3S_FOC_RunOffsetCalInit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    /* Zero phase current offsets so ReadPhaseCurrents gives I_A = raw * current_sf */
    dataHandle->adcData.offset_I_ad.value[0] = 0.0f;
    dataHandle->adcData.offset_I_ad.value[1] = 0.0f;
    dataHandle->adcData.offset_I_ad.value[2] = 0.0f;

    /* Seed phase voltage scale-factor offsets from gate driver */
    dataHandle->adcData.offset_V_sf.value[0] = dataHandle->gdDataHandle->gdVolSF;
    dataHandle->adcData.offset_V_sf.value[1] = dataHandle->gdDataHandle->gdVolSF;
    dataHandle->adcData.offset_V_sf.value[2] = dataHandle->gdDataHandle->gdVolSF;

    /* Set PWM outputs to 0% duty cycle */
    dataHandle->pwmData.Vabc_pu.value[0] = 0.0f;
    dataHandle->pwmData.Vabc_pu.value[1] = 0.0f;
    dataHandle->pwmData.Vabc_pu.value[2] = 0.0f;

    HAL_writePWMData(&configHandle->pwmCfg, &dataHandle->pwmData);

    HAL_3S_ClearCompStatus(&configHandle->cmpCfg);

#if FRL_COMMISSIONING
    /* Sampling still runs from the PWM timebase; gate inputs stay tripped low. */
    HAL_disablePWM(&configHandle->pwmCfg, &dataHandle->pwmData);
#else
    HAL_enablePWM(&configHandle->pwmCfg, &dataHandle->pwmData);
#endif
}

/*
 * Per-ISR-tick offset calibration step.
 *
 * \param[in]    configHandle   FOC configuration handle
 * \param[inout] dataHandle     FOC data handle
 * \param[in]    calibTickCnt   Current tick counter (0-based)
 * \param[in]    calibTotalCnt  Total calibration ticks
 * \return 1 when calibration is complete, 0 while running
 */
uint8_t HAL_3S_FOC_RunOffsetCalStep(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle,
                                    uint32_t calibTickCnt, uint32_t calibTotalCnt)
{
    static const float32_t offsetK1 = 0.998001f;
    static const float32_t offsetK2 = 0.001999f;

    static float32_t iirAccI0 = 0.0f;
    static float32_t iirAccI1 = 0.0f;
    static float32_t iirAccI2 = 0.0f;

    uint32_t warmupTicks = calibTotalCnt / 10U;
    uint8_t  retVal      = 0U;

    if (calibTickCnt == 0U)
    {
        iirAccI0 = 0.0f;
        iirAccI1 = 0.0f;
        iirAccI2 = 0.0f;
    }
    else
    {
        /* Nothing to do - accumulators already reset */
    }

    if (calibTickCnt >= warmupTicks)
    {
        float32_t invVdcbus;

        iirAccI0 = offsetK1 * iirAccI0 + offsetK2 * dataHandle->adcData.I_A.value[0];
        iirAccI1 = offsetK1 * iirAccI1 + offsetK2 * dataHandle->adcData.I_A.value[1];
        iirAccI2 = offsetK1 * iirAccI2 + offsetK2 * dataHandle->adcData.I_A.value[2];

        invVdcbus = 1.0f / dataHandle->adcData.VdcBus_V;

        dataHandle->adcData.offset_V_sf.value[0] = offsetK1 * dataHandle->adcData.offset_V_sf.value[0] +
                                                   offsetK2 * (invVdcbus * dataHandle->adcData.V_V.value[0]);

        dataHandle->adcData.offset_V_sf.value[1] = offsetK1 * dataHandle->adcData.offset_V_sf.value[1] +
                                                   offsetK2 * (invVdcbus * dataHandle->adcData.V_V.value[1]);

        dataHandle->adcData.offset_V_sf.value[2] = offsetK1 * dataHandle->adcData.offset_V_sf.value[2] +
                                                   offsetK2 * (invVdcbus * dataHandle->adcData.V_V.value[2]);
    }
    else
    {
        (void)configHandle;
    }

    if (calibTickCnt >= (calibTotalCnt - 1U))
    {
        float32_t invCurrentSf = 1.0f / dataHandle->adcData.current_sf;

        dataHandle->adcData.offset_I_ad.value[0] = iirAccI0 * invCurrentSf;
        dataHandle->adcData.offset_I_ad.value[1] = iirAccI1 * invCurrentSf;
        dataHandle->adcData.offset_I_ad.value[2] = iirAccI2 * invCurrentSf;

        HAL_disablePWM(&configHandle->pwmCfg, &dataHandle->pwmData);

        retVal = 1U;
    }
    else
    {
        /* Calibration still in progress */
    }

    return retVal;
}

/*
 * \brief      Configure PWM period and compare units
 * \param[in]  configHandle  FOC configuration handle
 * \param[in]  dataHandle    FOC data handle
 */
void HAL_3S_FOC_ConfigPWM(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_setupPWMs(&configHandle->pwmCfg);

    HAL_SetupThreeShuntPWMConfig(&configHandle->pwmCfg);

    HAL_StartIPDPulseCount(&configHandle->ecapIPDConfig);
}

void HAL_3S_FOC_clearMtrFaultStatus(const HAL_FOC_Config_t *configHandle)
{
    HAL_clearPWMFaultStatus(&configHandle->pwmCfg);
    HAL_3S_ClearCompStatus(&configHandle->cmpCfg);
}

void HAL_3S_IPDExit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_StopIPDPulseCount(&configHandle->ecapIPDConfig);
    HAL_ResetPWM(&configHandle->pwmCfg, &dataHandle->pwmData);
    HAL_clearPWMFaultStatus(&configHandle->pwmCfg);
    HAL_enablePWM(&configHandle->pwmCfg, &dataHandle->pwmData);
    HAL_3S_setCompIlimConfig(*dataHandle->pOverCurLimA, configHandle, dataHandle);
}

void HAL_3S_IPDPulseConfig(VOLTAGE_VECTOR voltageVector, const HAL_FOC_Config_t *configHandle,
                           HAL_FOC_Data_Obj *dataHandle)
{
    HAL_ResetIPDPulseCount(&configHandle->ecapIPDConfig);
    HAL_clearPWMFaultStatus(&configHandle->pwmCfg);
    HAL_forceVoltageSector(voltageVector, &configHandle->pwmCfg);
    HAL_StartIPDPulseCount(&configHandle->ecapIPDConfig);
}

void HAL_3S_IPDInit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    HAL_IPDForcePWMLow(&configHandle->pwmCfg);

    HAL_3S_ClearCompStatus(&configHandle->cmpCfg);

    HAL_clearPWMFaultStatus(&configHandle->pwmCfg);

    HAL_3S_setCompIlimConfig(dataHandle->ipdCurrentLimit, configHandle, dataHandle);

    HAL_StopIPDPulseCount(&configHandle->ecapIPDConfig);
}

uint8_t HAL_3S_IPDPulseDone(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    if (HAL_getMtrTripFaults(&configHandle->pwmCfg))
    {
        HAL_IPDForcePWMLow(&configHandle->pwmCfg);
        dataHandle->ecapIPDPulseTime = HAL_getIPDPulseTime(&configHandle->ecapIPDConfig);
        HAL_3S_ClearCompStatus(&configHandle->cmpCfg);
        HAL_StopIPDPulseCount(&configHandle->ecapIPDConfig);

        return 1;
    }
    else
    {
        return 0;
    }
}

void HAL_3S_CompOffsetCalInit(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    uint8_t  index      = 0U;
    uint16_t initStatus = 0U;

    for (; index <= 2U; index++)
    {
        /* Set DAC to sweep start value */
        if (configHandle->cmpCfg.compType[index] == COMP_HIGH)
        {
            DL_CMPSSLITE_setDACValueHigh((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index],
                                         HAL_COMP_DAC_SWEEP_LOW_COUNTS);
        }
        else
        {
            DL_CMPSSLITE_setDACValueLow((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index],
                                        HAL_COMP_DAC_SWEEP_LOW_COUNTS);
        }

        /* Clear filter latch so status reflects the new DAC value */
        if (configHandle->cmpCfg.compType[index] == COMP_HIGH)
        {
            DL_CMPSSLITE_clearFilterLatchHigh((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index]);
        }
        else
        {
            DL_CMPSSLITE_clearFilterLatchLow((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index]);
        }

        /* Wait for comparator to settle at the start DAC value */
        DL_Common_delayCycles(HAL_COMP_DAC_SETTLE_CYCLES);

        /* Capture initial filter-output status */
        initStatus = DL_CMPSSLITE_getStatus((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index]);

        if (configHandle->cmpCfg.compType[index] == COMP_HIGH)
        {
            HAL_3S_CompCalCtx.InitStatus[index] = (uint16_t)(initStatus & (uint16_t)DL_CMPSSLITE_STS_HI_FILOUT);
        }
        else
        {
            HAL_3S_CompCalCtx.InitStatus[index] = (uint16_t)(initStatus & (uint16_t)DL_CMPSSLITE_STS_LO_FILOUT);
        }

        HAL_3S_CompCalCtx.DacVal[index] = HAL_COMP_DAC_SWEEP_LOW_COUNTS;
        HAL_3S_CompCalCtx.Lo[index]     = HAL_COMP_DAC_SWEEP_LOW_COUNTS;
        HAL_3S_CompCalCtx.Hi[index]     = HAL_COMP_DAC_SWEEP_HIGH_COUNTS;
        HAL_3S_CompCalCtx.Done[index]   = 0U;

        /* Seed output with midpoint fallback; overwritten when binary search converges */
        dataHandle->compOffsetData[index] = HAL_COMP_DAC_SWEEP_MID_COUNTS;
    }
}

uint8_t HAL_3S_CompOffsetCalStep(const HAL_FOC_Config_t *configHandle, HAL_FOC_Data_Obj *dataHandle)
{
    uint8_t  index        = 0U;
    uint8_t  allDone      = 1U;
    uint16_t curStatus    = 0U;
    uint16_t maskedStatus = 0U;
    uint8_t  retVal       = 0U;

    for (; index <= 2U; index++)
    {
        if (HAL_3S_CompCalCtx.Done[index] != 0U)
        {
            /* Channel already resolved — skip */
        }
        else if (HAL_3S_CompCalCtx.Lo[index] >= HAL_3S_CompCalCtx.Hi[index])
        {
            /* Binary search converged — record result */
            dataHandle->compOffsetData[index] = HAL_3S_CompCalCtx.Lo[index];
            HAL_3S_CompCalCtx.Done[index]     = 1U;
        }
        else
        {
            /* Compute midpoint and write to DAC */
            HAL_3S_CompCalCtx.DacVal[index] =
                (uint16_t)((HAL_3S_CompCalCtx.Lo[index] + HAL_3S_CompCalCtx.Hi[index]) / 2U);

            if (configHandle->cmpCfg.compType[index] == COMP_HIGH)
            {
                DL_CMPSSLITE_setDACValueHigh((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index],
                                             HAL_3S_CompCalCtx.DacVal[index]);
            }
            else
            {
                DL_CMPSSLITE_setDACValueLow((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index],
                                            HAL_3S_CompCalCtx.DacVal[index]);
            }

            /* Wait 2 ms for comparator to settle at the new DAC value */
            DL_Common_delayCycles(HAL_COMP_DAC_SETTLE_CYCLES);

            curStatus = DL_CMPSSLITE_getStatus((CMPSS_LITE_Regs *)configHandle->cmpCfg.cmpssHandle[index]);

            if (configHandle->cmpCfg.compType[index] == COMP_HIGH)
            {
                maskedStatus = (uint16_t)(curStatus & (uint16_t)DL_CMPSSLITE_STS_HI_FILOUT);
            }
            else
            {
                maskedStatus = (uint16_t)(curStatus & (uint16_t)DL_CMPSSLITE_STS_LO_FILOUT);
            }

            if (maskedStatus == HAL_3S_CompCalCtx.InitStatus[index])
            {
                /* Trip point is above mid — search upper half */
                HAL_3S_CompCalCtx.Lo[index] = HAL_3S_CompCalCtx.DacVal[index] + 1U;
            }
            else
            {
                /* Trip point is at or below mid — search lower half */
                HAL_3S_CompCalCtx.Hi[index] = HAL_3S_CompCalCtx.DacVal[index];
            }

            allDone = 0U;
        }
    }

    if (allDone != 0U)
    {
        /* Calibration complete — reprogram OC limits using calibrated offsets,
         * then clear any comparator faults that tripped during the sweep. */
        HAL_3S_setCompIlimConfig(*dataHandle->pOverCurLimA, configHandle, dataHandle);
        HAL_3S_ClearCompStatus(&configHandle->cmpCfg);
        HAL_3S_FOC_clearMtrFaultStatus(configHandle);
        retVal = 1U;
    }
    else
    {
        retVal = 0U;
    }

    return retVal;
}

const HAL_FOC_vTable_t hal3SFOCFnTable = {
    .Init                 = HAL_3S_FOC_Init,
    .SetPhaseVoltages     = HAL_3S_FOC_SetPhaseVoltages,
    .ReadPhaseCurrents    = HAL_3S_FOC_ReadPhaseCurrents,
    .ReadInverterVoltages = HAL_3S_FOC_ReadInverterVoltages,
    .SetCompOCLimits      = HAL_3S_FOC_SetCompOCLimits,
    .RunOffsetCalInit     = HAL_3S_FOC_RunOffsetCalInit,
    .RunOffsetCalStep     = HAL_3S_FOC_RunOffsetCalStep,
    .ConfigPWM            = HAL_3S_FOC_ConfigPWM,
    .clearMtrFaultStatus  = HAL_3S_FOC_clearMtrFaultStatus,
    .ipdInit              = HAL_3S_IPDInit,
    .ipdExit              = HAL_3S_IPDExit,
    .ipdPulseConfig       = HAL_3S_IPDPulseConfig,
    .ipdPulseDone         = HAL_3S_IPDPulseDone,
    .compOffsetCalInit    = HAL_3S_CompOffsetCalInit,
    .compOffsetCalStep    = HAL_3S_CompOffsetCalStep,
};
