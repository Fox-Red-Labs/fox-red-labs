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
 * \file hal_hall_sen_interface.h
 * \brief Hall sensor HAL — ISR hot-path inline bodies and non-ISR declarations.
 *
 * \defgroup HAL_HALL_SEN_INTERFACE Hall Sensor HAL Interface
 * \brief Hall sensor position and speed sensing using ECAP capture
 * @{
 */

#ifndef HAL_HALL_SEN_INTERFACE_H_
#define HAL_HALL_SEN_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_obj.h"
#include "hal.h"
#include "userParams.h"

/* Included by hal_foc_interface.h after HAL_Hall_Config_t / HAL_Hall_Data_t are defined. */

/**
 * \brief Hall state to electrical angle lookup table (index = 3-bit Hall state)
 *
 * Valid states: 1, 2, 3, 4, 5, 6. States 0 and 7 are invalid (fault).
 * Angles are electrical angles at the center of each 60-degree sector.
 * Bit mapping: bit0 = HA, bit1 = HB, bit2 = HC.
 */
static const float32_t HAL_Hall_AngleTable_rad_fwd[8] = {
    0.0f,  /* 0b000 = INVALID */
    0.66,  /* 0b001 = 30 deg  */
    2.72,  /* 0b010 = 150 deg */
    1.77,  /* 0b011 = 90 deg  */
    -1.44, /* 0b100 = 270 deg */
    -0.33, /* 0b101 = 330 deg */
    -2.42, /* 0b110 = 210 deg */
    0.0f   /* 0b111 = INVALID */
};
/**
 * \brief Hall state to electrical angle lookup table (index = 3-bit Hall state)
 **/
static const float32_t HAL_Hall_AngleTable_rad_rev[8] = {
    0.0f,    /* 0b000 = INVALID */
    1.74f,   /* 0b001 = state 1 — entering from fwd state 3 */
    -2.46f,  /* 0b010 = state 2 — entering from fwd state 6 */
    2.69f,   /* 0b011 = state 3 — entering from fwd state 2 */
    -0.366f, /* 0b100 = state 4 — entering from fwd state 5 */
    0.610f,  /* 0b101 = state 5 — entering from fwd state 1 */
    -1.49f,  /* 0b110 = state 6 — entering from fwd state 4 */
    0.0f     /* 0b111 = INVALID */
};
/**
 * \brief Next-edge ECAP input index for forward rotation (index = 3-bit Hall state)
 *
 * Forward Hall sequence: 1 -> 3 -> 2 -> 6 -> 4 -> 5 -> 1 ...
 * The index selects which Hall signal will produce the next edge:
 *   0 = Hall A  (ecapInputHall[0])
 *   1 = Hall B  (ecapInputHall[1])
 *   2 = Hall C  (ecapInputHall[2])
 *
 * States 0 and 7 are invalid; index 0 is a safe don't-care default.
 */
/* Forward Hall sequence: 1(001)->3(011)->2(010)->6(110)->4(100)->5(101)->1
 * Next-edge signal = the bit that toggles going to the next state:
 *   state 1->3: bit1 toggles (HB 0->1) -> idx 1
 *   state 2->6: bit2 toggles (HC 0->1) -> idx 2
 *   state 3->2: bit0 toggles (HA 1->0) -> idx 0
 *   state 4->5: bit0 toggles (HA 0->1) -> idx 0
 *   state 5->1: bit2 toggles (HC 1->0) -> idx 2
 *   state 6->4: bit1 toggles (HB 1->0) -> idx 1
 */
static const uint8_t HAL_Hall_NextInputIdxFwd_tbl[8U] = {
    0U, /* 0b000 INVALID — don't-care, default to HA */
    1U, /* state 1 (001) — next FWD edge: HB (bit1 toggles 0->1) */
    2U, /* state 2 (010) — next FWD edge: HC (bit2 toggles 0->1) */
    0U, /* state 3 (011) — next FWD edge: HA (bit0 toggles 1->0) */
    0U, /* state 4 (100) — next FWD edge: HA (bit0 toggles 0->1) */
    2U, /* state 5 (101) — next FWD edge: HC (bit2 toggles 1->0) */
    1U, /* state 6 (110) — next FWD edge: HB (bit1 toggles 1->0) */
    0U  /* 0b111 INVALID — don't-care, default to HA */
};

/**
 * \brief Next-edge ECAP input index for reverse rotation (index = 3-bit Hall state)
 *
 * Reverse Hall sequence: 1 -> 5 -> 4 -> 6 -> 2 -> 3 -> 1 ...
 * The index selects which Hall signal will produce the next edge:
 *   0 = Hall A  (ecapInputHall[0])
 *   1 = Hall B  (ecapInputHall[1])
 *   2 = Hall C  (ecapInputHall[2])
 *
 * States 0 and 7 are invalid; index 0 is a safe don't-care default.
 */
/* Reverse Hall sequence: 1(001)->5(101)->4(100)->6(110)->2(010)->3(011)->1
 * Next-edge signal = the bit that toggles going to the next state:
 *   state 1->5: bit2 toggles (HC 0->1) -> idx 2
 *   state 2->3: bit0 toggles (HA 0->1) -> idx 0
 *   state 3->1: bit1 toggles (HB 1->0) -> idx 1
 *   state 4->6: bit1 toggles (HB 0->1) -> idx 1
 *   state 5->4: bit0 toggles (HA 1->0) -> idx 0
 *   state 6->2: bit2 toggles (HC 1->0) -> idx 2
 */
static const uint8_t HAL_Hall_NextInputIdxRev_tbl[8U] = {
    0U, /* 0b000 INVALID — don't-care, default to HA */
    2U, /* state 1 (001) — next REV edge: HC (bit2 toggles 0->1) */
    0U, /* state 2 (010) — next REV edge: HA (bit0 toggles 0->1) */
    1U, /* state 3 (011) — next REV edge: HB (bit1 toggles 1->0) */
    1U, /* state 4 (100) — next REV edge: HB (bit1 toggles 0->1) */
    0U, /* state 5 (101) — next REV edge: HA (bit0 toggles 1->0) */
    2U, /* state 6 (110) — next REV edge: HC (bit2 toggles 1->0) */
    0U  /* 0b111 INVALID — don't-care, default to HA */
};

/**
 * \brief ECAP Event 1 polarity for the next edge in forward rotation (index = 3-bit Hall state)
 *
 * The polarity is determined by the current GPIO level of the next Hall signal:
 *   current level HIGH (1) -> next edge is FALLING  (DL_ECAP_EVENT_FALLING_EDGE = 1)
 *   current level LOW  (0) -> next edge is RISING   (DL_ECAP_EVENT_RISING_EDGE  = 0)
 *
 * Forward sequence: 1->3->2->6->4->5->1
 *   state 1 (001): next=HB, HB=0 -> RISING
 *   state 2 (010): next=HA, HA=0 -> RISING
 *   state 3 (011): next=HC, HC=0 -> RISING
 *   state 4 (100): next=HB, HB=0 -> RISING
 *   state 5 (101): next=HA, HA=1 -> FALLING
 *   state 6 (110): next=HC, HC=1 -> FALLING
 */
/* Polarity = level of the next-edge signal IN THE CURRENT STATE:
 *   0 (LOW)  -> next edge is RISING  (DL_ECAP_EVENT_RISING_EDGE  = 0U)
 *   1 (HIGH) -> next edge is FALLING (DL_ECAP_EVENT_FALLING_EDGE = 1U)
 *
 *   state 1 (001): next=HB, HB(bit1)=0 -> RISING
 *   state 2 (010): next=HC, HC(bit2)=0 -> RISING
 *   state 3 (011): next=HA, HA(bit0)=1 -> FALLING
 *   state 4 (100): next=HA, HA(bit0)=0 -> RISING
 *   state 5 (101): next=HC, HC(bit2)=1 -> FALLING
 *   state 6 (110): next=HB, HB(bit1)=1 -> FALLING
 */
static const uint8_t HAL_Hall_NextPolarityFwd_tbl[8U] = {
    0U, /* 0b000 INVALID — don't-care, RISING  */
    0U, /* state 1 (001) — HB(bit1)=0 -> RISING  */
    0U, /* state 2 (010) — HC(bit2)=0 -> RISING  */
    1U, /* state 3 (011) — HA(bit0)=1 -> FALLING */
    0U, /* state 4 (100) — HA(bit0)=0 -> RISING  */
    1U, /* state 5 (101) — HC(bit2)=1 -> FALLING */
    1U, /* state 6 (110) — HB(bit1)=1 -> FALLING */
    0U  /* 0b111 INVALID — don't-care, RISING  */
};

/**
 * \brief ECAP Event 1 polarity for the next edge in reverse rotation (index = 3-bit Hall state)
 *
 * Reverse sequence: 1->5->4->6->2->3->1
 *   state 1 (001): next=HC, HC=0 -> RISING
 *   state 2 (010): next=HC, HC=0 -> RISING
 *   state 3 (011): next=HA, HA=1 -> FALLING
 *   state 4 (100): next=HA, HA=0 -> RISING
 *   state 5 (101): next=HB, HB=0 -> RISING
 *   state 6 (110): next=HB, HB=1 -> FALLING
 */
/* Polarity = level of the next-edge signal IN THE CURRENT STATE:
 *   state 1 (001): next=HC, HC(bit2)=0 -> RISING
 *   state 2 (010): next=HA, HA(bit0)=0 -> RISING
 *   state 3 (011): next=HB, HB(bit1)=1 -> FALLING
 *   state 4 (100): next=HB, HB(bit1)=0 -> RISING
 *   state 5 (101): next=HA, HA(bit0)=1 -> FALLING
 *   state 6 (110): next=HC, HC(bit2)=1 -> FALLING
 */
static const uint8_t HAL_Hall_NextPolarityRev_tbl[8U] = {
    0U, /* 0b000 INVALID — don't-care, RISING  */
    0U, /* state 1 (001) — HC(bit2)=0 -> RISING  */
    0U, /* state 2 (010) — HA(bit0)=0 -> RISING  */
    1U, /* state 3 (011) — HB(bit1)=1 -> FALLING */
    0U, /* state 4 (100) — HB(bit1)=0 -> RISING  */
    1U, /* state 5 (101) — HA(bit0)=1 -> FALLING */
    1U, /* state 6 (110) — HC(bit2)=1 -> FALLING */
    0U  /* 0b111 INVALID — don't-care, RISING  */
};

/**
 * \brief Update ECAP input mux to capture the next Hall edge based on current state and direction
 *
 * Selects the ECAP input XBAR to whichever Hall signal (A, B, or C) will produce
 * the next edge, given the current 3-bit Hall state and the rotation direction stored
 * in dataHandle->direction.  Must be called after hallState has been updated.
 *
 * Uses two 8-entry lookup tables (HAL_Hall_NextInputIdxFwd_tbl /
 * HAL_Hall_NextInputIdxRev_tbl) to avoid switch-case per SDK coding standards.
 *
 * \param[in] configHandle  Hall configuration handle (ecapBase and ecapInputHallA/B/C must be set)
 * \param[in] dataHandle    Hall data handle (hallState and direction must be current)
 */
__attribute__((always_inline)) static inline void HAL_Hall_updateEcapInput_inline(const HAL_Hall_Config_t *configHandle,
                                                                                  const HAL_Hall_Data_t   *dataHandle)
{
    uint8_t idx;
    uint8_t polarity;

    if (dataHandle->direction >= 0.0f)
    {
        idx      = HAL_Hall_NextInputIdxFwd_tbl[dataHandle->hallState];
        polarity = HAL_Hall_NextPolarityFwd_tbl[dataHandle->hallState];
    }
    else
    {
        idx      = HAL_Hall_NextInputIdxRev_tbl[dataHandle->hallState];
        polarity = HAL_Hall_NextPolarityRev_tbl[dataHandle->hallState];
    }

    DL_ECAP_selectECAPInput((ECAP_Regs *)configHandle->ecapBase, (DL_ECAP_INPUT)configHandle->ecapInputHall[idx]);
    DL_ECAP_setEventPolarity((ECAP_Regs *)configHandle->ecapBase, (DL_ECAP_EVENT)configHandle->ecapCaptureEvent,
                             (DL_ECAP_EVENT_POLARITY)polarity);
}

/**
 * \brief Compute electrical angle from Hall table or speed integration
 *
 * On a new Hall edge (flagNewEdge == true): snaps angleHall_rad to the sector
 * center value from the calibrated or default lookup table (direction-aware),
 * plus the mounting offset.
 * Between edges (flagNewEdge == false): propagates the angle by integrating the
 * signed filtered speed estimate over one ISR period:
 *   angleHall_rad += speedHall_Hz * 2*pi * isrPeriod_usec * 1e-6
 * speedHall_Hz is signed (positive = forward, negative = reverse), so no
 * separate direction multiplier is needed here.
 * Wraps the result to [-pi, pi) in both cases.
 * No-op when flagHallFault is set.
 *
 * \param[in]    configHandle  Hall configuration handle
 * \param[inout] dataHandle    Hall data handle (angleHall_rad updated in place)
 */
__attribute__((always_inline)) static inline void HAL_Hall_computeAngle_inline(const HAL_Hall_Config_t *configHandle,
                                                                               HAL_Hall_Data_t         *dataHandle)
{
    if (dataHandle->flagHallFault == false)
    {
        if (dataHandle->flagNewEdge == true)
        {
            /* New Hall edge: enforce angle from sector-center lookup table.
             * The two tables differ by ~60° per state because the Hall sensor fires
             * at the leading edge of the magnetic pole in the direction of travel.
             * Using the wrong table causes a systematic 60° error and torque loss. */
            const float32_t *angleTable;

            if (dataHandle->direction >= 0.0f)
            {
                angleTable = (configHandle->calAngleFwdTablePtr != NULL) ? configHandle->calAngleFwdTablePtr
                                                                         : HAL_Hall_AngleTable_rad_fwd;
            }
            else
            {
                angleTable = (configHandle->calAngleRevTablePtr != NULL) ? configHandle->calAngleRevTablePtr
                                                                         : HAL_Hall_AngleTable_rad_rev;
            }

            dataHandle->angleHall_rad = angleTable[dataHandle->hallState] + configHandle->angleOffset_rad;
        }
        else
        {
            /* No Hall transition: integrate filtered speed to propagate angle
             * between Hall edges for smoother, higher-resolution tracking.
             * speedHall_Hz is now signed (positive = forward, negative = reverse),
             * so direction multiplier is removed to avoid double-applying sign. */
            dataHandle->angleHall_rad +=
                dataHandle->speedHall_Hz * MATH_TWO_PI * configHandle->isrPeriod_usec * 1.0e-6f;
        }

        /* Wrap angle to [-pi, pi) */
        if (dataHandle->angleHall_rad >= MATH_PI)
        {
            dataHandle->angleHall_rad -= MATH_TWO_PI;
        }
        else if (dataHandle->angleHall_rad < -MATH_PI)
        {
            dataHandle->angleHall_rad += MATH_TWO_PI;
        }
        else
        {
            /* angle already in range */
        }
    }
}

/**
 * \brief Read Hall sensor GPIO pins, update state, speed, and angle
 *
 * Reads HA, HB, HC GPIO pins — each from its own configured port — and packs
 * them into a 3-bit value: bit0 = HA, bit1 = HB, bit2 = HC.
 * Sets flagNewEdge TRUE for one ISR cycle after a state transition.
 * Sets flagHallFault TRUE if state is 0 (000) or 7 (111).
 * On new edge: re-arms ECAP input and updates the ECAP-based speed estimate
 * with a first-order low-pass filter.
 * Always updates the electrical angle via HAL_Hall_computeAngle_inline().
 *
 * \param[in]    configHandle  Hall configuration handle
 * \param[inout] dataHandle    Hall data handle
 */
__attribute__((always_inline)) static inline void HAL_Hall_readState(const HAL_Hall_Config_t *configHandle,
                                                                     HAL_Hall_Data_t         *dataHandle)
{
    uint8_t state = 0U;

    if (DL_GPIO_readPins((GPIO_Regs *)configHandle->hallAGpioPort, configHandle->hallPinA) != 0U)
    {
        state |= 0x01U;
    }

    if (DL_GPIO_readPins((GPIO_Regs *)configHandle->hallBGpioPort, configHandle->hallPinB) != 0U)
    {
        state |= 0x02U;
    }

    if (DL_GPIO_readPins((GPIO_Regs *)configHandle->hallCGpioPort, configHandle->hallPinC) != 0U)
    {
        state |= 0x04U;
    }

    /* FRL port (2026-10-17): accept a new Hall state only after FRL_HALL_DEBOUNCE_TICKS identical reads. On the
     * DRV8376EVM with the BLY171S, single raw reads chattered (2,305 edges with one sector of net travel in TI's Hall
     * calibration), and the calibration counted each glitch as a found state. Three ticks at the 20 kHz ISR is 150 us,
     * under 1 percent of a Hall sector at the speeds commissioned here. The first read is accepted at once. */
#ifndef FRL_HALL_DEBOUNCE_TICKS
#define FRL_HALL_DEBOUNCE_TICKS 3U
#endif
    if (state == dataHandle->hallCandidate)
    {
        if (dataHandle->hallStableCnt < 255U) dataHandle->hallStableCnt++;
    }
    else
    {
        dataHandle->hallCandidate = state;
        dataHandle->hallStableCnt = 1U;
    }
    if ((dataHandle->hallState != 0U) && (dataHandle->hallStableCnt < FRL_HALL_DEBOUNCE_TICKS))
    {
        state = dataHandle->hallState; /* not yet confirmed: hold the last accepted state */
    }
    dataHandle->hallStatePrev = dataHandle->hallState;
    dataHandle->hallState     = state;
    dataHandle->flagNewEdge   = (state != dataHandle->hallStatePrev) ? true : false;
    dataHandle->flagHallFault = ((state == 0U) || (state == 7U)) ? true : false;

    /* On every Hall state transition: re-arm ECAP and update speed estimate */
    if (dataHandle->flagNewEdge == true)
    {
        HAL_Hall_updateEcapInput_inline(configHandle, dataHandle);

        dataHandle->noEdgeIsrCnt = 0U;

        /* ECAP-based speed: read capture period (counter resets on each capture event,
         * so the timestamp holds the period between the last two Hall edges). */
        dataHandle->capturePeriod_cnt = DL_ECAP_getEventTimeStamp((ECAP_Regs *)configHandle->ecapBase,
                                                                  (DL_ECAP_EVENT)configHandle->ecapCaptureEvent);

        /* FRL port (2026-10-17): reject a capture faster than the motor can turn. ECAP times the raw Hall line, which the
         * debounce above does not cover, so a glitch inside the debounce window reads as a 150 us interval (1,111 Hz on a
         * motor at 20 Hz); with the speed filter gain near 1 that spike reached TI's speed loop and, in the 2026-10-17
         * campaign, preceded hardware over-current faults in 11 of 14 holds at 20 Hz. Keep the last good speed instead. */
#ifndef FRL_HALL_MAX_SPEED_HZ
#define FRL_HALL_MAX_SPEED_HZ 320.0f /* about 1.2 x the BLY171S rated 266.7 Hz electrical */
#endif
        if ((dataHandle->capturePeriod_cnt > 0U) &&
            ((float32_t)dataHandle->capturePeriod_cnt * 6.0f * FRL_HALL_MAX_SPEED_HZ <
             configHandle->sysclkFreq_MHz * 1000000.0f))
        {
            dataHandle->capturePeriod_cnt = 0U; /* implausible: leave speedHall_raw_Hz at its last good value */
        }
        if (dataHandle->capturePeriod_cnt > 0U)
        {
            /* Speed magnitude in Hz = sysclk_Hz / (capturePeriod_cnt * 6)
             * Factor 6: 6 Hall transitions per electrical revolution.
             * Raw speed is always positive (ECAP period is unsigned). */
            dataHandle->speedHall_raw_Hz =
                (configHandle->sysclkFreq_MHz * 1000000.0f) / ((float32_t)dataHandle->capturePeriod_cnt * 6.0f);
        }

        /* First-order low-pass filter on signed speed.
         * Apply direction sign here so speedHall_Hz is negative in reverse.
         * speedHall_raw_Hz remains unsigned (magnitude) for stale-speed decay.
         * speedFilter_rps is read from dataHandle (dynamic, updated by low-priority
         * state machine to 2 * |speedRef_Hz| * 2*pi for adaptive bandwidth). */
        float32_t speedHall_signed_Hz = dataHandle->direction * dataHandle->speedHall_raw_Hz;
        float32_t alpha               = dataHandle->speedFilter_rps * configHandle->isrPeriod_usec * 1e-6f;
        dataHandle->speedHall_Hz = dataHandle->speedHall_Hz + alpha * (speedHall_signed_Hz - dataHandle->speedHall_Hz);
    }
    else
    {
        /* No Hall transition — accumulate stale counter */
        if (dataHandle->noEdgeIsrCnt < UINT32_MAX)
        {
            dataHandle->noEdgeIsrCnt++;
        }

        /* Stale-speed scaling:
         * Compute expected ISR cycles between Hall edges at the current filtered
         * speed magnitude.  Once noEdgeIsrCnt exceeds that threshold (motor is
         * slower than the current estimate), scale speedHall_raw_Hz down
         * proportionally and re-apply direction sign:
         *
         *   speedHall_raw_Hz = (expectedCount / noEdgeIsrCnt) * |speedHall_Hz|
         *
         * The ratio (expectedCount / noEdgeIsrCnt) < 1 and shrinks every ISR
         * cycle, so the LPF progressively pulls speedHall_Hz toward zero.
         * fabsf() is used for magnitude comparisons to handle reverse direction. */
        float32_t speedHall_abs_Hz = fabsf(dataHandle->speedHall_Hz);

        if (speedHall_abs_Hz > 0.0f)
        {
            /* expectedCount [ISR cycles] = 1 / (|speedHall_Hz| * 6 * isrPeriod_s) */
            float32_t expectedCount = 1.0f / (speedHall_abs_Hz * 6.0f * configHandle->isrPeriod_usec * 1.0e-6f);

            if ((float32_t)dataHandle->noEdgeIsrCnt >= expectedCount)
            {
                /* Scale down raw speed magnitude, then re-apply direction sign */
                dataHandle->speedHall_raw_Hz = (expectedCount / (float32_t)dataHandle->noEdgeIsrCnt) * speedHall_abs_Hz;

                float32_t speedHall_signed_Hz = dataHandle->direction * dataHandle->speedHall_raw_Hz;
                float32_t alpha               = dataHandle->speedFilter_rps * configHandle->isrPeriod_usec * 1.0e-6f;
                dataHandle->speedHall_Hz =
                    dataHandle->speedHall_Hz + alpha * (speedHall_signed_Hz - dataHandle->speedHall_Hz);
            }
        }
        else
        {
            /* Filtered speed at zero — ensure raw is also zeroed */
            dataHandle->speedHall_raw_Hz = 0.0f;
        }
    }

    HAL_Hall_computeAngle_inline(configHandle, dataHandle);
}

/* Non-ISR function declarations — bodies in hal_hall_sen_interface.c */

/**
 * \brief Initialize Hall sensor interface
 *
 * Resets all Hall data fields to safe initial values and starts the
 * ECAP peripheral for speed capture. ECAP is the sole speed measurement
 * method; configHandle->ecapBase must be non-zero.
 *
 * \param[in]    configHandle  Hall configuration handle (ecapBase must be non-zero)
 * \param[inout] dataHandle    Hall data handle
 */
extern void HAL_Hall_Init(const HAL_Hall_Config_t *configHandle, HAL_Hall_Data_t *dataHandle);

/**
 * \brief Initialize ECAP for Hall speed capture
 *
 * Starts the ECAP counter and enables timestamp capture.
 * ECAP peripheral is pre-configured by SysConfig (HALL_SPEED_CAP instance).
 *
 * \param[in]    configHandle  Hall configuration handle
 * \param[inout] dataHandle    Hall data handle
 */
extern void HAL_Hall_initSpeedCapture(const HAL_Hall_Config_t *configHandle, HAL_Hall_Data_t *dataHandle);

/**
 * \brief Reset Hall sensor data to safe initial state
 *
 * Zero-initializes all fields in the Hall data object.
 * Sets direction to 1.0f (forward) as a safe power-on default.
 *
 * \param[inout] dataHandle    Hall data handle
 */
extern void HAL_Hall_reset(HAL_Hall_Data_t *dataHandle);

/**
 * \brief Update ECAP input mux for the next Hall edge (non-ISR wrapper)
 *
 * Thin non-inline wrapper around HAL_Hall_updateEcapInput_inline().
 * Intended for use outside the ISR hot-path (e.g. forced update at init time
 * after direction and hallState are known).
 *
 * \param[in] configHandle  Hall configuration handle
 * \param[in] dataHandle    Hall data handle (hallState and direction must be current)
 */
extern void HAL_Hall_updateEcapInput(const HAL_Hall_Config_t *configHandle, const HAL_Hall_Data_t *dataHandle);

#ifdef __cplusplus
}
#endif

#endif /* HAL_HALL_SEN_INTERFACE_H_ */

/** @} */
