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
 * \file hall_cal.h
 * \brief Hall sensor bidirectional calibration state machine.
 *
 * \defgroup HALL_CAL_MODULE Hall Sensor Calibration
 * \brief Dither-sweep calibration producing separate forward and reverse
 *        electrical angle tables for Hall-based FOC.
 *
 * ## Why Two Tables Are Required
 *
 * A Hall sensor fires at the leading edge of the magnetic pole in the
 * direction of travel.  When the rotor reverses, the same sensor fires at
 * the trailing edge — which is one full 60° sector away.  Using a single
 * table for both directions introduces a systematic 60° error in one
 * direction, causing severe torque loss.
 *
 * ## Calibration Sequence
 *
 * 1. **ALIGN**: Lock rotor at 0° with d-axis current.
 * 2. **FWD sweep**: Increment angle 0° → 360°×polePairs×mechCycles in
 *    `stepSize_rad` steps.  At each step apply dither (+91°/−90°) to settle
 *    the rotor, then accumulate the sweep angle into `calAngleFwdSum_rad[s]`
 *    whenever Hall state `s` is entered.
 * 3. **REV_ALIGN**: Re-lock rotor at 0° before reversing.
 * 4. **REV sweep**: Decrement angle.  Accumulate into `calAngleRevSum_rad[s]`.
 * 5. **AVERAGE**: Divide each sum by `numMechCycles × numPolePairs` to get
 *    the mean transition angle per state per direction.
 * 6. **DONE**: `calAngleFwd_rad[8]` and `calAngleRev_rad[8]` are valid.
 *
 * ## No Raw Sample Buffer
 *
 * Because the number of transitions per state is deterministic
 * (`numMechCycles × numPolePairs`), the divisor is known at init time.
 * Angles are accumulated directly into per-state sums — no raw buffer needed.
 * Total RAM for `HallCal_Data_t` is ~160 bytes.
 *
 * @{
 */

#ifndef HALL_CAL_H_
#define HALL_CAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_obj.h"
#include "math_types.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * \anchor HALL_CAL_CONSTANTS
 * \name Calibration constants
 * @{
 */

/** \brief Number of Hall state indices (0–7; valid states 1–6) */
#define HALL_CAL_STATE_COUNT (8U)

/** \brief Nominal 60° sector width in radians */
#define HALL_CAL_SECTOR_RAD ((float32_t)(MATH_PI / 3.0))

/** \brief Tolerance on sector width for sanity check (±20° in radians) */
#define HALL_CAL_SECTOR_TOL_RAD ((float32_t)(MATH_PI / 9.0))

/** \brief 45° jump angle for state-triggered optimization (safe margin before ~60° transition) */
#define HALL_CAL_JUMP_ANGLE_RAD ((float32_t)(MATH_PI / 4.0))

/** \brief Wrap angle to [-π, π) range (safe for both sweep and FOC output) */
#define HALL_CAL_WRAP_ANGLE(angle_rad)  \
    do                                  \
    {                                   \
        while ((angle_rad) >= MATH_PI)  \
            (angle_rad) -= MATH_TWO_PI; \
        while ((angle_rad) < -MATH_PI)  \
            (angle_rad) += MATH_TWO_PI; \
    } while (0)

/** @} */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * \brief Hall calibration state machine states
 *
 * States >= HALL_CAL_ALIGN mean calibration is active and the normal FOC
 * path must be bypassed.
 */
typedef enum
{
    HALL_CAL_IDLE           = 0U, /**< Not running; normal FOC operation */
    HALL_CAL_ALIGN          = 1U, /**< Initial align to 0° before FWD sweep */
    HALL_CAL_FWD_DITHER_FWD = 2U, /**< FWD sweep: apply +ditherFwdAngle_rad */
    HALL_CAL_FWD_DITHER_REV = 3U, /**< FWD sweep: apply -ditherRevAngle_rad */
    HALL_CAL_FWD_SAMPLE     = 4U, /**< FWD sweep: sample Hall state */
    HALL_CAL_REV_ALIGN      = 5U, /**< Re-align to 0° before REV sweep */
    HALL_CAL_REV_DITHER_FWD = 6U, /**< REV sweep: apply +ditherFwdAngle_rad */
    HALL_CAL_REV_DITHER_REV = 7U, /**< REV sweep: apply -ditherRevAngle_rad */
    HALL_CAL_REV_SAMPLE     = 8U, /**< REV sweep: sample Hall state */
    HALL_CAL_AVERAGE        = 9U, /**< Background: compute averaged tables */
    HALL_CAL_DONE           = 10U /**< Complete; tables valid */
} HallCal_State_e;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * \brief Hall calibration configuration (read-only, placed in flash)
 *
 * Timing fields are in milliseconds.  `HallCal_init()` converts them to ISR
 * ticks at runtime using `isrPeriod_usec`.
 */
typedef struct _HallCal_Config_t_
{
    float32_t alignCurrent_A;     /**< \brief D-axis align current in Amps */
    float32_t calCurrent_A;       /**< \brief D-axis current during sweep in Amps */
    float32_t stepSize_rad;       /**< \brief Angle increment per step (rad), e.g. π/180 = 1° */
    float32_t ditherFwdAngle_rad; /**< \brief Forward dither offset (rad), e.g. 91° = π/2 + π/180 */
    float32_t ditherRevAngle_rad; /**< \brief Reverse dither offset (rad), e.g. 90° = π/2 */
    float32_t alignHoldTime_ms;   /**< \brief Align hold duration in milliseconds (e.g. 50.0f) */
    float32_t ditherHoldTime_ms;  /**< \brief Dither half-cycle hold in milliseconds (e.g. 5.0f) */
    uint16_t  numMechCycles;      /**< \brief Full mechanical cycles per sweep direction */
} HallCal_Config_t;

/**
 * \brief Hall calibration runtime data (mutable, placed in RAM)
 *
 * Total size ~160 bytes.  No raw sample buffer — angles are accumulated
 * directly into per-state sums and divided by the known divisor
 * (`numMechCycles × numPolePairs`) in `HallCal_average()`.
 */
typedef struct _HallCal_Data_t_
{
    HallCal_State_e state; /**< \brief Current calibration state */

    /**
     * \brief Current sweep target angle (rad), wrapped to [0, 2π).
     * Incremented in FWD sweep, decremented in REV sweep.
     */
    float32_t sweepAngle_rad;

    uint8_t  hallStatePrev;   /**< \brief Hall state at the previous step */
    uint32_t timerTicks;      /**< \brief General-purpose hold timer (ISR ticks) */
    uint32_t alignHoldTicks;  /**< \brief Align hold in ISR ticks (computed at init) */
    uint32_t ditherHoldTicks; /**< \brief Dither hold in ISR ticks (computed at init) */

    /**
     * \brief Running angle sums for forward rotation (indexed by Hall state).
     *
     * Accumulated in HALL_CAL_FWD_SAMPLE: whenever Hall state `s` is entered,
     * `sweepAngle_rad` is added to `calAngleFwdSum_rad[s]`.
     * Divided by `numMechCycles × numPolePairs` in `HallCal_average()`.
     */
    float32_t calAngleFwdSum_rad[HALL_CAL_STATE_COUNT];

    /**
     * \brief Running angle sums for reverse rotation (indexed by Hall state).
     *
     * Accumulated in HALL_CAL_REV_SAMPLE.
     */
    float32_t calAngleRevSum_rad[HALL_CAL_STATE_COUNT];

    /**
     * \brief Calibrated angle table for forward rotation (indexed by Hall state).
     *
     * Valid after `state == HALL_CAL_DONE` and `flagCalValid == true`.
     * Drop-in replacement for `HAL_Hall_AngleTable_rad`.
     */
    float32_t calAngleFwd_rad[HALL_CAL_STATE_COUNT];

    /**
     * \brief Calibrated angle table for reverse rotation (indexed by Hall state).
     *
     * Valid after `state == HALL_CAL_DONE` and `flagCalValid == true`.
     */
    float32_t calAngleRev_rad[HALL_CAL_STATE_COUNT];

    /* FRL port (2026-10-19, guard revision H4): circular averaging. The SDK summed the wrapped sweep angles and divided,
     * so a transition near +/-180 degrees (one pole pair at 179, the next at -179) averaged to about 0. Each entry now
     * keeps its first sample as a reference and sums the wrapped differences from it; the mean is reference + sum / count.
     * The counts and the spread (largest minus smallest difference) are kept for the checks in HallCal_validate and the
     * guard (frl_guard.c, hall_table_check). */
    float32_t calAngleFwdRef_rad[HALL_CAL_STATE_COUNT]; /**< \brief first FWD sample per state */
    float32_t calAngleRevRef_rad[HALL_CAL_STATE_COUNT]; /**< \brief first REV sample per state */
    float32_t calDevFwdMin_rad[HALL_CAL_STATE_COUNT];   /**< \brief smallest FWD sample minus its reference */
    float32_t calDevFwdMax_rad[HALL_CAL_STATE_COUNT];   /**< \brief largest FWD sample minus its reference */
    float32_t calDevRevMin_rad[HALL_CAL_STATE_COUNT];
    float32_t calDevRevMax_rad[HALL_CAL_STATE_COUNT];
    uint8_t   calCountFwd[HALL_CAL_STATE_COUNT];        /**< \brief FWD samples per state (numMechCycles x numPolePairs expected) */
    uint8_t   calCountRev[HALL_CAL_STATE_COUNT];

    bool flagCalDone;  /**< \brief TRUE when averaging is complete */
    bool flagCalValid; /**< \brief TRUE if result passes sanity checks */

    /**
     * \brief Number of states still required (countdown from 6 to 0).
     *
     * Initialized to 6 at start of each sweep direction (ALIGN/REV_ALIGN).
     * Decremented by 1 on each Hall state transition during FWD_SAMPLE/REV_SAMPLE.
     * When reaches 0, all 6 valid states have been detected — calibration
     * for that direction is complete. Deterministic, integer-based completion.
     */
    uint8_t statesRequired;

    /**
     * \brief Number of states found so far (diagnostic counter).
     *
     * Tracks progress: incremented on each transition, compared to expected 6.
     * Helps distinguish between "found N states" and "need M more states".
     * Resets at start of each direction.
     */
    uint8_t statesFound;
} HallCal_Data_t;

/* ============================================================================
 * ISR hot-path inline function
 * ============================================================================ */

/**
 * \brief Run one ISR tick of the Hall calibration state machine
 *
 * Call from the FOC ISR instead of the normal FOC path while
 * `motorState == MOTOR_HALL_CAL`.  Writes `angleFOC_rad`, `Idq_out_A.value[0]`
 * (d-axis) and `Idq_out_A.value[1]` (q-axis) directly.
 *
 * On Hall state transitions in FWD_SAMPLE / REV_SAMPLE, the current
 * `sweepAngle_rad` is accumulated directly into `calAngleFwdSum_rad[s]` or
 * `calAngleRevSum_rad[s]` — no raw buffer is used.
 *
 * \param[in]    configHandle  Calibration configuration handle
 * \param[inout] dataHandle    Calibration runtime data handle
 * \param[in]    hallState     Current 3-bit Hall state (from HAL_Hall_readState)
 * \param[out]   pAngleFOC_rad Pointer to obj->outVars.angleFOC_rad
 * \param[out]   pIdRef_A      Pointer to obj->outVars.Idq_out_A.value[0] (d-axis)
 * \param[out]   pIqRef_A      Pointer to obj->outVars.Idq_out_A.value[1] (q-axis)
 */
/* FRL port (2026-10-19): one sample into a per-state circular accumulator (see calAngleFwdRef_rad) */
__attribute__((always_inline)) static inline void HallCal_accumulate_inline(float32_t *pRef, float32_t *pSum, float32_t *pMin,
                                                                            float32_t *pMax, uint8_t *pCnt, float32_t angle_rad)
{
    float32_t d = 0.0f;
    if (*pCnt == 0U)
    {
        *pRef = angle_rad;
        *pSum = 0.0f;
        *pMin = 0.0f;
        *pMax = 0.0f;
    }
    else
    {
        d = angle_rad - *pRef;
        HALL_CAL_WRAP_ANGLE(d);
        if (d < *pMin) { *pMin = d; }
        if (d > *pMax) { *pMax = d; }
    }
    *pSum += d;
    if (*pCnt < 255U) { (*pCnt)++; }
}

__attribute__((always_inline)) static inline void HallCal_run_inline(const HallCal_Config_t *configHandle,
                                                                     HallCal_Data_t *dataHandle, uint8_t hallState,
                                                                     volatile float32_t *pAngleFOC_rad,
                                                                     float32_t *pIdRef_A, float32_t *pIqRef_A)
{
    switch (dataHandle->state)
    {
        /* ------------------------------------------------------------------ */
        case HALL_CAL_ALIGN:
        case HALL_CAL_REV_ALIGN:
        {
            *pAngleFOC_rad = 0.0f;
            *pIdRef_A      = configHandle->alignCurrent_A;
            *pIqRef_A      = 0.0f;

            dataHandle->timerTicks++;
            if (dataHandle->timerTicks >= dataHandle->alignHoldTicks)
            {
                dataHandle->timerTicks    = 0U;
                dataHandle->hallStatePrev = hallState;

                if (dataHandle->state == HALL_CAL_ALIGN)
                {
                    dataHandle->sweepAngle_rad = 0.0f;
                    dataHandle->state          = HALL_CAL_FWD_DITHER_FWD;
                }
                else
                {
                    /* REV_ALIGN complete — reset state counter and start reverse sweep from 0° */
                    dataHandle->sweepAngle_rad = 0.0f;
                    dataHandle->statesFound    = 0U;
                    dataHandle->state          = HALL_CAL_REV_DITHER_FWD;
                }
            }
            else
            {
                /* still aligning */
            }
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_FWD_DITHER_FWD:
        case HALL_CAL_REV_DITHER_FWD:
        {
            /* Apply forward dither offset to sweep angle */
            float32_t ditherAngle = dataHandle->sweepAngle_rad + configHandle->ditherFwdAngle_rad;

            /* Ensure dithered angle stays in [-π, π) range before output */
            HALL_CAL_WRAP_ANGLE(ditherAngle);

            *pAngleFOC_rad = ditherAngle;
            *pIdRef_A      = configHandle->calCurrent_A;
            *pIqRef_A      = 0.0f;

            dataHandle->timerTicks++;
            if (dataHandle->timerTicks >= dataHandle->ditherHoldTicks)
            {
                dataHandle->timerTicks = 0U;
                dataHandle->state =
                    (dataHandle->state == HALL_CAL_FWD_DITHER_FWD) ? HALL_CAL_FWD_DITHER_REV : HALL_CAL_REV_DITHER_REV;
            }
            else
            {
                /* still holding forward dither */
            }
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_FWD_DITHER_REV:
        case HALL_CAL_REV_DITHER_REV:
        {
            /* Dither REVERSE: subtract ditherRevAngle to bracket backward from sweep angle */
            float32_t ditherAngle = dataHandle->sweepAngle_rad - configHandle->ditherRevAngle_rad;

            /* Ensure dithered angle stays in [-π, π) range before output */
            HALL_CAL_WRAP_ANGLE(ditherAngle);

            *pAngleFOC_rad = ditherAngle;
            *pIdRef_A      = configHandle->calCurrent_A;
            *pIqRef_A      = 0.0f;

            dataHandle->timerTicks++;
            if (dataHandle->timerTicks >= dataHandle->ditherHoldTicks)
            {
                dataHandle->timerTicks = 0U;
                dataHandle->state =
                    (dataHandle->state == HALL_CAL_FWD_DITHER_REV) ? HALL_CAL_FWD_SAMPLE : HALL_CAL_REV_SAMPLE;
            }
            else
            {
                /* still holding reverse dither */
            }
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_FWD_SAMPLE:
        {
            /* Output sweep angle to FOC controller (always wrapped to [-π, π)) */
            *pAngleFOC_rad = dataHandle->sweepAngle_rad;
            *pIdRef_A      = configHandle->calCurrent_A;
            *pIqRef_A      = 0.0f;

            /* Detect Hall state transition */
            if ((hallState != dataHandle->hallStatePrev) && (hallState >= 1U) && (hallState <= 6U))
            {
                /* Accumulate angle for this state */
                /* FRL port: circular accumulation (SDK: calAngleFwdSum_rad[hallState] += sweepAngle_rad) */
                HallCal_accumulate_inline(&dataHandle->calAngleFwdRef_rad[hallState], &dataHandle->calAngleFwdSum_rad[hallState],
                                          &dataHandle->calDevFwdMin_rad[hallState], &dataHandle->calDevFwdMax_rad[hallState],
                                          &dataHandle->calCountFwd[hallState], dataHandle->sweepAngle_rad);
                dataHandle->statesFound++;

                /* Check if all 6 states have been found */
                if (dataHandle->statesFound >= dataHandle->statesRequired)
                {
                    /* FWD sweep complete — re-align before REV sweep */
                    dataHandle->timerTicks  = 0U;
                    dataHandle->statesFound = 0U;
                    dataHandle->state       = HALL_CAL_REV_ALIGN;
                    break;
                }

                /* STATE-TRIGGERED OPTIMIZATION:
                 * Jump 45° ahead on every state detection to approximate next transition.
                 * Safe margin: 60° spacing - 45° jump = 15° before next transition.
                 * Reduces calibration time by ~85%.
                 */
                dataHandle->sweepAngle_rad += HALL_CAL_JUMP_ANGLE_RAD;

                /* Ensure sweep angle stays in [-π, π) range after jump */
                HALL_CAL_WRAP_ANGLE(dataHandle->sweepAngle_rad);

                dataHandle->timerTicks    = 0U;
                dataHandle->hallStatePrev = hallState;
                dataHandle->state         = HALL_CAL_FWD_DITHER_FWD;
                break;
            }

            dataHandle->hallStatePrev = hallState;

            /* No state change — continue normal sweep with 1° steps */
            dataHandle->sweepAngle_rad += configHandle->stepSize_rad;

            /* Ensure sweep angle stays in [-π, π) range after increment */
            HALL_CAL_WRAP_ANGLE(dataHandle->sweepAngle_rad);

            dataHandle->state = HALL_CAL_FWD_DITHER_FWD;
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_REV_SAMPLE:
        {
            /* Output sweep angle to FOC controller (always wrapped to [-π, π)) */
            *pAngleFOC_rad = dataHandle->sweepAngle_rad;
            *pIdRef_A      = configHandle->calCurrent_A;
            *pIqRef_A      = 0.0f;

            /* Detect Hall state transition */
            if ((hallState != dataHandle->hallStatePrev) && (hallState >= 1U) && (hallState <= 6U))
            {
                /* Accumulate angle for this state */
                /* FRL port: circular accumulation (SDK: calAngleRevSum_rad[hallState] += sweepAngle_rad) */
                HallCal_accumulate_inline(&dataHandle->calAngleRevRef_rad[hallState], &dataHandle->calAngleRevSum_rad[hallState],
                                          &dataHandle->calDevRevMin_rad[hallState], &dataHandle->calDevRevMax_rad[hallState],
                                          &dataHandle->calCountRev[hallState], dataHandle->sweepAngle_rad);
                dataHandle->statesFound++;

                /* Check if all 6 states have been found for all cycles*/
                if (dataHandle->statesFound >= dataHandle->statesRequired)
                {
                    /* REV sweep complete — signal background to average */
                    dataHandle->statesFound = 0U;
                    dataHandle->state       = HALL_CAL_AVERAGE;
                    break;
                }

                /* STATE-TRIGGERED OPTIMIZATION:
                 * Jump -45° backward on every state detection to approximate next transition.
                 * Safe margin: 60° spacing - 45° jump = 15° before next transition.
                 * Reduces calibration time by ~85%.
                 */
                dataHandle->sweepAngle_rad -= HALL_CAL_JUMP_ANGLE_RAD;

                /* Ensure sweep angle stays in [-π, π) range after jump */
                HALL_CAL_WRAP_ANGLE(dataHandle->sweepAngle_rad);

                dataHandle->timerTicks    = 0U;
                dataHandle->hallStatePrev = hallState;
                dataHandle->state         = HALL_CAL_REV_DITHER_FWD;
                break;
            }

            dataHandle->hallStatePrev = hallState;

            /* No state change — continue normal sweep with 1° steps */
            dataHandle->sweepAngle_rad -= configHandle->stepSize_rad;

            /* Ensure sweep angle stays in [-π, π) range after decrement */
            HALL_CAL_WRAP_ANGLE(dataHandle->sweepAngle_rad);

            dataHandle->state = HALL_CAL_REV_DITHER_FWD;
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_AVERAGE:
        {
            /* Hold position; background loop calls HallCal_average() */
            float32_t outputAngle = dataHandle->sweepAngle_rad;

            /* Ensure output angle is in [-π, π) range */
            HALL_CAL_WRAP_ANGLE(outputAngle);

            *pAngleFOC_rad = outputAngle;
            *pIdRef_A      = configHandle->calCurrent_A;
            *pIqRef_A      = 0.0f;
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_DONE:
        {
            float32_t outputAngle = dataHandle->sweepAngle_rad;

            /* Ensure output angle is in [-π, π) range */
            HALL_CAL_WRAP_ANGLE(outputAngle);

            *pAngleFOC_rad = outputAngle;
            *pIdRef_A      = 0.0f;
            *pIqRef_A      = 0.0f;
            break;
        }

        /* ------------------------------------------------------------------ */
        case HALL_CAL_IDLE:
        default:
        {
            *pIdRef_A = 0.0f;
            *pIqRef_A = 0.0f;
            break;
        }
    }
}

/* ============================================================================
 * Non-ISR function declarations — bodies in hall_cal.c
 * ============================================================================ */

/**
 * \brief Initialize the Hall calibration data object and start the sequence
 *
 * Resets all fields, converts ms timing to ISR ticks using `isrPeriod_usec`,
 * computes `sweepCountdown_rad`, and sets `state = HALL_CAL_ALIGN`.
 *
 * \param[in]    configHandle    Calibration configuration handle
 * \param[inout] dataHandle      Calibration runtime data handle
 * \param[in]    isrPeriod_usec  ISR execution period in microseconds
 */
extern void HallCal_init(const HallCal_Config_t *configHandle, HallCal_Data_t *dataHandle, float32_t isrPeriod_usec);

/**
 * \brief Reset calibration data to safe initial state
 *
 * Clears all accumulators and flags.  Sets `state = HALL_CAL_IDLE`.
 *
 * \param[inout] dataHandle  Calibration runtime data handle
 */
extern void HallCal_reset(HallCal_Data_t *dataHandle);

/**
 * \brief Compute averaged calibration tables from accumulated sums
 *
 * Divides each per-state sum by `numMechCycles × numPolePairs` to produce
 * the mean transition angle.  Sets `flagCalDone = true` and
 * `state = HALL_CAL_DONE`.  Must be called from the background loop when
 * `state == HALL_CAL_AVERAGE`.
 *
 * \param[inout] dataHandle    Calibration runtime data handle
 * \param[in]    configHandle  Calibration configuration handle (for divisor)
 */
extern void HallCal_average(HallCal_Data_t *dataHandle, const HallCal_Config_t *configHandle);

/**
 * \brief Validate both calibrated angle tables
 *
 * Checks that adjacent transition angles (in forward sequence order) are
 * within `HALL_CAL_SECTOR_RAD ± HALL_CAL_SECTOR_TOL_RAD` for both FWD and
 * REV tables.  Sets `flagCalValid = true` only if both pass.
 * Must be called after `HallCal_average()`.
 *
 * \param[inout] dataHandle  Calibration runtime data handle
 */
extern void HallCal_validate(HallCal_Data_t *dataHandle);

#ifdef __cplusplus
}
#endif

#endif /* HALL_CAL_H_ */

/** @} */
