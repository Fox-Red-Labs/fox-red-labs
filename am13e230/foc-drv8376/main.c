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
// include the related header files
//
#include "mtr_config.h"
#include "main.h"
#include "motor_params.h"
#include "system_config.h"
#include "motorvision.h"
#include "export_package.h"
#include "frl_guard.h"

/* SDK includes */
#include "device.h"
#include "log.h"
#include "hal.h"
#include "commissioning.h"

/* Enable the TMU before C runtime initialization can evaluate fast math. */
int _system_pre_init(void) { SCB->CPACR |= (3UL << 0); __DSB(); __ISB(); return 1; }

//! Motor handle array for multi-motor support (defined in sys_main.h)
//! For dual motor configuration, both motors are initialized here
// MOTOR_Handle motorHandles[NUM_MOTORS] ;

/* ============================================================================
 * System Variables Instantiations
 * ============================================================================
 * Motor control variables are instantiated in mtr1_init.c (solution initialization).
 * System-level variables are instantiated here in main.c (application layer).
 */

/**
 * \brief System-level runtime variables
 *
 * Instantiated here in main.c. Contains timers, flags, and DAC control
 * structures used throughout the application.
 */
SYSTEM_Vars_t systemVars;

// **************************************************************************
// the functions
// !!! Please make sure that you had gone through the user guide, and follow the
// !!! guide to set up the kit and load the right code
int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
#if FRL_COMMISSIONING
    DL_SYSCTL_disableDACOUT(); // PA4 may carry the temporary ILIMIT measurement lead.
#else
    frl_guard_boot(); // ILIMIT at full scale before the gate driver is enabled (frl_guard.h)
#endif

    // Serial Communication
    DL_UART_enable(SERIAL_DATA_PLOT_INST);
    systemVars.commUartInstPtr = (uint32_t)SERIAL_DATA_PLOT_INST;
    MOTORVISION_init((void *)systemVars.commUartInstPtr);
#if !FRL_COMMISSIONING
    {   /* FRL port (2026-10-17): start quiet. TI's firmware streams its four default scope variables from boot (about
         * 370 KB/s at 9.2 Mbaud) until a host stops it; our hosts only poll, and a browser behind Web Serial could not
         * keep up with the flood to find its sync acknowledgement. INITCMD (40) is the protocol's own reset: stream off,
         * poll list empty. It also queues one acknowledgement packet, which a host ignores. */
        uint16_t none[13] = {0U};
        EXPORTPACKAGE_takeActionOnMessage(40U, none, false);
    }
#endif

    HAL_disableGlobalInts();
#if FRL_COMMISSIONING
    commissioning_init();
#endif

    // initialize the Peripherals
    halHandle = HAL_init(&hal, sizeof(hal));

    // Initialize the Handles for Motor Variables and Parameters
    initMotor1Handles((MOTOR_Handle)&motorVars_M1);

    // Initialize motor control parameters (PI controllers, trajectories, etc.)
    initMotor1CtrlParameters((MOTOR_Handle)&motorVars_M1, motorParams_M1);

    // Initialize motor control system (ISR, interrupts, PWM, gate driver, parameters, offset cal)
    initMotorControlSystem((MOTOR_Handle)&motorVars_M1);

    // set the driver parameters
    HAL_setParams(halHandle);

#if FRL_COMMISSIONING
    commissioning_stage("TI peripherals initialized; comparator calibration");
#endif
    // Run comparator DAC offset calibration (blocking, before ISR starts).
    // HAL_1S_setCompIlimConfig for over-current protection and IPD current limiting.
    HAL_CompOffsetCalInit(motorVars_M1.halInterface);
    uint32_t compStart = DWT->CYCCNT;
    while (HAL_CompOffsetCalStep(motorVars_M1.halInterface) == 0U)
    {
#if FRL_COMMISSIONING
        if ((uint32_t)(DWT->CYCCNT - compStart) > 200000000U) {
            commissioning_stage("comparator calibration timeout");
            commissioning_loop();
        }
#endif
    }

#if FRL_COMMISSIONING
    commissioning_stage("comparator calibration done; ADC offset calibration");
#endif
    // Seed ADC offsets. Commissioning keeps PWM forced low; normal zero-voltage PWM switches.
    // The IIR filter calibration runs inside the ISR in MOTOR_OFFSET_CALIB state
    // using PWM-synchronous ADC data.
    HAL_RunOffsetCalInit(motorVars_M1.halInterface);

    // Set initial motor state to offset calibration - ISR will run the IIR filter
    // and transition to MOTOR_STOP_IDLE when offsetCalibTimeSet_cnt ticks complete.
    motorVars_M1.outVars.motorState = MOTOR_OFFSET_CALIB;

    // Initialize DAC outputs for real-time debugging
    initDACOutputs(&systemVars, (MOTOR_Handle)&motorVars_M1);

    // Enable global interrupts (PRIMASK) - ISR now runs offset calibration.
    // motorVars_M1.statusVars.flagOffsetCalibDone and flagEnableSystem remain false here; both are
    // set to true by StateMachine_handleOffsetCalib() once the IIR window
    // completes.  The spin-wait below blocks until that happens.
    HAL_enableGlobalInts();

    systemVars.powerRelayWaitTime_ms = POWER_RELAY_WAIT_TIME_ms;

    systemVars.flagEnableSystem = true;

    systemVars.flagInitializeDone = true;

    // Wait here until the ISR-driven offset calibration completes.
    // StateMachine_handleOffsetCalib() sets motorVars_M1.statusVars.flagOffsetCalibDone then
    // flagEnableSystem when the IIR window finishes.
    uint32_t calStart = DWT->CYCCNT;
    while (motorVars_M1.statusVars.flagOffsetCalibDone == false)
    {
#if FRL_COMMISSIONING
        if ((uint32_t)(DWT->CYCCNT - calStart) > 1000000000U) {
            commissioning_stage("ADC calibration timeout");
            commissioning_loop();
        }
#endif
    }
#if FRL_COMMISSIONING
    commissioning_stage("TI ADC calibration complete");
    commissioning_loop();
#endif
#if !FRL_COMMISSIONING
    frl_guard_ilim_check(); // ILIMIT read-back with the ADC running (H5b; at boot it read nothing)
#endif

    while (systemVars.flagEnableSystem == true)
    {
        // Serial Communication
        MOTORVISION_sendPlotData();
        if (systemVars.timerCntFOCCtrlISR > LOW_PRIORITY_WAIT_TIME_1ms)
        {
            systemVars.timerCntFOCCtrlISR = 0;

            // Send & Receive Polling Data for GUI Comm Stack
            MOTORVISION_captureAndSendPollingData();
#if !FRL_COMMISSIONING
            frl_guard_tick_1ms();
#endif

            FOC_runLowPriorityStateMachine(&motorVars_M1);
        }
#ifdef DUAL_MOTOR_CONFIG
        else
        {
            FOC_runLowPriorityStateMachine(&motorVars_M2);
        }
#endif
    }  // end of while() loop

    return 0;
}  // end of main() function
