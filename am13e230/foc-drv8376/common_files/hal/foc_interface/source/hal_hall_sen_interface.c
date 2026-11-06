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

#include "hal_foc_interface.h"
#include "hal_hall_sen_interface.h" /* HAL_Hall_updateEcapInput_inline() */

/*
 * \brief Reset Hall sensor data to safe initial state
 *
 * \param[inout] dataHandle    Hall data handle
 */
void HAL_Hall_reset(HAL_Hall_Data_t *dataHandle)
{
    dataHandle->hallState         = 0U;
    dataHandle->hallStatePrev     = 0U;
    dataHandle->angleHall_rad     = 0.0f;
    dataHandle->speedHall_Hz      = 0.0f;
    dataHandle->speedHall_raw_Hz  = 0.0f;
    dataHandle->capturePeriod_cnt = 0U;
    dataHandle->noEdgeIsrCnt      = 0U;
    dataHandle->flagHallFault     = false;
    dataHandle->flagNewEdge       = false;
    /* Default to forward rotation; overwritten by main loop before first ISR */
    dataHandle->direction         = 1.0f;
    /* speedFilter_rps is initialized in HAL_Hall_Init() from config and updated
     * dynamically by the low-priority state machine based on motor reference speed */
    dataHandle->speedFilter_rps   = 0.0f;
}

/*
 * \brief Initialize ECAP for Hall speed capture
 *
 * Starts the ECAP counter and enables timestamp capture.
 * ECAP peripheral is pre-configured by SysConfig (HALL_SPEED_CAP instance).
 *
 * \param[in]    configHandle  Hall configuration handle
 * \param[inout] dataHandle    Hall data handle (unused)
 */
void HAL_Hall_initSpeedCapture(const HAL_Hall_Config_t *configHandle, HAL_Hall_Data_t *dataHandle)
{
    /* ECAP is pre-configured by SysConfig (HALL_SPEED_CAP instance).
     * Enable timestamp capture and start the counter. */
    DL_ECAP_enableTimeStampCapture((ECAP_Regs *)configHandle->ecapBase);
    DL_ECAP_startCounter((ECAP_Regs *)configHandle->ecapBase);
}

/*
 * \brief Initialize Hall sensor interface
 *
 * Resets all Hall data fields to safe initial values and starts the
 * ECAP peripheral for speed capture.
 *
 * \param[in]    configHandle  Hall configuration handle (ecapBase must be non-zero)
 * \param[inout] dataHandle    Hall data handle
 */
void HAL_Hall_Init(const HAL_Hall_Config_t *configHandle, HAL_Hall_Data_t *dataHandle)
{
    HAL_Hall_reset(dataHandle);
    /* Seed speedFilter_rps from config as the initial value.
     * The low-priority state machine will update it dynamically based on
     * motor reference speed: speedFilter_rps = 2 * |speedRef_Hz| * 2*pi */
    dataHandle->speedFilter_rps = USER_M1_HALL_SPEED_FILTER_POLE_rps;
    HAL_Hall_initSpeedCapture(configHandle, dataHandle);
}

/*
 * \brief Update ECAP input mux for the next Hall edge (non-ISR wrapper)
 *
 * Thin non-inline wrapper around HAL_Hall_updateEcapInput_inline().
 * Intended for use outside the ISR hot-path (e.g. forced update at init time
 * after direction and hallState are known).
 *
 * \param[in] configHandle  Hall configuration handle
 * \param[in] dataHandle    Hall data handle (hallState and direction must be current)
 */
void HAL_Hall_updateEcapInput(const HAL_Hall_Config_t *configHandle, const HAL_Hall_Data_t *dataHandle)
{
    HAL_Hall_updateEcapInput_inline(configHandle, dataHandle);
}
