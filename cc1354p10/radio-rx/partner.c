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
 * CC1354P10 test partner for the CC1407P radio lab: TI's rfPacketRx (prop_rf examples, F2 SDK 8.33)
 * grown into a radio that receives, sends, answers pings and starts pings, driven by text commands on
 * the XDS110 serial port (115200 8N1). The PHY is fixed at build time by SysConfig (phyType868 in
 * rfrx.syscfg); build with RX_ADV defined for TI's 802.15.4g PHYs (Wi-SUN FSK modes).
 *
 *   R                     listen (the default after boot): prints every packet and, every 5 s, the counters
 *   T <n> <len> <gap_ms>  send n packets "FRL C n=<k>" padded to len bytes
 *   E                     echo: answer every "FRL PING k" with "FRL PONG k r=<rssi>"
 *   G <n> <len> <tmo_ms> <gap_ms>  ping the other radio
 *   X <ms>                send back-to-back packets for ms (an interferer for carrier sense)
 *
 * With 802.15.4g settings this radio sends and stores each payload byte bit-reversed against the
 * CC1407P, so text is reversed on the way in and out (rev() below).
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
#include <ti/drivers/dpl/ClockP.h>
#include <ti_radio_config.h>

#ifndef MAX_LENGTH
#define MAX_LENGTH 255
#endif
#ifndef PHY_TEXT
#define PHY_TEXT "50 kbps 2-GFSK"
#endif
#define NUM_DATA_ENTRIES   4
#define NUM_APPENDED_BYTES 8
#ifdef RX_ADV
#define RX_CMD RF_cmdPropRxAdv
#define TX_CMD RF_cmdPropTxAdv
#define FLIP 1
#else
#define RX_CMD RF_cmdPropRx
#define TX_CMD RF_cmdPropTx
#define FLIP 0
#endif

static RF_Object rfObject; static RF_Handle rfHandle; static UART2_Handle uart;
static rfc_propRxOutput_t rxStats;
static uint8_t rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES, MAX_LENGTH + 4, NUM_APPENDED_BYTES)] __attribute__((aligned(4)));
static dataQueue_t dataQueue;
static RF_CmdHandle rxH = -1;
static int hexmode;
static uint8_t vbuf[MAX_LENGTH];
static uint8_t txbuf[MAX_LENGTH + 4];

#define RING 8
static struct { uint16_t len; uint8_t d[MAX_LENGTH + 1]; int8_t rssi; uint8_t status; uint32_t t; } ring[RING];
static volatile uint32_t head, tail, dropped;

static uint8_t rev(uint8_t b) { b = (uint8_t)((b & 0xF0) >> 4 | (b & 0x0F) << 4); b = (uint8_t)((b & 0xCC) >> 2 | (b & 0x33) << 2); return (uint8_t)((b & 0xAA) >> 1 | (b & 0x55) << 1); }
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)ClockP_getSystemTicks() * ClockP_getSystemTickPeriod() / 1000u); }

static void rx_cb(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
    (void)h; (void)ch;
    if (!(e & RF_EventRxEntryDone)) return;
    rfc_dataEntryGeneral_t *entry = RFQueue_getDataEntry();
    uint8_t *p = (uint8_t *)(&entry->data);
    uint16_t len; uint8_t *pl;
#ifdef RX_ADV
    len = (uint16_t)((((p[1] & 7u) << 8) | p[0]));        /* PHR low byte first; length includes the 4-byte checksum */
    len = len >= 4 ? (uint16_t)(len - 4) : 0; pl = p + 2;
#else
    len = p[0]; pl = p + 1;
#endif
    if (len > MAX_LENGTH) len = MAX_LENGTH;
    if (head - tail < RING) {
        uint32_t i = head % RING;
        ring[i].len = len;
        for (uint16_t k = 0; k < len; k++) ring[i].d[k] = FLIP ? rev(pl[k]) : pl[k];
        ring[i].d[len] = 0;
#ifdef RX_ADV
        ring[i].rssi = rxStats.lastRssi; ring[i].status = 0xEE;
#else
        ring[i].rssi = (int8_t)pl[len]; ring[i].status = pl[len + 1];
#endif
        ring[i].t = now_ms();
        head++;
    } else dropped++;
    GPIO_toggle(CONFIG_GPIO_RLED);
    RFQueue_nextEntry();
}

