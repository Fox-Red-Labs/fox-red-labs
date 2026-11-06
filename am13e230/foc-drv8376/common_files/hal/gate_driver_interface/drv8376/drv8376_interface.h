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
 * \file drv8376_interface.h
 * \brief DRV8376EVM gate driver interface for the 3-shunt solutions (LP-AM13E230 + DRV8376EVM)
 *
 * New file, not part of TI's SDK. Derived from drv8329a_interface.h (nSLEEP and DRVOFF enable
 * object) and drv8300drge_interface.h / drv8323rh_interface.h (3-shunt data layout).
 * Every number cites its source. Anything not proved on the bench is marked UNPROVEN.
 *
 * Board facts (TI SLVUD20 DRV8376EVM user's guide, data sheet SLVSHD4A):
 *   - The EVM is a 40-pin BoosterPack. Its six PWM inputs land on MCPWM4 1A..3B, the same pins
 *     the DRV8323RH BoosterPack uses, so the PWM block of the SysConfig file is unchanged.
 *   - 6x PWM mode (INHx and INLx independent): INH=1, INL=0 drives high; both 0 is Hi-Z.
 *   - Phase current: one low-side shunt per phase and a current-sense amplifier (CSA) per phase.
 *     SOx = VREF/2 + I x G (G = 0.4, 1, 2.5 or 5 V/A, set by resistors on the board). The CSA is
 *     only valid while the low-side FET conducts.
 *   - Bus and phase voltages: 143 k over 6.04 k divider with 100 nF (VSENVM, VSENA, VSENB, VSENC).
 *
 * @{
 */

#ifndef DRV8376_INTERFACE_H
#define DRV8376_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DRV8376EVM_REVA
#include "stdint.h"
#include "hal.h"

//! \brief Defines the nominal DC bus voltage for this bench, V (12 V adapter, 0.5 A)
#define DRV8376_NOMINAL_DC_BUS_VOLTAGE_V (12.0f)

//! \brief Bus and phase-voltage divider on the EVM (SLVUD20 figure 4-1): 143 k over 6.04 k, 100 nF
#define DRV8376_VDIV_RTOP_OHM    (143000.0f)
#define DRV8376_VDIV_RBOT_OHM    (6040.0f)
#define DRV8376_VDIV_C_F         (100.0e-9f)
#define DRV8376_ADC_VREF_V       (3.3f)  //!< sysctl.VREFSource = internal 3.3 V in the SysConfig file

//! \brief Defines the maximum voltage at the AD converter: 3.3 V x (143 k + 6.04 k) / 6.04 k = 81.43 V
#define DRV8376_ADC_FULL_SCALE_VOLTAGE_V \
    (DRV8376_ADC_VREF_V * (DRV8376_VDIV_RTOP_OHM + DRV8376_VDIV_RBOT_OHM) / DRV8376_VDIV_RBOT_OHM)

//! \brief Defines the analog voltage filter pole location, Hz: 1 / (2 pi (143 k || 6.04 k) 100 nF) = 274.6 Hz
#define DRV8376_VOLTAGE_FILTER_POLE_Hz \
    (1.0f / (MATH_TWO_PI * (DRV8376_VDIV_RTOP_OHM * DRV8376_VDIV_RBOT_OHM / (DRV8376_VDIV_RTOP_OHM + DRV8376_VDIV_RBOT_OHM)) * DRV8376_VDIV_C_F))

//! \brief CSA gain in volts per amp. UNPROVEN for this board: set by resistors R54/R58 (0.4, 1, 2.5 or 5).
//!        1 V/A is what the bench notes say the board wears (am13e230/bldc-drv8376/README.md).
//!        Read it off the board before the first closed-loop run. At 1 V/A one count is 0.806 mA and
//!        the linear range is +/-1.25 A at nominal VREF=3.0 V (data sheet: output linear 0.25 V to VREF - 0.25 V).
#define DRV8376_CSA_GAIN_V_PER_A (1.0f)

//! \brief Nominal CSA reference for J2 center to separate J3 (on-board REF3130).
//!        This is distinct from the MCU ADC's 3.3 V reference. J4 uses DIV; J6 stays open.
//!        Physical placement is not established by this constant. ISR calibration measures the offset.
#define DRV8376_CSA_VREF_V (3.0f)

//! \brief Defines the maximum current at the AD converter: the current that 4096 counts represent
//!        (SDK convention: I = (counts - offset) x FS / 4096). 3.3 V / 1 V/A = 3.3 A.
#define DRV8376_ADC_FULL_SCALE_CURRENT_A (DRV8376_ADC_VREF_V / DRV8376_CSA_GAIN_V_PER_A)

//! \brief Defines the voltage scale factor for the system
#define DRV8376_VOLTAGE_SF (DRV8376_ADC_FULL_SCALE_VOLTAGE_V / 4096.0f)

//! \brief Defines the current scale factor for the system
#define DRV8376_CURRENT_SF (DRV8376_ADC_FULL_SCALE_CURRENT_A / 4096.0f)

//! \brief Defines the current scale invert factor for the system
#define DRV8376_CURRENT_INV_SF (4096.0f / DRV8376_ADC_FULL_SCALE_CURRENT_A)

//! \brief Defines the analog voltage filter pole location, rad/s
#define DRV8376_VOLTAGE_FILTER_POLE_rps (MATH_TWO_PI * DRV8376_VOLTAGE_FILTER_POLE_Hz)

//! \brief Measured sink current raises CSA output; motor-positive current is opposite.
//! Six-vector diagnostic established polarity on this board. Gain remains nominal.
//! Must pair with non-inverted high comparators: HAL sets offset - Ilimit/current_sf,
//! placing the overcurrent threshold above zero for this negative scale.
#define DRV8376_SIGN_CURRENT_SF (-1.0f)

//! \brief ADC current offsets (counts): VREF/2 on a 12-bit, 3.3 V ADC. The ISR calibrates the real
//!        rest level at start; this is only the seed and the CMPSS fallback.
#define DRV8376_IDC_OFFSET_AD (4096.0f * (DRV8376_CSA_VREF_V * 0.5f) / DRV8376_ADC_VREF_V)

//! \brief ADC current offset for CMPSS
#define DRV8376_IS_OFFSET_CMPSS DRV8376_IDC_OFFSET_AD

//! \brief ADC current offsets checking range, A/B/C phases (counts), +/-150 as for the DRV8323RH
#define DRV8376_IA_OFFSET_AD_MAX (DRV8376_IDC_OFFSET_AD + 150.0f)
#define DRV8376_IB_OFFSET_AD_MAX (DRV8376_IDC_OFFSET_AD + 150.0f)
#define DRV8376_IC_OFFSET_AD_MAX (DRV8376_IDC_OFFSET_AD + 150.0f)
#define DRV8376_IA_OFFSET_AD_MIN (DRV8376_IDC_OFFSET_AD - 150.0f)
#define DRV8376_IB_OFFSET_AD_MIN (DRV8376_IDC_OFFSET_AD - 150.0f)
#define DRV8376_IC_OFFSET_AD_MIN (DRV8376_IDC_OFFSET_AD - 150.0f)

//! \brief ADC voltage offsets for A, B, and C phases: phase voltage over bus voltage at 50 percent duty.
//!        UNPROVEN for the EVM (no bias network, unlike the DRV8323RH BoosterPack's 0.507); the ISR
//!        re-learns it during offset calibration, so 0.5 is only the seed.
#define DRV8376_VA_OFFSET_SF (0.5f)
#define DRV8376_VB_OFFSET_SF (0.5f)
#define DRV8376_VC_OFFSET_SF (0.5f)

#define DRV8376_VA_OFFSET_SF_MAX (DRV8376_VA_OFFSET_SF + 0.05f)
#define DRV8376_VB_OFFSET_SF_MAX (DRV8376_VB_OFFSET_SF + 0.05f)
#define DRV8376_VC_OFFSET_SF_MAX (DRV8376_VC_OFFSET_SF + 0.05f)
#define DRV8376_VA_OFFSET_SF_MIN (DRV8376_VA_OFFSET_SF - 0.05f)
#define DRV8376_VB_OFFSET_SF_MIN (DRV8376_VB_OFFSET_SF - 0.05f)
#define DRV8376_VC_OFFSET_SF_MIN (DRV8376_VC_OFFSET_SF - 0.05f)

//! \brief DC bus over voltage threshold (chip rating 65 V; plan 06 asks for 60 V)
#define DRV8376_OVER_VOLTAGE_FAULT_V (60.0f)

//! \brief DC bus over voltage recovery threshold
#define DRV8376_OVER_VOLTAGE_NORM_V (58.0f)

//! \brief DC bus under voltage threshold (chip UV lockout 4.5 V; plan 06 asks for 6 V)
#define DRV8376_UNDER_VOLTAGE_FAULT_V (6.0f)

//! \brief DC bus under voltage recovery threshold
#define DRV8376_UNDER_VOLTAGE_NORM_V (7.0f)

//! \brief Defines the number of current sensors
#define DRV8376_NUM_CURRENT_SENSORS (3)

//! \brief Defines the number of Current Sense Readings
#define DRV8376_NUM_CURRENT_SENSE_READINGS (3)

//! \brief Defines the number of current sensor reading
#define DRV8376_NUM_CURRENT_SENS_VAL (3)

//! \brief Defines the number of voltage sensors including DC bus
#define DRV8376_NUM_VOLTAGE_SENSORS (4)

//! \brief Defines the ADC trigger delay after the PWM counter zero, in microseconds. Same 0.25 us as the
//!        DRV8323RH; the wait that lets the CSA settle comes from the minimum compare clamp in
//!        system_config.h (SYS_M1_ADC_NOISE_WIN_uS), not from this delay.
#define DRV8376_GD_PROP_DELAY (0.25f)

//! \brief nSLEEP to outputs-enabled wake wait in milliseconds. Data sheet tWAKE is 5.5 ms maximum.
#define DRV8376_WAKE_WAIT_ms (6U)

/**
 * \brief HAL Gate Driver object for DRV8376
 *
 * GPIO ports and pins for nSLEEP (high = awake) and DRVOFF (low = outputs on).
 */
typedef struct HAL_DRV8376_Obj
{
    uint32_t gateNSleepGPIO;     //!< GPIO port base address for nSLEEP pin
    uint32_t gateNSleepGPIONum;  //!< GPIO pin number for nSLEEP

    uint32_t gateDriverOffGPIO;     //!< GPIO port base address for DRVOFF pin
    uint32_t gateDriverOffGPIONum;  //!< GPIO pin number for DRVOFF

} HAL_DRV8376_Obj;

#ifdef __cplusplus
}
#endif

#endif /* DRV8376EVM_REVA */

#endif /* DRV8376_INTERFACE_H */

/** @} */
