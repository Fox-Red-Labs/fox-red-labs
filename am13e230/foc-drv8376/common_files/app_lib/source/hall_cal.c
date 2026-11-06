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
 */

/**
 * \file hall_cal.c
 * \brief Hall sensor calibration — non-ISR function bodies.
 *
 * Direct-accumulation design: no raw sample buffer.
 * Angles are accumulated into per-state sums during the sweep and divided
 * by the known divisor (numMechCycles × numPolePairs) in HallCal_average().
 * The ISR hot-path (HallCal_run_inline) lives in hall_cal.h.
 */

#include "hall_cal.h"

/* ============================================================================
 * Private — forward Hall state sequence for sector-order validation
 * Forward sequence: 1->3->2->6->4->5->1
 * ============================================================================ */

/** \brief Forward Hall state sequence (6 valid states) */
static const uint8_t HallCal_FwdSeq_tbl[6U] = {1U, 3U, 2U, 6U, 4U, 5U};

/* ============================================================================
 * Private helper — validate one angle table
 * ============================================================================ */

/*
 * Checks adjacent sector spacing for one table.
 * Returns true if all checks pass.
 */
static bool HallCal_validateTable(const float32_t *angleTable_rad)
{
    uint8_t   seqIdx;
    uint8_t   stateA;
    uint8_t   stateB;
    float32_t angleDiff_rad;
    float32_t sectorMin_rad;
    float32_t sectorMax_rad;
    bool      valid;

    valid         = true;
    sectorMin_rad = HALL_CAL_SECTOR_RAD - HALL_CAL_SECTOR_TOL_RAD;
    sectorMax_rad = HALL_CAL_SECTOR_RAD + HALL_CAL_SECTOR_TOL_RAD;

    for (seqIdx = 0U; seqIdx < 6U; seqIdx++)
    {
        stateA = HallCal_FwdSeq_tbl[seqIdx];
        stateB = HallCal_FwdSeq_tbl[(seqIdx + 1U) % 6U];

        angleDiff_rad = angleTable_rad[stateB] - angleTable_rad[stateA];

        /* Wrap to (-π, π] to handle the 330°→30° wrap-around */
        if (angleDiff_rad > MATH_PI)
        {
            angleDiff_rad -= MATH_TWO_PI;
        }
        else if (angleDiff_rad < -MATH_PI)
        {
            angleDiff_rad += MATH_TWO_PI;
        }
        else
        {
            /* already in range */
        }

        /* Absolute value */
        if (angleDiff_rad < 0.0f)
        {
            angleDiff_rad = -angleDiff_rad;
        }
        else
        {
            /* already positive */
        }

        if ((angleDiff_rad < sectorMin_rad) || (angleDiff_rad > sectorMax_rad))
        {
            valid = false;
        }
        else
        {
            /* spacing within tolerance */
        }
    }

    return valid;
}

/* ============================================================================
 * HallCal_reset
 * ============================================================================ */

/*
 * \brief Reset calibration data to safe initial state
 *
 * Clears all accumulators and flags.  Sets state = HALL_CAL_IDLE.
 *
 * \param[inout] dataHandle  Calibration runtime data handle
 */
void HallCal_reset(HallCal_Data_t *dataHandle)
{
    uint8_t idx;

    dataHandle->state           = HALL_CAL_IDLE;
    dataHandle->sweepAngle_rad  = 0.0f;
    dataHandle->hallStatePrev   = 0U;
    dataHandle->timerTicks      = 0U;
    dataHandle->alignHoldTicks  = 0U;
    dataHandle->ditherHoldTicks = 0U;
    dataHandle->flagCalDone     = false;
    dataHandle->flagCalValid    = false;
    dataHandle->statesFound     = 0U;

    for (idx = 0U; idx < (uint8_t)HALL_CAL_STATE_COUNT; idx++)
    {
        dataHandle->calAngleFwdSum_rad[idx] = 0.0f;
        dataHandle->calAngleRevSum_rad[idx] = 0.0f;
        dataHandle->calAngleFwd_rad[idx]    = 0.0f;
        dataHandle->calAngleRev_rad[idx]    = 0.0f;
        dataHandle->calAngleFwdRef_rad[idx] = 0.0f; /* FRL port: circular accumulators (hall_cal.h) */
        dataHandle->calAngleRevRef_rad[idx] = 0.0f;
        dataHandle->calDevFwdMin_rad[idx]   = 0.0f;
        dataHandle->calDevFwdMax_rad[idx]   = 0.0f;
        dataHandle->calDevRevMin_rad[idx]   = 0.0f;
        dataHandle->calDevRevMax_rad[idx]   = 0.0f;
        dataHandle->calCountFwd[idx]        = 0U;
        dataHandle->calCountRev[idx]        = 0U;
    }
}

/* ============================================================================
 * HallCal_init
 * ============================================================================ */