static void say(const char *s) { size_t n; UART2_write(uart, s, strlen(s), &n); }
static char line[400]; static unsigned lpos;
static int getline_(void)
{
    char c; size_t n;
    while (UART2_read(uart, &c, 1, &n) == UART2_STATUS_SUCCESS && n == 1) {
        if (c == '\r' || c == '\n') { if (lpos) { line[lpos] = 0; lpos = 0; return 1; } continue; }
        if (lpos < sizeof line - 1) line[lpos++] = c;
    }
    return 0;
}

/* A cancelled receive leaves TI's RFQueue helper and the radio pointing at different entries, and the
 * queue then fills after one packet; so every (re)start rebuilds the queue. */
static void rx_start(void)
{
    if (rxH >= 0) return;
    RFQueue_defineQueue(&dataQueue, rxDataEntryBuffer, sizeof(rxDataEntryBuffer), NUM_DATA_ENTRIES, MAX_LENGTH + 4 + NUM_APPENDED_BYTES);
    rxH = RF_postCmd(rfHandle, (RF_Op *)&RX_CMD, RF_PriorityNormal, &rx_cb, RF_EventRxEntryDone);
}
static void rx_stop(void) { if (rxH >= 0) { RF_cancelCmd(rfHandle, rxH, 0); RF_pendCmd(rfHandle, rxH, 0); rxH = -1; } }

static int send_bytes(const uint8_t *b, uint16_t len)
{
    if (len > MAX_LENGTH) len = MAX_LENGTH;
    uint16_t off = 0;
#ifdef RX_ADV
    uint16_t L = (uint16_t)(len + 4);
    txbuf[0] = (uint8_t)(L & 0xFF); txbuf[1] = (uint8_t)(((L >> 8) & 7u) | 0x08);
    off = 2;
#endif
    for (uint16_t k = 0; k < len; k++) txbuf[off + k] = FLIP ? rev(b[k]) : b[k];
    TX_CMD.pPkt = txbuf;
#ifdef RX_ADV
    TX_CMD.pktLen = (uint16_t)(off + len);
#else
    TX_CMD.pktLen = (uint8_t)len;
#endif
    rx_stop();
    RF_EventMask r = RF_runCmd(rfHandle, (RF_Op *)&TX_CMD, RF_PriorityNormal, NULL, 0);
    return (r & RF_EventLastCmdDone) && ((volatile RF_Op *)&TX_CMD)->status == PROP_DONE_OK;
}
static int b64v(char c) { if (c >= 'A' && c <= 'Z') return c - 'A'; if (c >= 'a' && c <= 'z') return c - 'a' + 26; if (c >= '0' && c <= '9') return c - '0' + 52; if (c == '+') return 62; if (c == '/') return 63; return -1; }
static uint16_t b64dec(const char *in, uint8_t *out, uint16_t max)
{
    uint32_t acc = 0; int bits = 0; uint16_t n = 0;
    for (; *in; in++) { int v = b64v(*in); if (v < 0) continue; acc = (acc << 6) | (uint32_t)v; bits += 6; if (bits >= 8) { bits -= 8; if (n < max) out[n++] = (uint8_t)(acc >> bits); } }
    return n;
}
static int send_text(const char *txt, uint16_t len)
{
    if (len > MAX_LENGTH) len = MAX_LENGTH;
    uint16_t k = 0, off = 0;
#ifdef RX_ADV
    uint16_t L = (uint16_t)(len + 4);                      /* 4-byte checksum appended by the radio */
    txbuf[0] = (uint8_t)(L & 0xFF); txbuf[1] = (uint8_t)(((L >> 8) & 7u) | 0x08);   /* whitening bit, as the CC1407P sends */
    off = 2;
#endif
    uint16_t tl = (uint16_t)strlen(txt);
    for (; k < len; k++) { uint8_t c = k < tl ? (uint8_t)txt[k] : (k == tl ? (uint8_t)' ' : (uint8_t)('a' + k % 26)); txbuf[off + k] = FLIP ? rev(c) : c; }
    TX_CMD.pPkt = txbuf;
#ifdef RX_ADV
    TX_CMD.pktLen = (uint16_t)(off + len);
#else
    TX_CMD.pktLen = (uint8_t)len;
#endif
    rx_stop();
    RF_EventMask r = RF_runCmd(rfHandle, (RF_Op *)&TX_CMD, RF_PriorityNormal, NULL, 0);
    return (r & RF_EventLastCmdDone) && ((volatile RF_Op *)&TX_CMD)->status == PROP_DONE_OK;
}

