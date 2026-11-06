/* FRL: copy of ot-ti/src/uart.c (TI, BSD-3) with the changes marked FRL: a checked re-arm of the console read,
 * a bigger receive queue, and no CLI input from an empty queue. The original stays in ot-ti for the RCP builds. */
/*
 *  Copyright (c) 2017, Texas Instruments Incorporated
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <openthread/config.h>

#include <stddef.h>

#include <utils/code_utils.h>
#include <utils/uart.h>

#include <ti/drivers/dpl/SemaphoreP.h>

#include "ti_drivers_config.h"
#include <ti/drivers/UART2.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_uart.h)
#include <ti/drivers/dpl/HwiP.h>

#include "system.h"
#include <string.h>
#include <mqueue.h>

#ifdef USE_COMBINED_SERIAL
#include "ti/dmm/combined_serial/embedded/mux_task_app.h"
#include "ti/dmm/combined_serial/mux_common.h"
#endif
#if (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART)
#include <openthread/platform/debug_uart.h>
#include <stdio.h>
#endif

#include <ti/log/Log.h>
#ifdef ti_log_Log_ENABLE
#include "ti_log_config.h"
#endif

/* Ensure all bytes are written in blocking mode before notifying the stack it
 * can send more data. Less efficient than callback mode. Necessary for certain
 * versions of the UART2 driver.
 */
#define TI_PLAT_UART_BLOCKING 1

#define PLATFORM_UART_EVENT_TX_DONE (1U << 0)
#define PLATFORM_UART_EVENT_RX_DONE (1U << 1)
#define PLATFORM_UART_EVENT_REARM   (1U << 2) /* FRL: the callback could not re-arm the read */

/* FRL: console health counters, read by the watchdog reporter (app/die.c) */
volatile uint32_t     frl_console_dropped;
volatile uint32_t     frl_console_read_errors;
volatile uint32_t     frl_console_rearm_failures;
volatile int_fast16_t frl_console_rearm_status;
volatile uint32_t     frl_console_tx_stuck;

#define PLATFORM_UART_RECV_BUF_LEN 256
#define PLATFORM_UART_RECV_MQUEUE_LEN 16 /* FRL: was 6; a busy host can fall behind the console */

static uint8_t PlatformUart_receiveBuffer[PLATFORM_UART_RECV_BUF_LEN];

static UART2_Handle PlatformUart_uartHandle;

#if (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART)
static UART2_Handle PlatformDebugUart_uartHandle;
static char sDebugUartBuffer[500];
void uartDebugWriteCallback(UART2_Handle aHandle, void *aBuf, size_t aLen, void *userArg, int_fast16_t status);
#endif

#if !TI_PLAT_UART_BLOCKING
static SemaphoreP_Struct PlatformUart_writeSem;
static SemaphoreP_Handle PlatformUart_writeSemHandle;
#endif

/* Local Message Queue for Incoming Data */
const  char  UART_procQueueName[] = "/uartrx_process";
static mqd_t UART_procQueueDesc;
struct UART_procQueueMsg {
    uint8_t rxBuffer[PLATFORM_UART_RECV_BUF_LEN];
    uint8_t readLen;
};

static void uartReadCallback(UART2_Handle aHandle, void *aBuf, size_t aLen, void *userArg, int_fast16_t status)
{
    (void)aHandle;
    (void)aBuf;
    (void)userArg;
    (void)status;
    struct UART_procQueueMsg rxMsg;

    /* FRL: a read that ends with an error delivers no bytes; do not forward a stale buffer */
    if (status == UART2_STATUS_SUCCESS && aLen > 0)
    {
        memcpy(rxMsg.rxBuffer, PlatformUart_receiveBuffer, aLen);
        rxMsg.readLen = aLen;

        /* Process incoming data in task context */
        if (mq_send(UART_procQueueDesc, (char *)&rxMsg, sizeof(struct UART_procQueueMsg), 0) == 0)
        {
            platformUartSignal(PLATFORM_UART_EVENT_RX_DONE);
        }
        else
        {
            frl_console_dropped++;
        }
    }
    else
    {
        frl_console_read_errors++;
    }

    /* FRL: re-arm, and remember whether it took; platformUartProcess re-arms from task context if it did not */
    frl_console_rearm_status = UART2_read(PlatformUart_uartHandle, PlatformUart_receiveBuffer, sizeof(PlatformUart_receiveBuffer), NULL);
    if (frl_console_rearm_status != UART2_STATUS_SUCCESS)
    {
        frl_console_rearm_failures++;
        platformUartSignal(PLATFORM_UART_EVENT_REARM);
    }
}

