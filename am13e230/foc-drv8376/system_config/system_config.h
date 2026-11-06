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
 * \file system_config.h
 * \brief System configuration for the 3-shunt Hall FOC port to the DRV8376EVM (LP-AM13E230)
 *
 * Project-local configuration. Declares all HAL hardware objects (FOC interface,
 * gate driver, Hall sensor interface) and the project-specific SYSTEM_Vars_t
 * instance. No preprocessor conditionals — Hall support is unconditional here.
 *
 * \defgroup HALL_SYSTEM_CONFIG_3SH DRV8376 Three-Shunt Hall System Configuration
 * \brief System and hardware object declarations for Motor 1 with DRV8376 gate
 *        driver, three-shunt current sensing, and Hall sensor position feedback
 * @{
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_debug_interface.h"
#include "hal_foc_interface.h"
#include "hal_gate_driver.h"
#include "drv8376_interface.h"
#include "hal_hall_sen_interface.h"

/**
 * \anchor HALL_3SH_EXTERN_DECL
 * \name External Declarations — FOC and Gate Driver
 * @{
 */

/** \brief Motor 1 Driver Dead Time */
#define SYS_M1_PWM_DEADTIME_uS (0.25f)

/** \brief Motor 1 ADC Sampling Time */
#define SYS_M1_ADC_SAMPTIME_uS (0.45f)

/** \brief CSA sampling allowance for the slowest unconfirmed SLEW setting.
 * minCMP = 0.25 us dead time + 6.3 us noise allowance + 0.45 us acquisition = 7 us.
 * With a 25 us half-period this clamps PWM duty to approximately 28..72 percent.
 * ADC trigger is 0.25 us after zero, after the guaranteed low-side settling interval.
 */
#define SYS_M1_ADC_NOISE_WIN_uS (6.3f)

/** \brief Gate driver function table for DRV8376 */
extern const HAL_GD_vTable_t halDRV8376FnTable;

/** \brief Gate driver data object for Motor 1 */
extern const HAL_GD_DATA_Obj drv8376Data_M1;

/** \brief DRV8376 instance object for Motor 1 */
extern const HAL_DRV8376_Obj drv8376Inst_M1;

/** \brief FOC function table for three-shunt configuration */
extern const HAL_FOC_vTable_t hal3SFOCFnTable;

/** \brief FOC configuration for Motor 1 */
extern const HAL_FOC_Config_t halFOCConfig_M1;

/** \brief Gate driver interface instance for Motor 1 */
extern const HAL_GD_Interface_t halGDInst;

/** \brief FOC interface instance for Motor 1 */
extern const HAL_FOC_Interface_t halFOCInterface;

/** @} */

/**
 * \anchor HALL_3SH_HALL_EXTERN_DECL
 * \name External Declarations — Hall Sensor
 * @{
 */

/** \brief Hall sensor hardware configuration for Motor 1 (const, placed in flash) */
extern HAL_Hall_Config_t hallConfig_M1;  /* FRL port: writable, see system_config.c */

/** \brief Hall sensor runtime data for Motor 1 (mutable, placed in RAM) */
/** \brief Hall sensor runtime data for Motor 1 (mutable, placed in RAM) */
extern HAL_Hall_Data_t hallData_M1;

/** \brief Hall sensor interface for Motor 1 */
extern const HAL_Hall_Interface_t hallInterface_M1;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CONFIG_H */

/** @} */