static const char *ap;
static int32_t argi(int32_t d) { while (*ap == ' ') ap++; if (!*ap) return d; int neg = 0; int32_t v = 0; if (*ap == '-') { neg = 1; ap++; } while (*ap >= '0' && *ap <= '9') v = v * 10 + (*ap++ - '0'); return neg ? -v : v; }

void *mainThread(void *arg0)
{
    (void)arg0;
    static char out[2 * MAX_LENGTH + 120];
    UART2_Params up; UART2_Params_init(&up); up.baudRate = 115200; up.readMode = UART2_Mode_NONBLOCKING;
    uart = UART2_open(CONFIG_UART2_0, &up);
    GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    say("\r\npartner: CC1354P10 at 915 MHz, " PHY_TEXT "\r\n");
    if (RFQueue_defineQueue(&dataQueue, rxDataEntryBuffer, sizeof(rxDataEntryBuffer), NUM_DATA_ENTRIES, MAX_LENGTH + 4 + NUM_APPENDED_BYTES)) { say("queue setup failed\r\n"); while (1) { } }
    RX_CMD.pQueue = &dataQueue; RX_CMD.pOutput = (uint8_t *)&rxStats;
    RX_CMD.rxConf.bAutoFlushIgnored = 1; RX_CMD.rxConf.bAutoFlushCrcErr = 1;
    RX_CMD.rxConf.bIncludeHdr = 1; RX_CMD.rxConf.bAppendRssi = 1; RX_CMD.rxConf.bAppendStatus = 1;
    #ifdef RX_ADV
    RX_CMD.maxPktLen = MAX_LENGTH + 4;   /* 16-bit field here; the 802.15.4g length includes the 4-byte checksum */
#else
    RX_CMD.maxPktLen = MAX_LENGTH;       /* 8-bit field in CMD_PROP_RX: 255 at most */
#endif
    RX_CMD.pktConf.bRepeatOk = 1; RX_CMD.pktConf.bRepeatNok = 1;
    RF_Params rp; RF_Params_init(&rp);
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup *)&RF_cmdPropRadioDivSetup, &rp);
    RF_postCmd(rfHandle, (RF_Op *)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
    rx_start();
    char mode = 'R'; uint32_t last = now_ms(), answered = 0;
    for (;;) {
        if (getline_()) {
            ap = line + 1; char op = line[0];
            snprintf(out, sizeof out, "> %s\r\n", line); say(out);
            if (op == 'R' || op == 'E') { mode = op; rx_start(); snprintf(out, sizeof out, "DONE %c\r\n", op); say(out); }
            else if (op == 'H') { hexmode = !hexmode; rx_start(); snprintf(out, sizeof out, "DONE H,%d\r\n", hexmode); say(out); }
            else if (op == 'S') { const char *t = line + 1; while (*t == ' ') t++; int ok = send_text(t, (uint16_t)strlen(t)); rx_start(); snprintf(out, sizeof out, "DONE S,%d\r\n", ok); say(out); }
            else if (op == 'W') { int32_t d = argi(13); RF_TxPowerTable_Value v = RF_TxPowerTable_findValue(txPowerTable_868_pa13, (int8_t)d); RF_Stat st = RF_setTxPower(rfHandle, v); snprintf(out, sizeof out, "DONE W,%ld,%d\r\n", (long)d, (int)st); say(out); }
            else if (op == 'V') {
                /* voice: each following line "A<base64>" is one frame to send at once; "X" ends */
                say("DONE V,ready\r\n");
                uint32_t sent = 0, fail = 0;
                for (;;) {
                    if (!getline_()) continue;
                    if (line[0] == 'X') break;
                    if (line[0] != 'A') continue;
                    uint16_t n = b64dec(line + 1, vbuf, MAX_LENGTH);
                    if (send_bytes(vbuf, n)) sent++; else fail++;
                }
                rx_start(); snprintf(out, sizeof out, "DONE VX,sent=%lu,fail=%lu\r\n", (unsigned long)sent, (unsigned long)fail); say(out);
            }
            else if (op == 'T') {
                int32_t n = argi(10), len = argi(20), gap = argi(0), ok = 0; char txt[32];
                for (int32_t k = 0; k < n; k++) { snprintf(txt, sizeof txt, "FRL C n=%ld", (long)k); ok += send_text(txt, (uint16_t)len); if (gap) { uint32_t t = now_ms(); while (now_ms() - t < (uint32_t)gap) { } } }
                rx_start(); snprintf(out, sizeof out, "DONE T,%ld,%ld\r\n", (long)ok, (long)n); say(out);
            } else if (op == 'X') {
                int32_t msr = argi(1000), n = 0; uint32_t t = now_ms();
                while (now_ms() - t < (uint32_t)msr) { n += send_text("FRL X", 200); }
                rx_start(); snprintf(out, sizeof out, "DONE X,%ld\r\n", (long)n); say(out);
            } else if (op == 'G') {
                int32_t n = argi(20), len = argi(30), tmo = argi(300), gap = argi(50), got = 0; long hs = 0, ts = 0, rs = 0; char txt[32];
                for (int32_t k = 0; k < n; k++) {
                    snprintf(txt, sizeof txt, "FRL PING %ld", (long)k);
                    head = tail = 0; uint32_t t0 = now_ms(); send_text(txt, (uint16_t)len); rx_start();
                    int hit = 0;
                    while (!hit && now_ms() - t0 < (uint32_t)tmo) {
                        while (tail != head) { uint32_t i = tail % RING; long kk, r;
                            if (sscanf((const char *)ring[i].d, "FRL PONG %ld r=%ld", &kk, &r) == 2 && kk == k) { hit = 1; got++; hs += ring[i].rssi; rs += r; ts += ring[i].t - t0;
                                snprintf(out, sizeof out, "PING,%ld,ok,rtt_ms=%lu,here=%d,there=%ld\r\n", (long)k, (unsigned long)(ring[i].t - t0), ring[i].rssi, r); say(out); }
                            tail++; } }
                    if (!hit) { snprintf(out, sizeof out, "PING,%ld,lost\r\n", (long)k); say(out); }
                    uint32_t t = now_ms(); while (now_ms() - t < (uint32_t)gap) { }
                }
                snprintf(out, sizeof out, "DONE G,%ld,%ld,rtt_ms_avg=%ld,here_avg=%ld,there_avg=%ld\r\n", (long)got, (long)n, got ? ts / got : 0, got ? hs / got : 0, got ? rs / got : 0); say(out);
            } else say("DONE ERR unknown\r\n");
        }
        while (tail != head) {
            uint32_t i = tail % RING;
            if (mode == 'E' && ring[i].len > 9 && !memcmp(ring[i].d, "FRL PING ", 9)) {
                long k = 0; char txt[32]; sscanf((const char *)ring[i].d + 9, "%ld", &k);
                snprintf(txt, sizeof txt, "FRL PONG %ld r=%d", k, ring[i].rssi);
                answered += send_text(txt, ring[i].len); rx_start();
            }
            int k = snprintf(out, sizeof out, "pkt len=%u rssi=%d status=0x%02x text=", ring[i].len, ring[i].rssi, ring[i].status);
            for (uint16_t b = 0; b < ring[i].len && b < 48 && k < (int)sizeof out - 4; b++) { char c = (char)ring[i].d[b]; out[k++] = (c >= 32 && c < 127) ? c : '.'; }
            out[k] = 0; say(out);
            if (hexmode) { k = 0; out[k++] = ' '; out[k++] = 'h'; out[k++] = '='; for (uint16_t b = 0; b < ring[i].len && k < (int)sizeof out - 4; b++) k += snprintf(out + k, sizeof out - k, "%02x", ring[i].d[b]); out[k] = 0; say(out); }
            say("\r\n");
            tail++;
        }
        if (now_ms() - last >= 5000) {
            last = now_ms();
            snprintf(out, sizeof out, "count ok=%u crc_bad=%u last_rssi=%d dropped=%lu answered=%lu\r\n", rxStats.nRxOk, rxStats.nRxNok, rxStats.lastRssi, (unsigned long)dropped, (unsigned long)answered);
            say(out);
        }
    }
}