#if !TI_PLAT_UART_BLOCKING
static void uartWriteCallback(UART2_Handle aHandle, void *aBuf, size_t aLen, void *userArg, int_fast16_t status)
{
    (void)aHandle;
    (void)aBuf;
    (void)aLen;
    (void)userArg;

    SemaphoreP_post(PlatformUart_writeSemHandle);
    platformUartSignal(PLATFORM_UART_EVENT_TX_DONE);
}
#endif /* !TI_PLAT_UART_BLOCKING */

otError otPlatUartEnable(void)
{
    struct mq_attr attr;

    /* FRL: opened early from the FreeRTOS entry so start-up failures can print; the CLI calls this again later */
    if (PlatformUart_uartHandle != NULL)
    {
        return OT_ERROR_NONE;
    }

    attr.mq_curmsgs = 0;
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = PLATFORM_UART_RECV_MQUEUE_LEN;
    attr.mq_msgsize = sizeof(struct UART_procQueueMsg);

    UART_procQueueDesc = mq_open(UART_procQueueName, (O_RDWR | O_NONBLOCK | O_CREAT), 0, &attr);

#ifdef USE_COMBINED_SERIAL
    /* MUX task owns the physical UART; skip UART2 hardware init.
     * The POSIX mqueue above is still needed for otSysProcessDrivers. */
    return OT_ERROR_NONE;
#else
    UART2_Params params;

#if !TI_PLAT_UART_BLOCKING
    PlatformUart_writeSemHandle = SemaphoreP_constructBinary(&PlatformUart_writeSem, 1U);
#endif

    UART2_Params_init(&params);

    params.readMode       = UART2_Mode_CALLBACK;
    params.readCallback   = uartReadCallback;
    params.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    params.eventMask      = UART2_EVENT_TX_FINISHED;
    params.baudRate       = 921600;
    params.dataLength     = UART2_DataLen_8;
    params.stopBits       = UART2_StopBits_1;
    params.parityType     = UART2_Parity_NONE;

#if TI_PLAT_UART_BLOCKING
    params.writeMode     = UART2_Mode_BLOCKING;
    params.writeCallback = NULL;
#else
    params.writeMode     = UART2_Mode_CALLBACK;
    params.writeCallback = uartWriteCallback;
#endif

#if (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART)
    UART2_Params debugParams;

    UART2_Params_init(&debugParams);

    debugParams.eventMask      = UART2_EVENT_TX_FINISHED;
    debugParams.baudRate       = 921600;
    debugParams.dataLength     = UART2_DataLen_8;
    debugParams.stopBits       = UART2_StopBits_1;
    debugParams.parityType     = UART2_Parity_NONE;
    debugParams.writeMode      = UART2_Mode_BLOCKING;

    PlatformDebugUart_uartHandle = UART2_open(CONFIG_DEBUG_UART, &debugParams);
#endif

    PlatformUart_uartHandle = UART2_open(CONFIG_UART2_0, &params);

    UART2_read(PlatformUart_uartHandle, PlatformUart_receiveBuffer, sizeof(PlatformUart_receiveBuffer), NULL);

    return OT_ERROR_NONE;
#endif /* USE_COMBINED_SERIAL */
}

otError otPlatUartDisable(void)
{
#ifdef USE_COMBINED_SERIAL
    /* MUX task owns the UART; nothing to close here. */
    return OT_ERROR_NONE;
#else
    UART2_close(PlatformUart_uartHandle);

    return OT_ERROR_NONE;
#endif
}

otError otPlatUartSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    /* FRL: polled writes straight to the UART registers instead of the driver's DMA transfer. The host's second
     * UART (the RCP link) is busy at the same time, and the console went dead mid-line with DMA; see the README. */
    uint16_t i;
    HWREG(UART0_BASE + UART_O_CTL) |= UART_CTL_TXE | UART_CTL_UARTEN; /* the driver leaves TXE off until it writes */
    for (i = 0; i < aBufLength; i++)
    {
        uint32_t spins = 200000;
        while (HWREG(UART0_BASE + UART_O_FR) & UART_FR_TXFF)
        {
            if (--spins == 0)
            {
                frl_console_tx_stuck++;
                return OT_ERROR_FAILED;
            }
        }
        HWREG(UART0_BASE + UART_O_DR) = aBuf[i];
    }
    platformUartSignal(PLATFORM_UART_EVENT_TX_DONE);
    return OT_ERROR_NONE;
}

