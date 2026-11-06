/*
 * Copyright (C) 2017-2022 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 *   Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file mtr_config.h
 * \brief Motor configuration aggregation header
 *
 * \defgroup MTR_CONFIG_MODULE Motor Configuration Module
 * \brief Aggregates motor configuration headers for single and multi-motor setups
 * @{
 */

#ifndef USER_H
#define USER_H

#ifdef __cplusplus
extern "C" {
#endif

// the includes
#include "user_motor.h"
#if defined(DUAL_MOTOR_CONFIG) || defined(TRI_MOTOR_CONFIG) || defined(QUAD_MOTOR_CONFIG)
#include "user_mtr2.h"
#if defined(TRI_MOTOR_CONFIG) || defined(QUAD_MOTOR_CONFIG)
#include "user_mtr3.h"
#if defined(QUAD_MOTOR_CONFIG)
#include "user_mtr4.h"
#endif  // QUAD_MOTOR_CONFIG
#endif  // TRI_MOTOR_CONFIG || QUAD_MOTOR_CONFIG
#endif  // DUAL_MOTOR_CONFIG || TRI_MOTOR_CONFIG || QUAD_MOTOR_CONFIG

#include "mtr1_init.h"
#if defined(DUAL_MOTOR_CONFIG) || defined(TRI_MOTOR_CONFIG) || defined(QUAD_MOTOR_CONFIG)
#include "motor2_init.h"
#if defined(TRI_MOTOR_CONFIG) || defined(QUAD_MOTOR_CONFIG)
#include "motor3_init.h"
#if defined(QUAD_MOTOR_CONFIG)
#include "motor4_init.h"
#endif  // QUAD_MOTOR_CONFIG
#endif  // TRI_MOTOR_CONFIG || QUAD_MOTOR_CONFIG
#endif  // DUAL_MOTOR_CONFIG || TRI_MOTOR_CONFIG || QUAD_MOTOR_CONFIG

// modules
#include "userParams.h"

#define USER_MOTOR_Ls_d_H MOTOR_PH_Ls_d_H
#define USER_MOTOR_Ls_q_H MOTOR_PH_Ls_q_H

#ifdef __cplusplus
}
#endif

#endif /* USER_H */

/** @} */
