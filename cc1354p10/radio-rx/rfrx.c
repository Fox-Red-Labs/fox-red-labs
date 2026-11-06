/*
 * Copyright (c) 2019, Texas Instruments Incorporated
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
 * Modified by Fox Red Labs, 2026. Derived from TI's rfPacketRx.c, TI prop_rf examples (SimpleLink F2 SDK 8.33.00.16).
 * The TI copyright and license above are kept as in the original file.
 */
/*
 * CC1354P10 sub-1 GHz receiver for the BP-CC1407P-9-FE demo: TI's rfPacketRx (prop_rf examples,
 * F2 SDK 8.33) with three changes. It listens at 915 MHz, keeps packets that fail the CRC so a
 * format mismatch shows instead of silence, and prints every packet with its RSSI on the
 * XDS110 serial port (115200 8N1), plus a count line about every 14 s.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)
#include "ti_drivers_config.h"
#include "RFQueue.h"
#include <ti_radio_config.h>

#ifndef MAX_LENGTH
#define MAX_LENGTH          64
#endif
#define NUM_DATA_ENTRIES    4
#define NUM_APPENDED_BYTES  3   /* length header byte, RSSI byte, status byte */
/* -DRX_ADV: for TI's 802.15.4g PHYs (Wi-SUN FSK modes), which receive with CMD_PROP_RX_ADV.
 * Each entry starts with the 2-byte PHR (11-bit length, FCS included, stored low byte first); the radio core's counters decide ok or bad. */
#ifdef RX_ADV
#define RX_CMD RF_cmdPropRxAdv
#else
#define RX_CMD RF_cmdPropRx
#endif
#ifndef PHY_TEXT
#define PHY_TEXT "50 kbps 2-GFSK, sync 0x930B51DE"
#endif

static RF_Object rfObject;
static RF_Handle rfHandle;
static UART2_Handle uart;
static rfc_propRxOutput_t rxStats;
static uint8_t rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES, MAX_LENGTH, NUM_APPENDED_BYTES)] __attribute__((aligned(4)));
static dataQueue_t dataQueue;

/* packets handed from the RF callback to the main loop */
#define RING 8
static struct { uint16_t len; uint8_t data[MAX_LENGTH + 4]; int8_t rssi; uint8_t status; } ring[RING];
static volatile uint32_t head, tail, dropped;

static void callback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
    (void)h; (void)ch;
    if (!(e & RF_EventRxEntryDone)) { return; }
    rfc_dataEntryGeneral_t *entry = RFQueue_getDataEntry();
    uint8_t *p = (uint8_t *)(&entry->data);
#ifdef RX_ADV
    /* the radio core stores the 16-bit PHR low byte first: 08 40 is length 64 with the whitening bit set */
    uint16_t len = (uint16_t)(2u + (((p[1] & 7u) << 8) | p[0]));
    if (len > MAX_LENGTH) { len = MAX_LENGTH; }
#else
    uint16_t len = p[0];
    if (len > MAX_LENGTH) { len = MAX_LENGTH; }
#endif
    if (head - tail < RING) {
        uint32_t i = head % RING;
        ring[i].len = len;
#ifdef RX_ADV
        memcpy(ring[i].data, p, len);
        ring[i].rssi = rxStats.lastRssi;
        ring[i].status = 0xEE;   /* not read in this mode: the count line's ok and crc_bad decide */
#else
        memcpy(ring[i].data, p + 1, len);
        ring[i].rssi = (int8_t)p[1 + len];
        ring[i].status = p[2 + len];
#endif
        head++;
    } else { dropped++; }
    GPIO_toggle(CONFIG_GPIO_RLED);
    RFQueue_nextEntry();
}

static void say(const char *s) { size_t n; UART2_write(uart, s, strlen(s), &n); }

void *mainThread(void *arg0)
{
    (void)arg0;
    char line[2 * MAX_LENGTH + 120];
    UART2_Params up; UART2_Params_init(&up); up.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &up);
    GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    say("\r\nrfrx: CC1354P10 listening at 915 MHz, " PHY_TEXT "\r\n");

    if (RFQueue_defineQueue(&dataQueue, rxDataEntryBuffer, sizeof(rxDataEntryBuffer), NUM_DATA_ENTRIES, MAX_LENGTH + NUM_APPENDED_BYTES)) {
        say("queue setup failed\r\n"); while (1) { }
    }
    RX_CMD.pQueue = &dataQueue;
    RX_CMD.pOutput = (uint8_t *)&rxStats;
    RX_CMD.rxConf.bAutoFlushIgnored = 1;
    RX_CMD.rxConf.bAutoFlushCrcErr = 0;   /* keep CRC failures: they show a format mismatch */
    RX_CMD.rxConf.bIncludeHdr = 1;
    RX_CMD.rxConf.bAppendRssi = 1;
    RX_CMD.rxConf.bAppendStatus = 1;
    RX_CMD.maxPktLen = MAX_LENGTH;
    RX_CMD.pktConf.bRepeatOk = 1;
    RX_CMD.pktConf.bRepeatNok = 1;

    RF_Params rfParams; RF_Params_init(&rfParams);
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup *)&RF_cmdPropRadioDivSetup, &rfParams);
    RF_postCmd(rfHandle, (RF_Op *)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
    RF_postCmd(rfHandle, (RF_Op *)&RX_CMD, RF_PriorityNormal, &callback, RF_EventRxEntryDone);

    uint32_t n = 0, last = 0;
    for (;;) {
        while (tail != head) {
            uint32_t i = tail % RING;
            int k = snprintf(line, sizeof line, "pkt %lu len=%u rssi=%d status=0x%02x data=", (unsigned long)++n, ring[i].len, ring[i].rssi, ring[i].status);
            for (uint16_t b = 0; b < ring[i].len && k < (int)sizeof line - 4; b++) { k += snprintf(line + k, sizeof line - k, "%02x", ring[i].data[b]); }
            snprintf(line + k, sizeof line - k, "\r\n");
            say(line);
            tail++;
        }
        /* every 5000 loop passes, about 14 s as measured (each pass is the 1 ms wait plus the printing): counts from the radio core */
        if (++last >= 5000) {
            last = 0;
            snprintf(line, sizeof line, "count ok=%u crc_bad=%u ignored=%u last_rssi=%d dropped=%lu\r\n",
                     rxStats.nRxOk, rxStats.nRxNok, rxStats.nRxIgnored, rxStats.lastRssi, (unsigned long)dropped);
            say(line);
        }
        for (volatile uint32_t d = 0; d < 12000; d++) { }   /* about 1 ms at 48 MHz */
    }
}