/*
 * \brief Initialize the Hall calibration data object and start the sequence
 *
 * Resets all fields, converts ms timing to ISR ticks, computes
 * sweepCountdown_rad, and sets state = HALL_CAL_ALIGN.
 *
 * \param[in]    configHandle    Calibration configuration handle
 * \param[inout] dataHandle      Calibration runtime data handle
 * \param[in]    isrPeriod_usec  ISR execution period in microseconds
 */
void HallCal_init(const HallCal_Config_t *configHandle, HallCal_Data_t *dataHandle, float32_t isrPeriod_usec)
{
    HallCal_reset(dataHandle);

    /* Convert millisecond timing to ISR ticks.
     * isrPeriod_usec is in microseconds; 1 ms = 1000 µs.
     * Add 0.5f before truncation to round to nearest tick. */
    dataHandle->alignHoldTicks  = (uint32_t)((configHandle->alignHoldTime_ms * 1000.0f) / isrPeriod_usec + 0.5f);
    dataHandle->ditherHoldTicks = (uint32_t)((configHandle->ditherHoldTime_ms * 1000.0f) / isrPeriod_usec + 0.5f);

    dataHandle->sweepAngle_rad = 0.0f;
    dataHandle->statesFound    = 0U;
    dataHandle->state          = HALL_CAL_ALIGN;
}

/* ============================================================================
 * HallCal_average
 * ============================================================================ */

/*
 * \brief Compute averaged calibration tables from accumulated sums
 *
 * Divides each per-state sum by numMechCycles × numPolePairs to produce
 * the mean transition angle for FWD and REV tables.
 * Sets flagCalDone = true and state = HALL_CAL_DONE.
 * Must be called from the background loop when state == HALL_CAL_AVERAGE.
 *
 * \param[inout] dataHandle    Calibration runtime data handle
 * \param[in]    configHandle  Calibration configuration handle (for divisor)
 */
void HallCal_average(HallCal_Data_t *dataHandle, const HallCal_Config_t *configHandle)
{
    uint8_t stateIdx;

    if (dataHandle->state != HALL_CAL_AVERAGE)
    {
        /* Not ready — do nothing */
        return;
    }

    /* FRL port (2026-10-19): circular mean, reference + mean difference, divided by the samples actually taken (the SDK
     * divided the linear sum by statesRequired / 6, which is wrong for an entry near +/-180 degrees). An entry with no
     * sample stays 0 and fails HallCal_validate's count check. */
    for (stateIdx = 1U; stateIdx <= 6U; stateIdx++)
    {
        float32_t a;
        a = 0.0f;
        if (dataHandle->calCountFwd[stateIdx] != 0U)
        {
            a = dataHandle->calAngleFwdRef_rad[stateIdx] +
                dataHandle->calAngleFwdSum_rad[stateIdx] / (float32_t)dataHandle->calCountFwd[stateIdx];
            HALL_CAL_WRAP_ANGLE(a);
        }
        dataHandle->calAngleFwd_rad[stateIdx] = a;
        a = 0.0f;
        if (dataHandle->calCountRev[stateIdx] != 0U)
        {
            a = dataHandle->calAngleRevRef_rad[stateIdx] +
                dataHandle->calAngleRevSum_rad[stateIdx] / (float32_t)dataHandle->calCountRev[stateIdx];
            HALL_CAL_WRAP_ANGLE(a);
        }
        dataHandle->calAngleRev_rad[stateIdx] = a;
    }
}

/* ============================================================================
 * HallCal_validate
 * ============================================================================ */

/*
 * \brief Validate both calibrated angle tables
 *
 * Checks that adjacent transition angles (in forward sequence order) are
 * within HALL_CAL_SECTOR_RAD ± HALL_CAL_SECTOR_TOL_RAD for both FWD and REV.
 * Sets flagCalValid = true only if both tables pass.
 * Must be called after HallCal_average().
 *
 * \param[inout] dataHandle  Calibration runtime data handle
 */
void HallCal_validate(HallCal_Data_t *dataHandle)
{
    bool fwdValid;
    bool revValid;

    if (dataHandle->flagCalDone == false)
    {
        dataHandle->flagCalValid = false;
        return;
    }

    fwdValid = HallCal_validateTable(dataHandle->calAngleFwd_rad);
    revValid = HallCal_validateTable(dataHandle->calAngleRev_rad);

    /* FRL port (2026-10-19): every entry must hold exactly statesRequired / 6 samples (numMechCycles x numPolePairs), the
     * deterministic count the SDK's averaging assumed but never checked */
    {
        uint8_t   s;
        uint16_t  want = (uint16_t)(dataHandle->statesRequired / 6U);
        for (s = 1U; s <= 6U; s++)
        {
            if ((dataHandle->calCountFwd[s] != want) || (dataHandle->calCountRev[s] != want))
            {
                fwdValid = false;
            }
        }
    }

    dataHandle->flagCalValid = (fwdValid && revValid) ? true : false;
}
