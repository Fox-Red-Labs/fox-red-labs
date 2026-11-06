/*
 * Copyright (c) 2026, Texas Instruments Incorporated
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
 * @file main.c
 * @brief Wakeword Detection Application using NPU AI on MSPM0G5187
 *
 * This application implements real-time wakeword (keyword) detection using:
 * - Audio input from SPI microphone or ADC-based microphone
 * - Feature extraction using LPC (Linear Predictive Coding) or Filter Bank
 * - Neural network inference using TI's Neural Processing Unit (NPU)
 * - LED indication when wakeword is detected
 *
 * @note The application runs in a continuous loop, sampling audio, extracting
 *       features, and performing inference to detect the target wakeword.
 */

#include "ti_msp_dl_config.h"
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>
#include "npu.h"
#include "post_inference.h"
#include "feature_extraction.h"
#include "data_collection.h"
#include "common.h"
#include "model/tvmgen_default.h"

/* Configuration-specific definitions based on feature extraction method */
#include "lpc_float/lpc_defs.h"

#define REUSE_DEPTH (4)

/* ---- Fox Red Labs additions: serial report, detection events, hold LED (marked FRL) ---- */
#include "frl_report.h"

/* LPC coefficients buffer */
extern int16_t lpc[LPC_BANKS];

/* Input array for neural network (stores multiple frames for reuse) */
int16_t inputArray[REUSE_DEPTH][LPC_BANKS] = {0};

/* LPC initialization and computation functions */
extern void init_lpc();
extern void get_lpc(int16_t *buffer);

/* Neural network output buffer [batch_size][num_classes] */
int8_t output[1][2] = {0, 0};


/* Neural network input/output structures */
struct tvmgen_default_inputs inputs = {(void *)&audio_features[0]};
struct tvmgen_default_outputs outputs = {(void *)&output};

#if INPUT_ADC
/* Global counter for SysTick timer rollovers */
uint32_t systick_rollOver_counter = 0;

/**
 * @brief Initialize SysTick timer for system timing
 *
 * Configures SysTick to roll over after counting down 8M cycles,
 * enabling the interrupt and starting the timer.
 */
SYSCONFIG_WEAK void SYSCFG_DL_SYSTICK_init(void)
{
    DL_SYSTICK_config(8000000);
}

/**
 * @brief SysTick interrupt handler
 *
 * Increments a counter each time the SysTick timer rolls over.
 * Can be used for timekeeping or timeout operations.
 */
void SysTick_Handler(void)
{
    systick_rollOver_counter++;
}
#endif

/**
 * @brief Main application entry point
 *
 * Initializes peripherals and runs the main processing loop:
 * 1. Continuously samples audio from microphone
 * 2. Extracts audio features when enough samples are collected
 * 3. Runs neural network inference on extracted features
 * 4. Processes inference results to detect wakeword
 *
 * @return Never returns (infinite loop)
 */
int main(void)
{
    /* Initialize SPI microphone or BOOSTXL audio microphone peripherals */
    mic_init();

#if INPUT_ADC
    /* Initialize SysTick timer for system timing */
    SYSCFG_DL_SYSTICK_init();
#endif
    frl_init();

    /* Reset and power up the Neural Processing Unit (NPU) */
    DL_NPU_reset(NPU);
    DL_NPU_enablePower(NPU);

    /* Wait for NPU to be ready */
    while (!(DL_SYSCTL_getStatus() & DL_SYSCTL_STATUS_NPU_READY))
        ;

    /* Configure NPU interrupts */
    DL_NPU_clearInterruptStatus(NPU, DL_NPU_INTERRUPT_DONE);
    DL_NPU_enableInterrupt(NPU, DL_NPU_INTERRUPT_DONE);
    NVIC_EnableIRQ(NPU_INT_IRQn);

    /* Initialize Linear Predictive Coding (LPC) module */
    init_lpc();


    /* Delay to allow system stabilization */
    delay_cycles(5000000);

#if INPUT_ADC
    /* Start timer to begin audio sampling */
    DL_TimerG_startCounter(TIMER_0_INST);
#endif

#if INPUT_ADC
    /* Configure low-power mode (STANDBY0) */
    DL_SYSCTL_setPowerPolicySTANDBY0();
#endif
    frl_ready();

    /**
     * Main processing loop:
     * - Collects audio samples via interrupt-driven sampling
     * - Extracts features when sufficient samples are available
     * - Runs inference when new features are ready
     * - Processes inference results for wakeword detection
     */
#if INPUT_ADC
    /* the shipped loop, unchanged apart from the FRL hooks */
    while (1)
    {
        frl_poll();  /* FRL: replay input and hold LED */
        /* Check if enough audio samples have been collected */
        if (samples_count >= NUM_AUDIO_SAMPLES)
        {
            /* Extract features using Linear Predictive Coding */
            frl_l0();  /* FRL */
            extract_features_lpc();
            frl_l1();  /* FRL */


            /* Increment count of newly extracted features */
            new_features_extracted++;

            /* Reset samples count (subtract to avoid losing overflow samples) */
            samples_count -= NUM_AUDIO_SAMPLES;
        }

        /* Check if new features are ready for inference */
        if (new_features_extracted == 1)
        {
            /* Run neural network inference on the NPU */
            frl_t0();  /* FRL */
            (void)tvmgen_default_run(&inputs, &outputs);

            /* Wait for inference to complete */
            while (!tvmgen_default_finished)
                ;
            frl_t1();  /* FRL */

            /* Process inference output for wakeword detection */
            post_inference_ww(output);
            frl_after_inference(output[0][0], output[0][1]);  /* FRL */

            /* Reset feature extraction counter */
            new_features_extracted = 0;
        }
    }
#else
    /**
     * FRL: pipelined loop for the microphone builds. The shipped loop does LPC (8.6 ms) and then the NPU
     * (11.2 ms) one after the other and takes 24.4 ms per 20 ms frame at 80 MHz, so it falls behind a
     * real-time microphone. Here the NPU starts on the window as it stands (frames up to the previous one)
     * and the CPU computes the new frame's LPC features while it runs; the new frame joins the window
     * when the NPU is done. Same model, same features, same window, one frame (20 ms) later.
     */
    while (1)
    {
        frl_poll();
        if (samples_count >= NUM_AUDIO_SAMPLES)
        {
            frl_t0();
            (void)tvmgen_default_run(&inputs, &outputs);     /* returns at once; the NPU runs */
            frl_l0();
            frl_lpc_only();                                  /* LPC of the new frame into lpc[] */
            frl_l1();
            samples_count -= NUM_AUDIO_SAMPLES;
            while (!tvmgen_default_finished)
                ;
            frl_t1();
            post_inference_ww(output);
            frl_after_inference(output[0][0], output[0][1]);
            frl_window_add();                                /* new frame joins the window */
        }
    }
#endif
}
