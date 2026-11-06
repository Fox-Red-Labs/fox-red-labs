/*****************************************************************************/
/* Copyright (c) 2026 Texas Instruments Incorporated                         */
/* http://www.ti.com/                                                        */
/*                                                                           */
/*  Redistribution and  use in source  and binary forms, with  or without    */
/*  modification,  are permitted provided  that the  following conditions    */
/*  are met:                                                                 */
/*                                                                           */
/*     Redistributions  of source  code must  retain the  above copyright    */
/*     notice, this list of conditions and the following disclaimer.         */
/*                                                                           */
/*     Redistributions in binary form  must reproduce the above copyright    */
/*     notice, this  list of conditions  and the following  disclaimer in    */
/*     the  documentation  and/or   other  materials  provided  with  the    */
/*     distribution.                                                         */
/*                                                                           */
/*     Neither the  name of Texas Instruments Incorporated  nor the names    */
/*     of its  contributors may  be used to  endorse or  promote products    */
/*     derived  from   this  software  without   specific  prior  written    */
/*     permission.                                                           */
/*                                                                           */
/*  THIS SOFTWARE  IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS    */
/*  "AS IS"  AND ANY  EXPRESS OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT    */
/*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR    */
/*  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT    */
/*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    */
/*  SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES  (INCLUDING, BUT  NOT    */
/*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,    */
/*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    */
/*  THEORY OF  LIABILITY, WHETHER IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE    */
/*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.     */
/*                                                                           */
/*****************************************************************************/
/*
 * csi_presence.h - on-device Wi-Fi CSI presence detection (empty / presence)
 *
 * Pipeline:
 *   per CSI packet:  csiDataBuf[27] -> 52 subcarrier amplitudes, buffered with
 *                    its real (irregular) timestamp
 *   once ~PRES_WIN_US (2 s) of raw samples have arrived (async task):
 *     interpolate      : resample onto a uniform PRES_FS-Hz grid of
 *                         PRES_WIN_PHYS (256) points using two-point linear
 *                         interpolation
 *     odd-decimate     : keep the odd-indexed grid points (1,3,...,255) ->
 *                         PRES_WIN_MODEL (128) rows, matching the offline
 *                         preprocessing
 *     l2_normalize     : per-row (per-time-step) L2 norm across subcarriers
 *     rfft_time        : arm_rfft_fast_f32(128) -> 64 bins, log10(1+|F|)
 *     rfft_subcarriers : RFFT(52) -> 26 bins, log10(1+|F|)
 *     TVM              : (1,26,1,64) -> 2 logits -> softmax -> presence prob
 *     soft avg         : rolling mean over last PRES_MAJ_K windows -> decision
 *
 * The feature-extraction math must match the offline training preprocessing.
 * Raw samples are timestamped and interpolated to tolerate irregular packet
 * arrival.
 *
 * Call csi_pres_push() once per CSI packet. The result is reported over DAP
 * (see csi_presence.c), not returned directly.
 */
#ifndef CSI_PRESENCE_H
#define CSI_PRESENCE_H

#include <stdint.h>
#include "wlan_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Geometry (must match the offline preprocessing) */
#define PRES_N_SUB_RAW    54          /* subcarriers decoded from 27 words (2 per word) */
#define PRES_N_SUB        52          /* used (drop DC at index 26 and pilot at index 53) */
#define PRES_DC_IDX0      26
#define PRES_DC_IDX1      53
#define PRES_FS           128u        /* Hz, interpolated grid rate */
#define PRES_WIN_PHYS     256u        /* interpolated grid points per window (2 s at PRES_FS) */
#define PRES_WIN_MODEL    128u        /* rows fed to the model after odd-decimation */
#define PRES_WIN_US       2000000u    /* 2 s window (PRES_WIN_PHYS / PRES_FS), in packet-timestamp units (us) */
#define PRES_SESSION_GAP_US 500000u   /* CSI packet gap (us) that discards the partial window */
#define PRES_RAW_CAP      300u        /* raw accumulator capacity; headroom over ~2 s at ~130-140 pkts/s */
#define PRES_FFT_T        128u        /* rfft_time input length */
#define PRES_MODEL_T      64u         /* time bins after rfft_time (drop Nyquist) */
#define PRES_MODEL_SC     26u         /* subcarrier bins after rfft_subcarriers */
#define PRES_SCALE        1.0f        /* log multiplier for the subcarrier-axis log-FFT; must equal FE_LOG_MUL in user_input_config.h */

/* Post-processing */
#define PRES_MAJ_K       3u           /* soft-average window (windows) */
#define PRES_THRESH_AVG  0.5f         /* soft-average threshold -> final decision */

/* Result of the most recently completed window. */
typedef struct {
    int      cls;         /* 0 = empty, 1 = presence */
    float    p1;          /* P(presence) from softmax */
    float    pavg;        /* soft average of p1 over last PRES_MAJ_K windows */
    int      decision;    /* final: pavg >= PRES_THRESH_AVG */
    float    pps;         /* packets per second over the window */
    uint32_t win_index;   /* running window counter */
} pres_result_t;

/* Feed one CSI packet. Accumulates data; signals the inference task when the
 * window is full. Returns 0 always -- the result is reported over DAP. */
int  csi_pres_push(const WlanGetCSIData_t *p);

/* Initialize the async inference task. Call once, after DAP_open(). */
void csi_pres_init_task(void);

/* Reset the accumulation buffer (drops partial window). */
void csi_pres_reset(void);

/* Copy the last completed result into *out. Returns 1 if a result exists. */
int  csi_pres_get_last(pres_result_t *out);

/* Human-readable label for a class index. */
const char *csi_pres_label(int cls);

#ifdef __cplusplus
}
#endif

#endif /* CSI_PRESENCE_H */
