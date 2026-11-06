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
 * csi_presence.c - on-device Wi-Fi CSI presence detection (empty / presence).
 *
 * Pipeline:
 *   extract_csi -> buffer 2 s of raw, irregularly timestamped packets ->
 *   interpolate_into_model_input (uniform 256-point / 2 s grid, odd-decimated
 *   to 128 rows, time-major) -> transpose to variable-major -> FE_l2NormRow ->
 *   per-subcarrier FE_fft + FE_log (time axis) -> FE_fftCol + FE_log
 *   (subcarrier axis) -> TVM (26x64, no transpose) -> softmax -> soft
 *   average -> result reported over DAP.
 *
 * The feature-extraction math must match the offline training preprocessing
 * exactly. All stages (L2 norm, both FFT axes, both log steps) come from the
 * shared fe_v2 library (source/ti/ai/edge_ai/fe_v2, staged into
 * feature_extraction/core at export time). Its per-stage functions
 * (FE_l2NormRow / FE_fft / FE_fftCol / FE_log) are called directly instead of
 * through FE_runFeatureExtract, because that entry point depends on fe_v2's
 * compile-time configuration and history buffer, whereas this pipeline feeds
 * the stages from its own window buffers. The log multiplier PRES_SCALE
 * (1.0) equals FE_LOG_MUL in user_input_config.h.
 */
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "csi_presence.h"
#include "tvmgen_default.h"
#include "uart_term.h"
#include "feature_extract.h"

/* FreeRTOS: async inference task */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Device Agent Protocol (DAP): reports inference results to the host */
#include <ti/ai/edge_ai/dap/DAP.h>

/* lwIP sockets: used for a minimal UDP keepalive that stops the AP from
 * disconnecting the station for inactivity (reason code 6). There is
 * otherwise no outgoing Wi-Fi traffic. */
#include "lwip/sockets.h"

/* CMSIS-DSP: arm_sub_f32, arm_scale_f32, arm_add_f32 */
#include "arm_math.h"

/* fe_v2 per-stage building blocks (feature_extract.c). These are not in
 * feature_extract.h's public API, so they are declared here for direct use. */
void FE_l2NormRow(float *input_buffer, uint16_t num_vars, uint16_t num_time);
void FE_fft(float *fft_input_buffer, float *fft_output_buffer, uint16_t frame_size, uint16_t fft_stages);
void FE_fftCol(float *assembled, float *output_buffer, uint16_t num_vars, uint16_t num_time_bins, uint16_t num_bins);
void FE_log(float *input_buffer, uint16_t feature_size_per_frame, float log_mul, float log_threshold, float log_base);

/* Memory placement (must match linker.cmd) */
#ifdef CC35XX
/* .bss.presence is placed in TCM_DRAM_NON_SECURE: large noinit scratch buffers */
#define PRES_TCM __attribute__((section(".bss.presence")))
#else
#define PRES_TCM
#endif

/* Window accumulator: buffers (timestamp, mag[52]) pairs and interpolates onto
 * a uniform grid at window close, since packets do not arrive evenly spaced. */

static uint32_t s_raw_ts_us[PRES_RAW_CAP];
static float    s_raw_mag[PRES_RAW_CAP][PRES_N_SUB];
static uint32_t s_raw_head   = 0u;   /* valid raw rows currently buffered */
static uint32_t s_close_head = 0u;   /* raw row count captured at window close */

/* Timing */
static uint32_t s_win_t0_us = 0u;   /* timestamp of first packet in window (us) */
static float    s_win_pps   = 0.0f; /* packets per second, computed at window close */

/* Last result */
static pres_result_t s_last = { 0, 0.0f, 0.0f, 0, 0.0f, 0u };

/* Wi-Fi keepalive: UDP socket opened once in csi_pres_init_task(); -1 means
 * not ready. A 1-byte broadcast datagram is sent each inference cycle to keep
 * the AP's station table entry alive (avoids WLAN_EVENT_DISCONNECT reason 6). */
static int s_ka_sock = -1;

/* Async inference */

static SemaphoreHandle_t s_infer_sem  = NULL;
static volatile int      s_infer_busy = 0;

/* Intermediate buffers (static to avoid stack pressure) */

/* Interpolated model input rows: PRES_WIN_MODEL x PRES_N_SUB (128 x 52 float),
 * time-major (one row per interpolated time step). */
PRES_TCM static float s_dec[PRES_WIN_MODEL][PRES_N_SUB];

/* s_dec transposed to variable-major, as FE_l2NormRow and the per-channel
 * FFT loop expect: PRES_N_SUB x PRES_WIN_MODEL (52 x 128 float) */
PRES_TCM static float s_dec_T[PRES_N_SUB][PRES_WIN_MODEL];

/* Scratch for FE_fft: FE_FRAME_SIZE*4 floats (time-domain input plus
 * interleaved complex FFT output), per feature_extract.h's sizing convention. */
