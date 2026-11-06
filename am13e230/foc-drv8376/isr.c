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

#include "application.h"
#include "mtr_config.h"
#include "system_config.h"
#include "main.h"
#include "motorvision.h"
#include "frl_guard.h"
#include "motorvision_uart_hal.h"
#include "ti_sdk_dl_config.h"

#if FRL_COMMISSIONING
/* Revision 30, commissioning build only: the ISR's own duration, and a dead-man for bounded hand-offs to TI's state
 * machine. While armed, the supervising background loop clears frlIsrSinceBg every pass; if 4000 ISRs (200 ms at
 * 20 kHz) pass without that, the background is starved, so the ISR disables the driver and clears TI's run flag. */
volatile uint32_t frlArmed = 0U, frlIsrSinceBg = 0U, frlStarved = 0U, frlIsrMaxCycles = 0U, frlIsrCount = 0U;
#endif
/* ADC Trigger Interrupt to execute the FOC state machine*/
MOTOR_CTRL_RAMFUNC void motor1CtrlISR_Handler(void)
{
#if !FRL_COMMISSIONING
    FRL_GUARD_ISR_ENTER();  // revision H: cycle counter at the ISR's start; frl_guard_isr reads it at the end (frl_guard.h)
#endif
#if FRL_COMMISSIONING
    uint32_t frlC0 = DWT->CYCCNT;
#endif
    /* Acknowledge ADC interrupt (ADC2 INT1, raised after the last conversion of the sequence).
     * ISR_TIMING is PA5, LaunchPad LED4 (active low), a scope point for the ISR duration. */
    HAL_ackMtrADCInt(&motorVars_M1.halInterface->halFOCConfigHandle->intCfg);

    HAL_setGPIOHigh((uint32_t)MTR1_FOC_CTRL_PORT_BASE, MTR1_FOC_CTRL_ISR_TIMING_PIN);

    /* Runs the FOC Control for the Motor 1 */
    application_run((MOTOR_Handle)&motorVars_M1);

    MOTORVISION_capturePlotData();  // Capture to ping-pong buffer
#if !FRL_COMMISSIONING
    frl_guard_isr();  // bench guard: phase-current trip, capture buffer and ISR timing (frl_guard.h); keep it last
#endif

    HAL_setGPIOLow((uint32_t)MTR1_FOC_CTRL_PORT_BASE, MTR1_FOC_CTRL_ISR_TIMING_PIN);

    systemVars.timerCntFOCCtrlISR++;
#if FRL_COMMISSIONING
    {
        uint32_t d = DWT->CYCCNT - frlC0;
        if (d > frlIsrMaxCycles) frlIsrMaxCycles = d;
        frlIsrCount++;
        if ((frlArmed != 0U) && (++frlIsrSinceBg > 4000U))
        {
            DL_GPIO_setPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
            motorVars_M1.inputCtrl.flagEnableRunAndIdentify = false;
            HAL_disablePWM(&halFOCConfig_M1.pwmCfg, &motorVars_M1.halInterface->halFOCDataHandle->pwmData);
            frlStarved = 1U;
            frlArmed   = 0U;
        }
    }
#endif
}

/**
 * \brief UART RX ISR for MotorVision GUI communication.
 *
 * The ISR lives here because SERIAL_DATA_PLOT_INT_Handler is a
 * SysConfig-generated vector table symbol specific to this solution.
 * All communication stack logic is handled inside MV_Uart_HandleRxInterrupt().
 */
void SERIAL_DATA_PLOT_INT_Handler(void)
{
    MV_Uart_HandleRxInterrupt(SERIAL_DATA_PLOT_INST);
}
