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
 * \file motorvision_uart_hal.h
 * \brief MotorVision UART HAL -- solution-layer UART abstraction for MotorVision
 *
 * Provides UART TX primitives, initialization, and the RX interrupt handler
 * for the MotorVision protocol. The UART instance is injected at init time so
 * the library remains peripheral-agnostic.
 *
 * The solution's isr.c owns the SysConfig-generated vector table symbol and
 * delegates all communication stack logic to MV_Uart_HandleRxInterrupt().
 *
 * \defgroup MOTORVISION_UART_HAL MotorVision UART HAL
 * \brief UART communication primitives for MotorVision GUI protocol
 * @{
 */

#ifndef MOTORVISION_UART_HAL_H
#define MOTORVISION_UART_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ti_sdk_dl_config.h"

/**
 * \brief Initialize the MotorVision UART HAL with the given UART instance.
 *
 * Must be called before any transmit functions. Stores the UART instance
 * for use by all subsequent TX operations.
 *
 * \param[in] uartInst Opaque pointer to the UART peripheral registers.
 *                     Passed as void* to keep this header device-agnostic.
 *                     Cast to UART_Regs* internally in motorvision_uart_hal.c.
 *                     (e.g. APP_UART_0_INST from ti_sdk_dl_config.h)
 */
void MV_Uart_Init(void *uartInst);

/**
 * \brief Transmit a byte array via UART (non-blocking).
 *
 * Writes bytes to the TX FIFO without spinning. If the TX FIFO is full
 * when a byte is about to be written, the function stops immediately and
 * returns an error code; it never busy-waits.
 * Called by the MotorVision export_package layer to send protocol packets.
 *
 * \param[in] data   Pointer to byte array to transmit
 * \param[in] length Number of bytes to transmit
 *
 * \return  0  All bytes written successfully.
 * \return -1  UART instance not initialised (MV_Uart_Init() not called).
 * \return -2  TX FIFO full; transmission aborted before all bytes were sent.
 */
int32_t MV_Uart_Transmit(const char *data, uint16_t length);

/**
 * \brief Handle a UART RX interrupt for the MotorVision communication channel.
 *
 * Non-blocking ISR handler. Drains all bytes currently available in the RX
 * FIFO using DL_UART_isRXFIFOEmpty() / DL_UART_receiveData() without ever
 * blocking. Bytes are accumulated in a module-level buffer across multiple
 * ISR invocations (the RX FIFO threshold is NOT_EMPTY, so the interrupt may
 * fire with fewer than EXPORT_RX_BUFFER_SIZE bytes present).
 * EXPORT_rxCallback() is dispatched only once a complete
 * EXPORT_RX_BUFFER_SIZE-byte packet has been assembled, preserving GUI
 * packet framing integrity.
 *
 * Intended to be called directly from the solution UART ISR so that all
 * communication stack logic is contained in the HAL/library layer and the
 * ISR body remains a single call.
 *
 * \param[in] uartInst Opaque pointer to the UART peripheral registers
 *                     (e.g. SERIAL_DATA_PLOT_INST from ti_sdk_dl_config.h)
 */
void MV_Uart_HandleRxInterrupt(void *uartInst);

/* FRL port: background service, drains a stuck receive FIFO (see motorvision_uart_hal.c). */
void MV_Uart_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTORVISION_UART_HAL_H */

/** @} */