PRES_TCM static float s_fe_scratch[PRES_FFT_T * 4];

/* Per-subcarrier time-axis FFT+log output, variable-major, as FE_fftCol
 * expects: PRES_N_SUB x PRES_MODEL_T (52 x 64 float) */
PRES_TCM static float s_time_feat[PRES_N_SUB][PRES_MODEL_T];

/* FE_fftCol output: PRES_MODEL_SC x PRES_MODEL_T (26 x 64 float), freq-bin-major */
PRES_TCM static float s_freq_feat[PRES_MODEL_SC][PRES_MODEL_T];

/* TVM model input: (1, 26, 1, 64), NCHW with freq bins as channels */
PRES_TCM static float s_model_in[1][PRES_MODEL_SC][1][PRES_MODEL_T];

/* TVM output: (1, 2) */
static float s_tvm_out[2];

/* Post-processing state */

static float    s_soft_buf[PRES_MAJ_K];
static uint32_t s_soft_i = 0u;
static uint32_t s_soft_n = 0u;

/* DAP wire format for one inference result. Matches inferences[] in
 * dap/DAP_interface.c: {"inference_result", INT8}, {"p1_probability", FLOAT32} */
typedef struct __attribute__((packed))
{
    int8_t inferred_class;   /* "inference_result": class selector, not plotted */
    float  p1_probability;   /* "p1_probability": P(presence) */
} Pres_InferenceResult;

/* Decode 27 CSI words into 54 subcarrier amplitudes and drop the DC (index 26)
 * and pilot (index 53) subcarriers, leaving 52 values. */
static void extract_csi(const WlanGetCSIData_t *p, float out52[PRES_N_SUB])
{
    float raw54[PRES_N_SUB_RAW];
    int   n = 0, w;

    for (w = 0; w < 27; w++)
    {
        uint32_t word = p->csiDataBuf[w];
        int8_t im0 = (int8_t)((word >>  0) & 0xFF);
        int8_t re0 = (int8_t)((word >>  8) & 0xFF);
        int8_t im1 = (int8_t)((word >> 16) & 0xFF);
        int8_t re1 = (int8_t)((word >> 24) & 0xFF);
        raw54[n++] = sqrtf((float)re0*re0 + (float)im0*im0);
        raw54[n++] = sqrtf((float)re1*re1 + (float)im1*im1);
    }

    int j = 0, i;
    for (i = 0; i < (int)PRES_N_SUB_RAW; i++)
    {
        if (i == PRES_DC_IDX0 || i == PRES_DC_IDX1) continue;
        out52[j++] = raw54[i];
    }
}

/* Two-point linear interpolation across the 52-wide magnitude vector:
 * out = a + frac * (b - a). */
static void lerp52(const float *a, const float *b, float frac, float *out)
{
    float diff[PRES_N_SUB];
    arm_sub_f32(b, a, diff, PRES_N_SUB);
    arm_scale_f32(diff, frac, diff, PRES_N_SUB);
    arm_add_f32(a, diff, out, PRES_N_SUB);
}

/* Resample the buffered raw window (s_close_head rows at their real packet
 * arrival times) onto a uniform PRES_FS-Hz grid of PRES_WIN_PHYS (256) points
 * over the 2 s window, then keep only the odd-indexed grid points
 * (1,3,...,255 -> PRES_WIN_MODEL = 128 rows), matching the offline
 * preprocessing. Writes s_dec, [PRES_WIN_MODEL][PRES_N_SUB] (time-major);
 * it is transposed to variable-major later, where the FE stages need it. */
static void interpolate_into_model_input(void)
{
    const uint32_t n     = s_close_head;
    const uint32_t t0_us = s_win_t0_us;
    uint32_t j = 0u;   /* sweep pointer: largest raw idx with ts <= tq */

    if (n < 2u)
    {
        /* Not enough raw samples to interpolate (e.g. a session boundary at
         * window close): zero-fill so downstream stages get a defined input. */
        memset(s_dec, 0, sizeof(s_dec));
        return;
    }

    /* i sweeps the 256-point / 2 s grid, but only odd i (1,3,...,255) are
     * written, at model row k = (i-1)/2 = 0..127. Even points are skipped
     * (odd-interleave decimation). */
    for (uint32_t i = 1u; i < PRES_WIN_PHYS; i += 2u)
    {
        /* grid point in integer microseconds, rounded division */
        uint32_t tq_us = t0_us + (uint32_t)(((uint64_t)i * 1000000ull + PRES_FS / 2u) / PRES_FS);
        uint32_t k     = (i - 1u) / 2u;

        while ((j + 1u) < n && s_raw_ts_us[j + 1u] <= tq_us) j++;

        uint32_t a = j;
        uint32_t b = ((j + 1u) < n) ? (j + 1u) : j;   /* clamp at the tail */

        uint32_t dt_ab = s_raw_ts_us[b] - s_raw_ts_us[a];
        if (a == b || dt_ab == 0u)
        {
            memcpy(s_dec[k], s_raw_mag[a], sizeof(s_dec[k]));
        }
        else
        {
            uint32_t dt_aq = tq_us - s_raw_ts_us[a];
            float    frac  = (float)dt_aq / (float)dt_ab;
            lerp52(s_raw_mag[a], s_raw_mag[b], frac, s_dec[k]);
        }
    }
}

