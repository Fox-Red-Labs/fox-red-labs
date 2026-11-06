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
#include "motorvision_hash.h"
#include <stdint.h>
#include <stdbool.h>

#include "motor_common.h"
#include "foc.h"
#include "mtr1_init.h"
#include "hall_cal.h"
#include "frl_guard.h"

extern HallCal_Data_t hallSensorData;

/* FRL port (2026-10-17): the frlGuard.* and Hall calibration entries are ours (frl_guard.h); TI listed VdcBus_V
 * three times, kept as shipped. Guard versions 3 to 6 (2026-10-17) add the shadow observer entries; version 7 (2026-10-19) the ISR timing and capture entries; version 8 (2026-10-19) the Hall tracking observer; version 9 (2026-10-19) the calibrated Hall table switch and its angles. The JSON beside this file is regenerated from this table. */
int16_t MVHASH_uint16_t_startIndex = 0;
int16_t MVHASH_uint32_t_startIndex = 37;
int16_t MVHASH_int16_t_startIndex  = -1;
int16_t MVHASH_int32_t_startIndex  = -1;
int16_t MVHASH_bool_startIndex     = 49;
int16_t MVHASH_float_startIndex    = 59;

volatile void* MVHASH_hash_table[MVHASH_NUMBER_OF_VARIABLES] = {
    &motorVars_M1.outVars.motorState,                   // uint16
    &motorVars_M1.outVars.faultMtrNow,                  // uint16
    &motorVars_M1.inputCtrl.startupMode,                // uint16
    &motorVars_M1.inputCtrl.estimatorMode,              // uint16
    &motorVars_M1.inputCtrl.svmMode,                    // uint16
    (void*)&frlGuard.version,                           // uint16
    (void*)&frlGuard.state,                             // uint16
    (void*)&frlGuard.reason,                            // uint16
    (void*)&frlGuard.heartbeat,                         // uint16
    (void*)&frlGuard.hall,                              // uint16
    (void*)&frlGuard.hallEdgesA,                        // uint16
    (void*)&frlGuard.hallEdgesB,                        // uint16
    (void*)&frlGuard.hallEdgesC,                        // uint16
    (void*)&frlGuard.nFault,                            // uint16
    &hallSensorData.state,                              // uint16
    (void*)&frlGuard.obsMode,                           // uint16
    (void*)&frlGuard.onObs,                             // uint16
    (void*)&frlGuard.hallFree,                          // uint16
    (void*)&frlGuard.capChan[0],                        // uint16
    (void*)&frlGuard.capChan[1],                        // uint16
    (void*)&frlGuard.capChan[2],                        // uint16
    (void*)&frlGuard.capChan[3],                        // uint16
    (void*)&frlGuard.capDecim,                          // uint16
    (void*)&frlGuard.capTrig,                           // uint16
    (void*)&frlGuard.capArm,                            // uint16
    (void*)&frlGuard.capState,                          // uint16
    (void*)&frlGuard.capWinMs,                          // uint16
    (void*)&frlGuard.hallPll,                           // uint16
    (void*)&frlGuard.hallTable,                         // uint16
    (void*)&frlGuard.hallTableOk,                       // uint16
    (void*)&frlGuard.ilimMode,                          // uint16
    (void*)&frlGuard.ilimOk,                            // uint16
    (void*)&frlGuard.dirRotor,                          // uint16
    (void*)&frlGuard.testMode,                          // uint16
    (void*)&frlGuard.ilimCode,                          // uint16
    (void*)&frlGuard.drvReset,                          // uint16
    (void*)&frlGuard.drvResets,                         // uint16
    &motorVars_M1.ISRCount,                             // uint32
    (void*)&frlGuard.runMs,                             // uint32
    (void*)&frlGuard.capMs,                             // uint32
    (void*)&frlGuard.hostPackets,                       // uint32
    (void*)&frlGuard.capLen,                            // uint32
    (void*)&frlGuard.capPre,                            // uint32
    (void*)&frlGuard.capCount,                          // uint32
    (void*)&frlGuard.capStart,                          // uint32
    (void*)&frlGuard.capIndex,                          // uint32
    (void*)&frlGuard.capWinIdxA,                        // uint32
    (void*)&frlGuard.capWinIdxB,                        // uint32
    (void*)&frlGuard.dirFlips,                          // uint32
    &motorVars_M1.statusVars.flagMotorIdentified,       // bool
    &motorVars_M1.inputCtrl.flagEnableRunAndIdentify,   // bool
    &motorVars_M1.inputCtrl.flagClearFaults,            // bool
    &motorVars_M1.inputCtrl.flagEnableFlyingStart,      // bool
    &motorVars_M1.inputCtrl.flagEnableDTC,              // bool
    &motorVars_M1.inputCtrl.flagEnableFWC,              // bool
    &motorVars_M1.inputCtrl.flagEnableMTPA,             // bool
    &motorVars_M1.inputCtrl.flagStartHallCal,           // bool
    &hallSensorData.flagCalValid,                       // bool
    &hallSensorData.flagCalDone,                        // bool
    &motorVars_M1.outVars.speed_Hz,                     // float
    &motorVars_M1.outVars.Is_A,                         // float
    &motorVars_M1.statusVars.Irms_A[0],                 // float
    &motorVars_M1.outVars.Idq_in_A.value[1],            // float
    &motorVars_M1.outVars.Idq_in_A.value[0],            // float
    &motorVars_M1.statusVars.angleObs_rad,              // float
    &motorVars_M1.inputCtrl.speedRef_Hz,                // float
    &motorSetVars_M1.Kp_spd,                            // float
    &motorSetVars_M1.Ki_spd,                            // float
    &motorSetVars_M1.overVoltageFault_V,                // float
    &motorSetVars_M1.underVoltageFault_V,               // float
    &motorSetVars_M1.Kp_fwc,                            // float
    &motorSetVars_M1.Ki_fwc,                            // float
    &motorSetVars_M1.Kp_Id,                             // float
    &motorSetVars_M1.Ki_Id,                             // float
    &motorSetVars_M1.Kp_Iq,                             // float
    &motorSetVars_M1.Ki_Iq,                             // float
    &motorSetVars_M1.maxCurrent_A,                      // float
    &motorSetVars_M1.overCurrent_A,                     // float
    &motorSetVars_M1.accelerationMax_Hzps,              // float
    &halFOCDataObj_M1.adcData.VdcBus_V,                 // float
    &halFOCDataObj_M1.adcData.VdcBus_V,                 // float
    &halFOCDataObj_M1.adcData.VdcBus_V,                 // float
    &halFOCDataObj_M1.adcData.I_A.value[0],             // float
    &halFOCDataObj_M1.adcData.I_A.value[1],             // float
    &halFOCDataObj_M1.adcData.I_A.value[2],             // float
    &halFOCDataObj_M1.adcData.V_V.value[0],             // float
    &halFOCDataObj_M1.adcData.V_V.value[1],             // float
    &halFOCDataObj_M1.adcData.V_V.value[2],             // float
    &motorSetVars_M1.olSpeedThr,                        // float
    &motorSetVars_M1.accelerationStart_Hzps,            // float
    &motorSetVars_M1.startCurrent_A,                    // float
    (void*)&frlGuard.peak_A,                            // float
    (void*)&frlGuard.limit_A,                           // float
    (void*)&frlGuard.speedMax_Hz,                       // float
    (void*)&frlGuard.handover_Hz,                       // float
    (void*)&frlGuard.obsSpeed_Hz,                       // float
    (void*)&frlGuard.obsErr_deg,                        // float
    (void*)&frlGuard.obsErrPk_deg,                      // float
    (void*)&frlGuard.obsTrim_deg,                       // float
    (void*)&frlGuard.obsTrimSet_deg,                    // float
    (void*)&frlGuard.angleAdj_deg,                      // float
    (void*)&frlGuard.busMin_V,                          // float
    (void*)&frlGuard.busMax_V,                          // float
    (void*)&frlGuard.isrUs_avg,                         // float
    (void*)&frlGuard.isrUs_max,                         // float
    (void*)&frlGuard.cpuLoad_pct,                       // float
    (void*)&frlGuard.capScale[0],                       // float
    (void*)&frlGuard.capScale[1],                       // float
    (void*)&frlGuard.capScale[2],                       // float
    (void*)&frlGuard.capScale[3],                       // float
    (void*)&frlGuard.capWin[0],                         // float
    (void*)&frlGuard.capWin[1],                         // float
    (void*)&frlGuard.capWin[2],                         // float
    (void*)&frlGuard.capWin[3],                         // float
    (void*)&frlGuard.pllSpeed_Hz,                       // float
    (void*)&frlGuard.pllErr_deg,                        // float
    (void*)&frlGuard.calFwd_deg[0],                     // float
    (void*)&frlGuard.calFwd_deg[1],                     // float
    (void*)&frlGuard.calFwd_deg[2],                     // float
    (void*)&frlGuard.calFwd_deg[3],                     // float
    (void*)&frlGuard.calFwd_deg[4],                     // float
    (void*)&frlGuard.calFwd_deg[5],                     // float
    (void*)&frlGuard.calRev_deg[0],                     // float
    (void*)&frlGuard.calRev_deg[1],                     // float
    (void*)&frlGuard.calRev_deg[2],                     // float
    (void*)&frlGuard.calRev_deg[3],                     // float
    (void*)&frlGuard.calRev_deg[4],                     // float
    (void*)&frlGuard.calRev_deg[5],                     // float
    (void*)&frlGuard.calSpread_deg,                     // float
    (void*)&frlGuard.ilim_V,                            // float
    (void*)&frlGuard.dirNow,                            // float
    (void*)&frlGuard.testAngle_deg,                     // float
    (void*)&frlGuard.testId_A,                          // float
    (void*)&frlGuard.testRamp_Aps,                      // float
    (void*)&frlGuard.testVd_V,                          // float
    (void*)&frlGuard.testIdNow_A,                       // float
    (void*)&frlGuard.cmpLimit_A,                        // float
    &motorVars_M1.outVars.Vdq_out_V.value[0],           // float
    &motorVars_M1.outVars.Vdq_out_V.value[1],           // float
    (void*)&frlGuard.ilimTest_V,                        // float
};
