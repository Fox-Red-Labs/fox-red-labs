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

/*
 * motorvision_uart_hal.c -- MotorVision UART HAL implementation
 *
 * Provides UART TX primitives and the RX interrupt handler for the
 * MotorVision protocol. The UART instance is injected via MV_Uart_Init()
 * so this file contains no hardcoded SysConfig-generated symbols.
 *
 * The solution's isr.c owns the vector table entry (ISR function name) and
 * delegates all communication stack logic to MV_Uart_HandleRxInterrupt().
 */

#include "motorvision_uart_hal.h"
#include "motorvision_hash.h"
#include "export.h"

/* ========================================================================== */
/*                           Module-Level State                               */
/* ========================================================================== */

/* Context struct -- all module-level state in one place (compliance rule 10). */
typedef struct
{
    /* UART instance set by MV_Uart_Init() -- stored as void* to match the
     * device-agnostic header; cast to UART_Regs* at point of use. */
    void *UartInst;

    /* Accumulation buffer for incoming MotorVision RX packets.
     * Bytes are collected across multiple ISR invocations (one byte per
     * interrupt when RX FIFO threshold = NOT_EMPTY) until a complete
     * EXPORT_RX_BUFFER_SIZE-byte packet has been received. */
    uint16_t RxBuffer[EXPORT_RX_BUFFER_SIZE];

    /* Number of bytes accumulated so far in RxBuffer. */
    uint16_t RxIndex;
} MV_Uart_ContextType;

static MV_Uart_ContextType MV_Uart_Ctx = {NULL, {0U}, 0U};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void MV_Uart_Init(void *uartInst)
{
    MV_Uart_Ctx.UartInst = uartInst;
}

int32_t MV_Uart_Transmit(const char *data, uint16_t length)
{
    DL_UNICOMM_Inst_Regs *uartPtr;
    uint16_t              i;
    int32_t               retVal = 0;

    if (MV_Uart_Ctx.UartInst == NULL)
    {
        retVal = -1;
    }
    else
    {
        uartPtr = (DL_UNICOMM_Inst_Regs *)MV_Uart_Ctx.UartInst;

        for (i = 0U; i < length; i++)
        {
            if (DL_UART_isTXFIFOFull(uartPtr))
            {
                /* TX FIFO full -- non-blocking: report error without spinning */
                retVal = -2;
                break;
            }
            else
            {
                DL_UART_transmitData(uartPtr, (uint8_t)data[i]);
            }
        }
    }

    return retVal;
}

/* FRL port (2026-10-17): TI's parser uses a write's or a poll's variable index straight as an index into
 * MVHASH_hash_table, with no bounds check; an index past the end reads or writes through a stray pointer. Drop such a
 * packet here, before the parser sees it (a well-framed packet only: 0x73 ... 0x65). */
static bool MV_Uart_PacketIndicesValid(const uint16_t *pkt)
{
    if (pkt[0] != 0x73U || pkt[EXPORT_RX_BUFFER_SIZE - 1U] != 0x65U)
    {
        return true; /* not a packet: TI answers PACKET_ERROR and touches nothing */
    }
    uint16_t cmd = pkt[1];
    if (cmd == 43U) /* WRITE_VAR: payload[0] is the index */
    {
        return pkt[2] < MVHASH_NUMBER_OF_VARIABLES;
    }
    if (cmd == 44U) /* INIT_STREAMING_VARS: four indices */
    {
        for (uint16_t i = 0U; i < 4U; i++)
        {
            if (pkt[2U + i] >= MVHASH_NUMBER_OF_VARIABLES) return false;
        }
        return true;
    }
    if (cmd >= 63U && cmd <= 75U) /* START_POLLING_1..13: that many indices */
    {
        for (uint16_t i = 0U; i < (uint16_t)(cmd - 62U); i++)
        {
            if (pkt[2U + i] >= MVHASH_NUMBER_OF_VARIABLES) return false;
        }
    }
    return true;
}

/* FRL port (2026-10-17): drain the receive FIFO completely. Shared by the interrupt and MV_Uart_Service(). */
static void MV_Uart_DrainRx(DL_UNICOMM_Inst_Regs *uartPtr)
{
    while (!DL_UART_isRXFIFOEmpty(uartPtr))
    {
        MV_Uart_Ctx.RxBuffer[MV_Uart_Ctx.RxIndex] = (uint16_t)DL_UART_receiveData(uartPtr);
        MV_Uart_Ctx.RxIndex++;
        if (MV_Uart_Ctx.RxIndex >= EXPORT_RX_BUFFER_SIZE)
        {
            if (MV_Uart_PacketIndicesValid(MV_Uart_Ctx.RxBuffer))
            {
                EXPORT_rxCallback(MV_Uart_Ctx.RxBuffer, EXPORT_RX_BUFFER_SIZE);
            }
            MV_Uart_Ctx.RxIndex = 0U;
        }
    }
}

void MV_Uart_HandleRxInterrupt(void *uartInst)
{
    DL_UNICOMM_Inst_Regs *uartPtr = (DL_UNICOMM_Inst_Regs *)uartInst;
    uint32_t              uartStatus;

    uartStatus = DL_UART_getPendingInterrupt(uartPtr);

    if ((uartStatus & DL_UART_IIDX_RX) != 0U)
    {
        /* FRL port (2026-10-17): clear first, then drain. TI's order (drain, then clear) loses the interrupt of a byte
         * that lands between the two; the receive interrupt fires on a FIFO level crossing, so with the FIFO left
         * above the level it never fires again: on the bench the FIFO sat full with OVRERR set after about 100 s of
         * traffic and the board stopped hearing the host. */
        DL_UART_clearInterruptStatus(uartPtr, DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_OVERRUN_ERROR);
        MV_Uart_DrainRx(uartPtr);
    }
}

/* FRL port: called every millisecond from the background loop. Drains anything the interrupt left in the FIFO and
 * clears an overrun, so the receive path recovers by itself; the host resyncs on PACKET_ERROR. */
void MV_Uart_Service(void)
{
    if (MV_Uart_Ctx.UartInst == NULL)
    {
        return;
    }
    DL_UNICOMM_Inst_Regs *uartPtr = (DL_UNICOMM_Inst_Regs *)MV_Uart_Ctx.UartInst;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    DL_UART_clearInterruptStatus(uartPtr, DL_UART_INTERRUPT_OVERRUN_ERROR);
    MV_Uart_DrainRx(uartPtr);
    __set_PRIMASK(primask);
}