/* Inference and post-processing */

static inline float softmax_p1(float z0, float z1)
{
    float m  = (z0 > z1) ? z0 : z1;
    float e0 = expf(z0 - m);
    float e1 = expf(z1 - m);
    return e1 / (e0 + e1);
}

static float update_soft_avg(float p1)
{
    if (p1 < 0.0f) p1 = 0.0f;
    if (p1 > 1.0f) p1 = 1.0f;
    s_soft_buf[s_soft_i] = p1;
    s_soft_i = (s_soft_i + 1u) % PRES_MAJ_K;
    if (s_soft_n < PRES_MAJ_K) s_soft_n++;
    float sum = 0.0f;
    for (uint32_t i = 0; i < s_soft_n; i++) sum += s_soft_buf[i];
    return sum / (float)s_soft_n;
}

static struct tvmgen_default_inputs  s_tvm_in   = { .onnx__Add_0 = (void *)s_model_in };
static struct tvmgen_default_outputs s_tvm_outs = { .output = (void *)s_tvm_out };

static void run_inference_and_send(void)
{
    /* Resample the raw window onto the uniform grid the feature extraction
     * expects. */
    interpolate_into_model_input();

    /* Feature extraction; must match the offline training preprocessing
     * (L2 norm -> time-axis FFT + log -> subcarrier-axis FFT + log). The fe_v2
     * stages are called directly; see the file header for why. */

    /* FE_l2NormRow and the per-channel FFT loop expect variable-major input:
     * [PRES_N_SUB][PRES_WIN_MODEL]. */
    for (uint32_t t = 0; t < PRES_WIN_MODEL; t++)
        for (uint32_t sc = 0; sc < PRES_N_SUB; sc++)
            s_dec_T[sc][t] = s_dec[t][sc];

    /* Row-wise L2 norm per time step over subcarriers: A[i,:] /= ||A[i,:]||_2 */
    FE_l2NormRow(&s_dec_T[0][0], PRES_N_SUB, PRES_WIN_MODEL);

    /* Time-axis FFT + log, per subcarrier: 128 -> 64 bins, log10(1 + |F|)
     * (real FFT, Nyquist bin dropped). */
    for (uint32_t sc = 0; sc < PRES_N_SUB; sc++)
    {
        memcpy(s_fe_scratch, s_dec_T[sc], PRES_FFT_T * sizeof(float));
        FE_fft(s_fe_scratch, s_fe_scratch + (PRES_FFT_T * 2), PRES_FFT_T, 7);
        FE_log(s_fe_scratch, PRES_MODEL_T, 1.0f, 1.0f, 10.0f);
        memcpy(s_time_feat[sc], s_fe_scratch, PRES_MODEL_T * sizeof(float));
    }

    /* Subcarrier-axis FFT + log: 52 -> 26 bins, log10(1 + |F|) * PRES_SCALE
     * (real FFT, Nyquist bin dropped). */
    FE_fftCol(&s_time_feat[0][0], &s_freq_feat[0][0],
              PRES_N_SUB, PRES_MODEL_T, PRES_MODEL_SC);
    FE_log(&s_freq_feat[0][0], (uint16_t)(PRES_MODEL_SC * PRES_MODEL_T),
           PRES_SCALE, 1.0f, 10.0f);

    /* FE_fftCol output is freq-bin-major [PRES_MODEL_SC][PRES_MODEL_T], which
     * already matches the (1,26,1,64) model input; no transpose needed. */
    memcpy(s_model_in, s_freq_feat, sizeof(s_model_in));

    /* TVM inference */
    s_tvm_out[0] = 0.0f; s_tvm_out[1] = 0.0f;
    tvmgen_default_run(&s_tvm_in, &s_tvm_outs);

    /* Post-processing: soft average */
    float p1       = softmax_p1(s_tvm_out[0], s_tvm_out[1]);
    float pavg     = update_soft_avg(p1);
    int   decision = (pavg >= PRES_THRESH_AVG) ? 1 : 0;

    s_last.cls      = decision;
    s_last.p1       = p1;
    s_last.pavg     = pavg;
    s_last.decision = decision;
    s_last.pps      = s_win_pps;
    s_last.win_index++;

    /* Report over DAP regardless of the selected pipeline mode (Live Preview
     * does not necessarily select DAP_PIPELINE_MODE_SENSOR_INFERENCING). Send
     * only once the host has started streaming; before that DAP answers each
     * call with a "Not ready for streaming yet" error frame. */
    if (DAP_isReadyToStartStreaming())
    {
        Pres_InferenceResult result;
        result.inferred_class = (int8_t)decision;
        result.p1_probability = DAP_swapEndianness(p1);
        DAP_sendData(DAP_SEND_DATA_TYPE_INFERENCING_VALUE, &result, sizeof(result));
    }

    /* Wi-Fi keepalive: 1-byte UDP broadcast so the AP sees outgoing traffic
     * and does not expire the station entry (reason 6 disconnect). */
    if (s_ka_sock >= 0)
    {
        struct sockaddr_in bcast;
        uint8_t ka = 0u;
        bcast.sin_family = AF_INET;
        bcast.sin_port = htons(9002);
        bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        int ret = lwip_sendto(s_ka_sock, &ka, 1, 0,
                    (struct sockaddr *)&bcast, sizeof(bcast));
        if (ret < 0)
        {
            lwip_close(s_ka_sock);
            s_ka_sock = -1;

            /* Recreate the socket */
            s_ka_sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
            if (s_ka_sock >= 0)
            {
                int bc = 1;
                lwip_setsockopt(s_ka_sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

                struct sockaddr_in local;
                memset(&local, 0, sizeof(local));
                local.sin_family = AF_INET;
                local.sin_port = htons(0);
                local.sin_addr.s_addr = htonl(INADDR_ANY);
                lwip_bind(s_ka_sock, (struct sockaddr *)&local, sizeof(local));
            }
        }
    }
}

/* Inference task */

static void pres_inference_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        xSemaphoreTake(s_infer_sem, portMAX_DELAY);
        run_inference_and_send();
        s_infer_busy = 0;
    }
}