void platformUartProcess(uintptr_t arg)
{

#ifdef ti_log_Log_ENABLE
    struct mq_attr mqstat;
    static int maxQSize = 0;

    mq_getattr(UART_procQueueDesc, &mqstat);
    if (mqstat.mq_curmsgs > maxQSize)
    {
        maxQSize = mqstat.mq_curmsgs;
    }
    if (mqstat.mq_curmsgs >= mqstat.mq_maxmsg)
    {
        Log_printf(LogModule_Thread, Log_VERBOSE, "platformUartProcess: Current Queue size: %d, Max Messages: %d High Watermark: %d", mqstat.mq_curmsgs, mqstat.mq_maxmsg, maxQSize);
    }
#endif

    if (arg & PLATFORM_UART_EVENT_TX_DONE)
    {
        otPlatUartSendDone();
    }

    if (arg & PLATFORM_UART_EVENT_RX_DONE)
    {
        struct UART_procQueueMsg rxMsg;

        /* FRL: drain every queued chunk, and never hand the CLI an empty-queue result (uninitialized bytes) */
        while (mq_receive(UART_procQueueDesc, (char *)&rxMsg, sizeof(struct UART_procQueueMsg), NULL) > 0)
        {
            otPlatUartReceived(rxMsg.rxBuffer, rxMsg.readLen);
        }
    }
    if (arg & PLATFORM_UART_EVENT_REARM)
    {
        /* FRL: the callback's re-arm failed; try again from task context (EINUSE means a read is already pending) */
        int_fast16_t rc = UART2_read(PlatformUart_uartHandle, PlatformUart_receiveBuffer, sizeof(PlatformUart_receiveBuffer), NULL);
        if (rc == UART2_STATUS_SUCCESS)
        {
            frl_console_rearm_status = rc;
        }
    }
}

#ifdef USE_COMBINED_SERIAL
/*!
 * @brief Deliver bytes received from the MUX NLI_OT channel into the
 *        OpenThread processing loop.
 *
 * Called from thread_mux.c (MUX task context) when a decoded NLI_OT frame
 * arrives.  Copies the payload into the UART POSIX mqueue and signals the
 * OT event loop via platformUartSignal(RX_DONE), mirroring what the UART2
 * ISR callback does in the non-MUX path.
 *
 * @param buf  Decoded spinel payload bytes.
 * @param len  Number of valid bytes in @p buf.
 */
void platformUartMuxDeliver(const uint8_t *buf, uint16_t len)
{
    struct UART_procQueueMsg rxMsg;
    uint16_t copyLen = (len > PLATFORM_UART_RECV_BUF_LEN)
                           ? (uint16_t)PLATFORM_UART_RECV_BUF_LEN
                           : len;

    memcpy(rxMsg.rxBuffer, buf, copyLen);
    rxMsg.readLen = copyLen;

    mq_send(UART_procQueueDesc, (char *)&rxMsg, sizeof(rxMsg), 0);
    platformUartSignal(PLATFORM_UART_EVENT_RX_DONE);
}
#endif /* USE_COMBINED_SERIAL */

otError otPlatUartFlush(void)
{
    return OT_ERROR_NOT_IMPLEMENTED;
}

#if (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART)
otError otSysDebugUart_write_bytes(const uint8_t *aBuf, uint16_t aBufLength)
{
    /* Unsupported */
}

void otPlatDebugUart_putchar_raw(int c)
{
    /* Unsupported */
}

void otPlatDebugUart_vprintf(const char *fmt, va_list ap)
{
    int ret;

    ret = vsnprintf(sDebugUartBuffer, sizeof(sDebugUartBuffer), fmt, ap);
    if (0 < ret)
    {
        // PuTTY likes \r\n
        size_t len                = (ret + 2U) < sizeof(sDebugUartBuffer) ? (ret + 2) : sizeof(sDebugUartBuffer);
        sDebugUartBuffer[len - 2] = '\r';
        sDebugUartBuffer[len - 1] = '\n';

        UART2_write(PlatformDebugUart_uartHandle, sDebugUartBuffer, len, NULL);
    }
}

void otPlatDebugUart_putchar(int c)
{

}
#endif