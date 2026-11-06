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
 * \file hal_debug_interface.h
 * \brief HAL Debug Interface for serial plotting and DAC output
 *
 * \defgroup HAL_DEBUG_INTERFACE_MODULE HAL Debug Interface Module
 * \brief Debug communication interface for diagnostic output and DAC control
 * @{
 */

#ifndef HAL_DEBUG_INTERFACE_H
#define HAL_DEBUG_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "math_types.h"
#include "Platform_Types.h"
#include "ti_sdk_dl_config.h"
#include <string.h>

/**
 * \anchor DAC_CONTROL
 * \name DAC Control Structure
 * \brief DAC control register structure for real-time debug output
 * @{
 */
typedef struct
{
    uint16_t   dacEn;             //!< DAC enable flag
    int16_t    mode;              //!< Unipolar output left/right shift (used if dacScalingFactor is 0)
    float32_t  dacScalingFactor;  //!< DAC scale factor in IQ27 format
    float32_t *pDacOutAddr;       //!< Pointer to variable address for DAC output
    uint32_t   dacData;           //!< DAC output data value
} RAM_DAC_CNTRL_T;
//! @}

/**
 * \anchor HAL_DEBUG_CONFIG
 * \name HAL Debug Configuration Structure
 * @{
 */

/**
 * \brief HAL Debug Configuration structure
 *
 * Minimal configuration structure containing only the pointers and data
 * that HAL debug functions need. This decouples HAL from application-level
 * structures like SYSTEM_Vars_t, maintaining HAL layer independence.
 *
 * The application layer populates this structure from SYSTEM_Vars_t and
 * passes it to HAL functions.
 */
typedef struct
{
    /**
     * \brief Pointer to DAC peripheral instance
     *
     * Used for DAC output register access
     */
    uint32_t dacInst;

    /**
     * \brief UART peripheral instance for serial plot transmission
     *
     * Cast of UART_Regs* — set from systemVars.commUartInst in initDACOutputs().
     * Used by HAL_updateSerialPlot() to transmit debug data to the host.
     */
    uint32_t uartInst;

    /**
     * \brief DAC Channel 1 control structure
     *
     * Contains enable flag, mode, scaling factor, output address, and data
     */
    volatile RAM_DAC_CNTRL_T *pDacCtrl1;

    /**
     * \brief DAC Channel 2 control structure
     *
     * Contains enable flag, mode, scaling factor, output address, and data
     */
    volatile RAM_DAC_CNTRL_T *pDacCtrl2;

    /**
     * \brief Pointer to serial plot buffer
     *
     * Buffer for serial plot data transmission via UART
     */
    uint8_t *pSerialPlotBuffer;

    /**
     * \brief Pointer to debug data variable 1
     *
     * First debug data value for serial plot transmission
     */
    uint32_t *pData1;

    /**
     * \brief Pointer to debug data variable 2
     *
     * Second debug data value for serial plot transmission
     */
    uint32_t *pData2;

    /**
     * \brief Pointer to index counter
     *
     * General purpose index for buffer management
     */
    uint16_t *pIdx;

} HAL_DebugConfig_t;

/** @} */

/* Writes bytes to UART in non-blocking mode, returns when FIFO is full */
static inline void SCI_writeBytesNonBlocking(DL_UNICOMM_Inst_Regs *base, uint8_t *data, uint16_t *idx, uint16_t len)
{
    while (*idx < len)
    {
        if (!DL_UART_isTXFIFOFull(base))
        {
            /* Only lower 8 bits are sent */
            DL_UART_transmitData(base, data[*idx]);
            (*idx)++;
        }
        else
        {
            break; /* no room; finish next ISR tick */
        }
    }
}

/**
 * \brief Update serial plot data for UART transmission
 *
 * Handles serial plot UART updates for diagnostic output. Copies DAC control
 * data into serial plot buffer and transmits via SCI interface.
 *
 * NOTE: This should ideally be moved to background loop for better ISR performance
 *
 * \param[inout] config Pointer to HAL_DebugConfig_t structure containing debug configuration
 */
static inline void HAL_updateSerialPlot(HAL_DebugConfig_t *config)
{
    static uint8_t plotCount = 1U;

    if (plotCount >= 1U)
    {
        memcpy((void *)config->pData1, (void *)config->pDacCtrl1->pDacOutAddr, sizeof(float));
        memcpy((void *)config->pData2, (void *)config->pDacCtrl2->pDacOutAddr, sizeof(float));

        if (*config->pIdx >= 10U)
        {
            config->pSerialPlotBuffer[0U] = 0xAAU;
            config->pSerialPlotBuffer[1U] = 0x55U;
            config->pSerialPlotBuffer[2U] = (uint8_t)(*config->pData1 & 0xFFU);
            config->pSerialPlotBuffer[3U] = (uint8_t)((*config->pData1 >> 8U) & 0xFFU);
            config->pSerialPlotBuffer[4U] = (uint8_t)((*config->pData1 >> 16U) & 0xFFU);
            config->pSerialPlotBuffer[5U] = (uint8_t)((*config->pData1 >> 24U) & 0xFFU);
            config->pSerialPlotBuffer[6U] = (uint8_t)(*config->pData2 & 0xFFU);
            config->pSerialPlotBuffer[7U] = (uint8_t)((*config->pData2 >> 8U) & 0xFFU);
            config->pSerialPlotBuffer[8U] = (uint8_t)((*config->pData2 >> 16U) & 0xFFU);
            config->pSerialPlotBuffer[9U] = (uint8_t)((*config->pData2 >> 24U) & 0xFFU);
            *config->pIdx                 = 0U;
        }

        SCI_writeBytesNonBlocking((DL_UNICOMM_Inst_Regs *)config->uartInst, config->pSerialPlotBuffer, config->pIdx,
                                  10U);
        plotCount = 0U;
    }
    else
    {
        plotCount++;
    }
}

/**
 * \brief Update DAC outputs for oscilloscope monitoring
 *
 * Handles DAC output updates for real-time oscilloscope monitoring of motor
 * state variables. Supports both bipolar and unipolar DAC modes with scaling.
 *
 * NOTE: This should ideally be moved to background loop for better ISR performance
 *
 * \param[inout] config Pointer to HAL_DebugConfig_t structure containing debug configuration
 */
static inline void HAL_updateDACOutputs(HAL_DebugConfig_t *config)
{
    /* Local variables to ensure defined order of volatile accesses */
    float32_t dacOutVal;
    float32_t scalingFactor;

    /* DAC Channel 1 */
    if (config->pDacCtrl1->dacEn == 1U)
    {
        /* Read volatile values into locals first to avoid Pa082 */
        dacOutVal     = *config->pDacCtrl1->pDacOutAddr;
        scalingFactor = config->pDacCtrl1->dacScalingFactor;

        if (config->pDacCtrl1->mode == 0U) /* Bipolar */
        {
            config->pDacCtrl1->dacData = (uint32_t)roundf((dacOutVal * scalingFactor) + 2048.0f);
        }
        else /* Unipolar */
        {
            config->pDacCtrl1->dacData = (uint32_t)(dacOutVal * scalingFactor);
        }
        if ((config->pDacCtrl1->dacData < 4095U) && (config->pDacCtrl1->dacData > 0U))
        {
            DL_CMPSSLITE_setDACValueLow((CMPSS_LITE_Regs *)config->dacInst, config->pDacCtrl1->dacData);
        }
    }

    /* DAC Channel 2 */
    if (config->pDacCtrl2->dacEn == TRUE)
    {
        /* Read volatile values into locals first to avoid Pa082 */
        dacOutVal     = *config->pDacCtrl2->pDacOutAddr;
        scalingFactor = config->pDacCtrl2->dacScalingFactor;

        if (config->pDacCtrl2->mode == 0U) /* Bipolar */
        {
            config->pDacCtrl2->dacData = (uint32_t)roundf((dacOutVal * scalingFactor) + 2048.0f);
        }
        else /* Unipolar */
        {
            config->pDacCtrl2->dacData = (uint32_t)(dacOutVal * scalingFactor);
        }
        /* TODO: Syscfg Dependency - Uncomment when DAC2 is configured in SysConfig */
        /* DL_CMPSSLITE_setDACValueLow(FOC_DAC_OUT_INST, config->pDacCtrl2->dacData); */
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_DEBUG_INTERFACE_H */

/** @} */