/* Public API */

void csi_pres_init_task(void)
{
    /* Open the UDP keepalive socket once. Broadcast is enabled so packets can
     * go to 255.255.255.255 without knowing the AP's IP address. */
    s_ka_sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (s_ka_sock >= 0)
    {
        int bc = 1;
        lwip_setsockopt(s_ka_sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

        /* Bind to any address on an ephemeral port */
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(0);
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        lwip_bind(s_ka_sock, (struct sockaddr *)&local, sizeof(local));

        UART_PRINT("[CSI] Keepalive socket: fd=%d\r\n", s_ka_sock);
    }
    else
    {
        UART_PRINT("[CSI] ERROR: Keepalive socket creation failed\r\n");
    }

    s_infer_sem = xSemaphoreCreateBinary();
    xTaskCreate(pres_inference_task, "pres_infer", 2048, NULL, 1, NULL);
}

void csi_pres_reset(void)
{
    s_raw_head   = 0u;
    s_close_head = 0u;
    s_win_t0_us  = 0u;
    s_win_pps    = 0.0f;
    s_infer_busy = 0;
}

int csi_pres_get_last(pres_result_t *out)
{
    if (out) *out = s_last;
    return (s_last.win_index > 0u) ? 1 : 0;
}

const char *csi_pres_label(int cls)
{
    return (cls == 1) ? "presence" : "empty";
}

int csi_pres_push(const WlanGetCSIData_t *p)
{
    if (s_infer_busy) return 0;

    float mags[PRES_N_SUB];
    extract_csi(p, mags);

    uint32_t ts_us = p->timestamp;

    if (s_raw_head > 0u)
    {
        int32_t dt = (int32_t)(ts_us - s_raw_ts_us[s_raw_head - 1u]);
        if (dt < 0 || dt > (int32_t)PRES_SESSION_GAP_US)
        {
            /* Session boundary (backwards time or gap > PRES_SESSION_GAP_US):
             * drop the partial window and start fresh. */
            s_raw_head = 0u;
        }
    }

    if (s_raw_head == 0u)
    {
        s_win_t0_us = ts_us;
    }

    if (s_raw_head < PRES_RAW_CAP)
    {
        s_raw_ts_us[s_raw_head] = ts_us;
        memcpy(s_raw_mag[s_raw_head], mags, sizeof(mags));
        s_raw_head++;
    }

    uint32_t elapsed = ts_us - s_win_t0_us;
    if (((elapsed >= PRES_WIN_US && s_raw_head >= 2u) || s_raw_head >= PRES_RAW_CAP) && s_infer_sem)
    {
        s_close_head = s_raw_head;
        s_win_pps    = (s_raw_head >= 2u && elapsed > 0u)
                       ? ((float)s_raw_head * 1e6f / (float)elapsed) : 0.0f;
        s_infer_busy = 1;
        s_raw_head   = 0u;
        xSemaphoreGive(s_infer_sem);
    }
    return 0;
}
