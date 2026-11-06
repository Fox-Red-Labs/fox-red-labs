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
 * \file hal_obj.h
 * \brief Hardware Abstraction Layer (HAL) object definitions
 *
 * \defgroup HAL_OBJ HAL Object Definitions
 * \brief Structures and types for HAL peripheral configuration
 * @{
 */

#ifndef HAL_OBJ_H
#define HAL_OBJ_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

/* modules */
#include "hal_data.h"
#include "soc.h"

/**
 * \brief Maximum DAC value (12-bit resolution)
 */
#define DAC_MAX_VALUE 4095U

/**
 * \brief ADC interrupt number type
 * \note Placeholder type for M33 compatibility
 */
typedef uint16_t ADC_IntNumber;

/**
 * \brief ADC post-processing block number type
 * \note Placeholder type for M33 compatibility
 */
typedef uint16_t ADC_PPBNumber;

/**
 * \brief ADC start-of-conversion number type
 * \note Placeholder type for M33 compatibility
 */
typedef uint16_t ADC_SOCNumber;

/**
 * \brief ECAP events type
 * \note Placeholder type for M33 compatibility
 */
typedef uint16_t ECAP_Events;

/**
 * \brief Enumeration for supported evaluation kit boards
 */
typedef enum
{
    BOARD_BSXL8323RS_REVA  = 0, /**< \brief BOOSTXL_8323RS board */
    BOARD_BSXL8323RH_REVB  = 1, /**< \brief BOOSTXL_8323RH board */
    BOARD_BSXL8353RS_REVA  = 2, /**< \brief BOOSTXL_8353RS board */
    BOARD_BSXL3PHGAN_REVA  = 3, /**< \brief BOOSTXL_3PHGAN board */
    BOARD_HVMTRPFC_REV1P1  = 4, /**< \brief HVMTRPFC_REV1P1 board */
    BOARD_BSXL8316RT_REVA  = 5, /**< \brief BOOSTXL_8316RT board */
    BOARD_DRV8329AEVM_REVA = 6  /**< \brief BOOSTXL-DRV8329 board */
} Board_Kit_e;

/**
 * \brief Enumeration for current sensing types
 */
typedef enum
{
    CURSEN_TYPE_SINGLE_SHUNT = 0, /**< \brief Single shunt current sensing */
    CURSEN_TYPE_THREE_SHUNT  = 1, /**< \brief Three shunt current sensing */
    CURSEN_TYPE_INLINE_SHUNT = 2, /**< \brief Inline shunt with isolated amplifier */
    CURSEN_TYPE_INLINE_HALL  = 3, /**< \brief Hall effect current sensor */
    CURSEN_TYPE_INLINE_SDFM  = 4  /**< \brief Inline shunt with SDFM */
} CURRENTSEN_Type_e;

/**
 * \brief Enumeration for comparator types
 */
typedef enum
{
    COMP_LOW    = 0, /**< \brief Low comparator of instance */
    COMP_HIGH   = 1, /**< \brief High comparator of instance */
    COMP_WINDOW = 2  /**< \brief Window comparator mode */
} COMP_Type_e;

/**
 * \brief Interrupt configuration structure
 */
typedef struct _HAL_INT_Config_t_
{
    uint32_t      intPeripheralBase; /**< \brief Peripheral base address for interrupt */
    ADC_IntNumber intPeripheralNum;  /**< \brief Peripheral interrupt number */
    IRQn_Type     intNVICNum;        /**< \brief NVIC interrupt number */
} HAL_INT_Config_t;

/**
 * \brief PWM configuration structure
 *
 * Contains all PWM-related configuration including trigger sources
 * for both single-shunt and three-shunt current sensing.
 */
typedef struct _HAL_PWM_Config_t_
{
    uint32_t pwmHandle;                    /**< \brief PWM peripheral handle */
    uint32_t pwmTrigHandle;                /**< \brief PWM trigger handle for single shunt */
    uint16_t pwmPeriod;                    /**< \brief PWM period value */
    uint16_t numPWMTicksPerISRTick;        /**< \brief PWM ticks per ISR tick */
    uint16_t MotorStoptripInputXBARSource; /**< \brief Motor stop trip XBAR source */
    uint32_t overCurrentTZSouce;           /**< \brief Over-current trip zone source */
    uint32_t GateDriverFaultTZSource;      /**< \brief Gate driver fault trip zone source */
    uint32_t MotorStopTZSource;            /**< \brief Motor stop trip zone source */

    /* Triggers for Three Shunt */
    uint16_t adcTrigPWMSource; /**< \brief ADC trigger PWM source */

    /* Triggers for Single Shunt */
    uint16_t adcTrigPWMUpCountASource;   /**< \brief ADC trigger up-count A source */
    uint16_t adcTrigPWMUpCountBSource;   /**< \brief ADC trigger up-count B source */
    uint16_t adcTrigPWMDownCountASource; /**< \brief ADC trigger down-count A source */
    uint16_t adcTrigPWMDownCountBSource; /**< \brief ADC trigger down-count B source */

    /* Trigger registers for Single Shunt support */
    volatile uint32_t *pwmDownCountTriggerSource;  /**< \brief Down-count trigger register */
    volatile uint32_t *pwmUpCountTriggerSourceA;   /**< \brief Up-count trigger A register */
    volatile uint32_t *pwmUpCountTriggerSourceB;   /**< \brief Up-count trigger B register */
    volatile uint32_t *pwmDownCountTriggerSourceA; /**< \brief Down-count trigger A register */
    volatile uint32_t *pwmDownCountTriggerSourceB; /**< \brief Down-count trigger B register */
    volatile uint16_t  gateDriverDelay;
} HAL_PWM_Config_t;

/**
 * \brief Single-shunt comparator configuration structure
 */
typedef struct _HAL_1S_CMP_Config_t_
{
    uint32_t    cmpssHandle[1]; /**< \brief Comparator handle */
    COMP_Type_e compType[1];    /**< \brief Comparator type */
} HAL_1S_CMP_Config_t;

/**
 * \brief ECAP configuration structure for IPD pulse measurement
 */
typedef struct _HAL_ECAP_Config_t_
{
    uint32_t    base;      /**< \brief ECAP base address */
    ECAP_Events highEvent; /**< \brief High event capture for IPD pulse */
} HAL_ECAP_Config_t;

/**
 * \brief Three-shunt comparator configuration structure
 */
typedef struct _HAL_3S_CMP_Config_t_
{
    uint32_t    cmpssHandle[3]; /**< \brief Comparator handles for 3 phases */
    COMP_Type_e compType[3];    /**< \brief Comparator types for 3 phases */
} HAL_3S_CMP_Config_t;

/**
 * \anchor HAL_COMP_CAL_CONSTANTS
 * \brief Comparator DAC offset calibration sweep constants
 *
 * Used by HAL_3S_CompOffsetCalInit/Step and HAL_1S_CompOffsetCalInit/Step
 * to define the binary-search window for the CMPSS-Lite DAC trip-point.
 * The 12-bit DAC range is 0–4095; the window 1800–2400 covers the expected
 * zero-current operating point.
 */
/** \brief DAC sweep lower bound (inclusive), raw 12-bit counts */
#define HAL_COMP_DAC_SWEEP_LOW_COUNTS  (0)
/** \brief DAC sweep upper bound (inclusive), raw 12-bit counts */
#define HAL_COMP_DAC_SWEEP_HIGH_COUNTS (4095)
/** \brief DAC midpoint fallback value, raw 12-bit counts */
#define HAL_COMP_DAC_SWEEP_MID_COUNTS  (2047U)
/** \brief Comparator settle time in CPU cycles (2 ms at 200 MHz) */
#define HAL_COMP_DAC_SETTLE_CYCLES     (400000U)

/**
 * \brief Three-shunt comparator DAC offset calibration context
 *
 * Holds the binary-search state for all three phase comparator channels.
 * One static instance is kept per translation unit in hal_3sh_foc_interface.c.
 */
typedef struct
{
    uint16_t DacVal[3];     /**< \brief Last DAC midpoint written per channel */
    uint16_t Lo[3];         /**< \brief Binary search lower bound (inclusive) per channel */
    uint16_t Hi[3];         /**< \brief Binary search upper bound (inclusive) per channel */
    uint16_t InitStatus[3]; /**< \brief Comparator filter-output status captured at cal start */
    uint8_t  Done[3];       /**< \brief 1 = binary search converged for this channel */
} HAL_3S_CompCalCtx_t;

/**
 * \brief Single-shunt comparator DAC offset calibration context
 *
 * Holds the binary-search state for the DC-link comparator channel.
 * One static instance is kept per translation unit in hal_1sh_foc_interface.c.
 */
typedef struct
{
    uint16_t DacVal; /**< \brief Last DAC midpoint written */
    uint16_t Lo;     /**< \brief Binary search lower bound (inclusive) */
    uint16_t Hi;     /**< \brief Binary search upper bound (inclusive) */
    uint8_t  Done;   /**< \brief 1 = binary search converged */
} HAL_1S_CompCalCtx_t;

/**
 * \brief ADC result configuration structure
 */
typedef struct _HAL_ADC_Result_Config_t_
{
    uint32_t      adcResultBase; /**< \brief ADC result register base address */
    ADC_SOCNumber socNum;        /**< \brief Start-of-conversion number */
} HAL_ADC_Result_Config_t;

/**
 * \brief Handle to ADC result configuration
 */
typedef struct _HAL_ADC_Result_Config_t_ *HAL_ADC_Result_Config_Handle;

/**
 * \brief I2C slave address for EEPROM
 */
#define I2C_SLAVE_ADDRESS 0x50U

/**
 * \brief Hardware Abstraction Layer (HAL) object
 *
 * Contains all handles to peripherals. When accessing a peripheral
 * on a processor, use a HAL function along with the HAL handle
 * for that processor to access its peripherals.
 */
typedef struct _HAL_Obj_
{
    uint32_t timerHandle[1]; /**< \brief Timer peripheral handles */
} HAL_Obj;

/**
 * \brief HAL handle type
 *
 * The HAL handle is a pointer to a HAL object. In all HAL functions
 * the HAL handle is passed so that the function knows what peripherals
 * are to be accessed.
 */
typedef struct _HAL_Obj_ *HAL_Handle;

/**
 * \brief Hall sensor GPIO and timer configuration (read-only, placed in flash)
 */
typedef struct _HAL_Hall_Config_t_
{
    uint32_t hallAGpioPort; /**< \brief GPIO port base for Hall A inputs (e.g. MTR1_HALL_INP_PORT) */
    uint32_t hallBGpioPort; /**< \brief GPIO port base for Hall B inputs (e.g. MTR1_HALL_INP_PORT) */
    uint32_t hallCGpioPort; /**< \brief GPIO port base for Hall C inputs (e.g. MTR1_HALL_INP_PORT) */

    uint32_t hallPinA; /**< \brief GPIO pin mask for Hall A (e.g. MTR1_HALL_INP_PIN_A_PIN) */
    uint32_t hallPinB; /**< \brief GPIO pin mask for Hall B */
    uint32_t hallPinC; /**< \brief GPIO pin mask for Hall C */
    uint32_t ecapBase; /**< \brief ECAP peripheral base for speed capture (required, non-zero) */

    /**
     * \brief ECAP input mux values for Hall signals A, B, C.
     *
     * Indexed directly by the next-edge lookup tables:
     *   ecapInputHall[0] = Hall A XBAR input
     *   ecapInputHall[1] = Hall B XBAR input
     *   ecapInputHall[2] = Hall C XBAR input
     *
     * Stored as uint32_t to keep this struct MCU-agnostic (no dl_ecap.h dependency).
     * Cast to DL_ECAP_INPUT when calling DL_ECAP_selectECAPInput().
     * Populated from SysConfig-generated HALL_SPEED_CAP_ECAP_INPUT_HALL_A/B/C defines.
     */
    uint32_t ecapInputHall[3U]; /**< \brief DL_ECAP_INPUT values: [0]=Hall A, [1]=Hall B, [2]=Hall C */

    /**
     * \brief ECAP capture event used for Hall speed measurement and polarity control.
     *
     * Stored as uint32_t (cast of DL_ECAP_EVENT enum) to keep this struct MCU-agnostic.
     * Must match the event configured in SysConfig for HALL_SPEED_CAP (typically EVENT_1).
     * Used in both DL_ECAP_setEventPolarity() and DL_ECAP_getEventTimeStamp() calls.
     */
    uint32_t ecapCaptureEvent; /**< \brief DL_ECAP_EVENT value for Hall speed capture (e.g. DL_ECAP_EVENT_1) */

    /**
     * \brief Pointer to the calibrated angle table for forward rotation.
     *
     * Points to `HallCal_Data_t.calAngleFwd_rad[8]` after a successful
     * bidirectional calibration.  Set to NULL_PTR to use the default static
     * `HAL_Hall_AngleTable_rad` table (30° sector-centre values).
     * Indexed by 3-bit Hall state; valid states 1–6.
     */
    const float32_t *calAngleFwdTablePtr; /**< \brief Calibrated FWD angle table, or NULL_PTR for default */

    /**
     * \brief Pointer to the calibrated angle table for reverse rotation.
     *
     * Points to `HallCal_Data_t.calAngleRev_rad[8]` after a successful
     * bidirectional calibration.  Set to NULL_PTR to use the default static
     * `HAL_Hall_AngleTable_rad` table.
     * Indexed by 3-bit Hall state; valid states 1–6.
     */
    const float32_t *calAngleRevTablePtr; /**< \brief Calibrated REV angle table, or NULL_PTR for default */

    uint16_t  numPolePairs;    /**< \brief Motor pole pair count for speed scaling */
    float32_t angleOffset_rad; /**< \brief Electrical angle offset for Hall mounting compensation */
    float32_t sysclkFreq_MHz;  /**< \brief System clock frequency in MHz (e.g. USER_SYSTEM_FREQ_MHz) */
    float32_t isrPeriod_usec;  /**< \brief ISR execution period in microseconds (e.g. USER_M1_ISR_PERIOD_usec) */
} HAL_Hall_Config_t;

/**
 * \brief Hall sensor runtime data (mutable, placed in RAM)
 */
typedef struct _HAL_Hall_Data_t_
{
    uint8_t   hallState;         /**< \brief Current 3-bit Hall state (bits: HC=2, HB=1, HA=0) */
    uint8_t   hallStatePrev;     /**< \brief Previous Hall state for transition detection */
    uint8_t   hallCandidate;     /**< \brief FRL port: raw state waiting to be confirmed (debounce) */
    uint8_t   hallStableCnt;     /**< \brief FRL port: consecutive ISR reads of hallCandidate */
    float32_t angleHall_rad;     /**< \brief Decoded electrical angle in radians */
    float32_t speedHall_Hz;      /**< \brief Filtered signed speed in Hz (positive = forward, negative = reverse) */
    float32_t speedHall_raw_Hz;  /**< \brief Unfiltered speed magnitude in Hz (always >= 0) */
    uint32_t  capturePeriod_cnt; /**< \brief ECAP capture period in timer counts */
    uint32_t  noEdgeIsrCnt;      /**< \brief ISR cycles elapsed since last Hall edge (staleness counter) */
    bool      flagHallFault;     /**< \brief TRUE if invalid Hall state (000 or 111) detected */
    bool      flagNewEdge;       /**< \brief TRUE for one ISR cycle after a Hall transition */
    /**
     * \brief Motor rotation direction: 1.0f = forward, -1.0f = reverse.
     *
     * Written by the main (background) loop via FOC_runHallObserver() before
     * HAL_Hall_readState() is called each ISR cycle.  Read by
     * HAL_Hall_updateEcapInput_inline() to select the correct next-edge XBAR.
     * Initialised to 1.0f (forward) in HAL_Hall_reset().
     */
    float32_t direction; /**< \brief Motor direction: 1.0f = forward, -1.0f = reverse */
    float32_t
        speedFilter_rps; /**< \brief First-order speed filter pole in rad/s (dynamic, updated by low-priority loop) */
} HAL_Hall_Data_t;

/**
 * \brief Hall sensor interface — binds hardware config and runtime data
 */
typedef struct _HAL_Hall_Interface_t_
{
    const HAL_Hall_Config_t *const hallConfigHandle; /**< \brief Pointer to const config in flash */
    HAL_Hall_Data_t *const         hallDataHandle;   /**< \brief Pointer to mutable data in RAM */
} HAL_Hall_Interface_t;
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

/** @} */ /* End of HAL_OBJ group */

#endif /* HAL_OBJ_H */
