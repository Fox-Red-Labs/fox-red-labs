/*
 *  Created on: Nov 20, 2025
 */
// #############################################################################
//
//  FILE:   feature_extract.h
//
//!
//! This contains prototyping of user interactive API functions
//!
//
// #############################################################################
//
/*
 * Copyright (c) 2024-2026, Texas Instruments Incorporated
 * 
 *  All rights reserved not granted herein.
 *  Limited License.
 * 
 *  Texas Instruments Incorporated grants a world-wide, royalty-free,
 *  non-exclusive license under copyrights and patents it now or hereafter
 *  owns or controls to make, have made, use, import, offer to sell and sell
 *  ("Utilize") this software subject to the terms herein. With respect to the
 *  foregoing patent license, such license is granted solely to the extent that
 *  any such patent is necessary to Utilize the software alone. The patent
 *  license shall not apply to any combinations which include this software,
 *  other than combinations with devices manufactured by or for TI ("TI
 *  Devices"). No hardware patent is licensed hereunder.
 * 
 *  Redistributions must preserve existing copyright notices and reproduce
 *  this license (including the above copyright notice and the disclaimer and
 *  (if applicable) source code license limitations below) in the documentation
 *  and/or other materials provided with the distribution.
 * 
 *  Redistribution and use in binary form, without modification, are permitted
 *  provided that the following conditions are met:
 * 
 *    * No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *    * Any redistribution and use are licensed by TI for use only with TI Devices.
 *    * Nothing shall obligate TI to provide you with source code for the software
 *      licensed and provided to you in object code.
 * 
 *  If software source code is provided to you, modification and redistribution
 *  of the source code are permitted provided that the following conditions are
 *  met:
 * 
 *    * Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *    * Any redistribution and use of any object code compiled from the source
 *      code and any resulting derivative works, are licensed by TI for use
 *      only with TI Devices.
 * 
 *  Neither the name of Texas Instruments Incorporated nor the names of its
 *  suppliers may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 * 
 *  DISCLAIMER.
 * 
 *  THIS SOFTWARE IS PROVIDED BY TI AND TI'S LICENSORS "AS IS" AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL TI AND TI'S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 *  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// #############################################################################

/**
 * \addtogroup ai_fe_group AI - Feature Extract APIs
 * 
 * @{
 */

/**
 * \file feature_extract.h
  * 
  * \brief
  * Header file contains enumerations, structure definitions and functions
  * declarations for Edge AI Feature Extract Module interface
  */

#ifndef FEATURE_EXTRACT_H_
#define FEATURE_EXTRACT_H_

// Peripheral includes
#include "math.h"
#if defined(__TMS320C2000__) || defined(__C29__)
#include "driverlib.h"
#else
#include "stdint.h"
#include "stdbool.h"
#endif

#include "user_input_config.h"

#if defined(FE_FFT_COL) && !defined(FE_MODEL_SC)
#define FE_MODEL_SC (FE_VARIABLES / 2)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Model Input
 * 
 * Model Input type
 */
#if defined(SKIP_NORMALIZE)
typedef int8_t model_input_t;
#else
typedef float model_input_t;
#endif

/**
 * \brief Model Output
 * 
 * Model Output type
 */
#if defined(OUTPUT_INT)
typedef int8_t model_output_t;
#else
typedef float model_output_t;
#endif

/**
 * \struct feature_extraction
 * \brief Feature extraction context structure
 * 
 * Contains buffers and configuration for feature extraction operations
 */
typedef struct {
    void* input_buffer;                 /**< Pointer to input buffer address on which feature extraction will be performed */
    void* output_buffer;                /**< Pointer to output buffer address to store extracted features */
    void* history_buffer;               /**< Pointer to history buffer address if need to initialize history */
    void* scratch_buffer;               /**< Pointer to scratch buffer address for computation */
    bool test_feature_extraction;       /**< Toggle to test feature extraction from test vector */
    int32_t size_required_by_library;   /**< Size required by FE Library (used by scratch buffer) */
} feature_extraction;

typedef feature_extraction* feature_extraction_handle;

/**
 * \brief Calculates required memory allocation for feature extraction
 * 
 * \param fe Feature extraction handle
 * 
 * Sets the required memory size for feature extraction operations
 * Size is calculated as 2x frame size in float precision
 * \return None
 * 
 */
void FE_allocFeatureExtract(feature_extraction_handle fe);

/**
 * \brief Initializes feature extraction state and buffers
 * 
 * \param fe Feature extraction handle
 * 
 * Initializes history buffer and output buffer for feature extraction
 * Handles both normalized and quantized output modes
 * Processes each channel separately for multi-channel data
 * \return None
 * 
 */
void FE_initFeatureExtract(feature_extraction_handle fe);

/**
 * \brief Main feature extraction processing function
 * 
 * \param fe Feature extraction handle containing all buffers and state
 * 
 * Performs complete feature extraction pipeline including:
 * - Raw signal processing (optional mean removal)
 * - Windowing (optional)
 * - FFT computation (optional)
 * - FFT normalization (optional)
 * - DC removal (optional)
 * - Frequency binning (optional)
 * - Log scaling (optional)
 * - Frame concatenation (optional)
 * - Batch normalization or direct copy (based on configuration)
 * 
 * Processes each channel independently and supports both normalized
 * float output and quantized int8_t output modes
 * \return None
 * 
 */
void FE_runFeatureExtract(feature_extraction_handle fe);

/**
 * \brief Compares feature extraction buffers
 * 
 * \param expected_array Expected output from feature extraction
 * \param obtained_array Obtained output from feature extraction
 * 
 * Takes the difference of the two arrays and returns the number of
 * elements that were not matched.
 * \return number of mismatches in two array
 * 
 */
int FE_compareModelInput(model_input_t* expected_array, model_input_t* obtained_array);

/**
 * \brief Compares nn model buffers
 * 
 * \param expected_array Expected output from nn model
 * \param obtained_array Obtained output from nn model
 * 
 * Takes the difference of the two arrays and returns the number of
 * elements that were not matched.
 * \return number of mismatches in two array
 * 
 */
int FE_compareModelOutput(model_output_t* expected_array, model_output_t* obtained_array);

/** @} */
#ifdef __cplusplus
}
#endif  /* extern "C" */

#endif /* FEATURE_EXTRACT_H_ */
