/*
 * Fox Red Labs board-to-board test: LP-EM-CC2340R5 as Bluetooth LE central, LP-CC1354P10-1 as peripheral.
 * Scans for the advertised name "Fox Red 1354", connects, finds Project Zero's LED0 characteristic
 * (F0001111-0451-4000-B000-000000000000) by UUID, then writes 1, 0, 1, 0 ... TOGGLES times, waiting for
 * each write response, and prints the round-trip time of every write over the UART menu.
 * Added to TI's basic_ble example (F3 SDK 9.21); TI's files are unchanged except for one call in app_central.c.
 */
#include <string.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <app_main.h>

#define TARGET_NAME   "Fox Red 1354"
#define TOGGLES       20
#define LINE_STATUS   APP_MENU_GENERAL_STATUS_LINE
#define LINE_RESULT   APP_MENU_PROFILE_STATUS_LINE

/* Project Zero LED0 characteristic, little-endian as it appears on air */
static const uint8 led0Uuid[ATT_UUID_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x40, 0x51, 0x04, 0x11, 0x11, 0x00, 0xF0 };

static uint8  targetAddr[B_ADDR_LEN];
static uint8  targetAddrType;
static uint8  targetFound = 0;
static uint16 connHandle = LINKDB_STATUS_UPDATE_REMOVED;
static uint16 led0Handle = 0;
static uint8  writesDone = 0;
static uint32 writeStart = 0;
static uint32 rttMin = 0xFFFFFFFF, rttMax = 0, rttSum = 0;

static void FoxRed_scanHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
static void FoxRed_connHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
static void FoxRed_gattHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);

static BLEAppUtil_EventHandler_t foxRedScan = {
    .handlerType = BLEAPPUTIL_GAP_SCAN_TYPE, .pEventHandler = FoxRed_scanHandler,
    .eventMask = BLEAPPUTIL_ADV_REPORT | BLEAPPUTIL_SCAN_DISABLED };
static BLEAppUtil_EventHandler_t foxRedConn = {
    .handlerType = BLEAPPUTIL_GAP_CONN_TYPE, .pEventHandler = FoxRed_connHandler,
    .eventMask = BLEAPPUTIL_LINK_ESTABLISHED_EVENT | BLEAPPUTIL_LINK_TERMINATED_EVENT };
static BLEAppUtil_EventHandler_t foxRedGatt = {
    .handlerType = BLEAPPUTIL_GATT_TYPE, .pEventHandler = FoxRed_gattHandler,
    .eventMask = 0xFFFFFFFF };   /* every GATT event, so the log shows what arrives */

static uint32 nowUs(void) { return ClockP_getSystemTicks() * ClockP_getSystemTickPeriod(); }

/* Does this advertising payload carry a complete or short local name equal to TARGET_NAME? */
static uint8 FoxRed_nameMatches(const uint8 *p, uint16 len)
{
    uint16 i = 0;
    while (i + 1 < len && p[i] != 0)
    {
        uint8 adLen = p[i], adType = p[i + 1];
        if ((adType == 0x09 || adType == 0x08) && adLen - 1 == sizeof(TARGET_NAME) - 1 &&
            i + 1 + adLen <= len && memcmp(&p[i + 2], TARGET_NAME, adLen - 1) == 0)
        {
            return 1;
        }
        i += adLen + 1;
    }
    return 0;
}

static void FoxRed_scan(void)
{
    const BLEAppUtil_ScanStart_t params = { .scanPeriod = 0, .scanDuration = 1000, .maxNumReport = 0 };
    targetFound = 0;
    bStatus_t s = BLEAppUtil_scanStart(&params);
    MenuModule_printf(LINE_STATUS, 0, "FOXRED scan for '" TARGET_NAME "' status %d", s);
}

static void FoxRed_write(uint8 value)
{
    attWriteReq_t req;
    req.pValue = GATT_bm_alloc(connHandle, ATT_WRITE_REQ, 1, NULL);
    if (req.pValue == NULL) { MenuModule_printf(LINE_STATUS, 0, "FOXRED write alloc failed"); return; }
    req.handle = led0Handle; req.len = 1; req.pValue[0] = value; req.sig = 0; req.cmd = 0;
    writeStart = nowUs();
    if (GATT_WriteCharValue(connHandle, &req, BLEAppUtil_getSelfEntity()) != SUCCESS)
    {
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        MenuModule_printf(LINE_STATUS, 0, "FOXRED write call failed");
    }
}

static void FoxRed_scanHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    BLEAppUtil_ScanEventData_t *scan = (BLEAppUtil_ScanEventData_t *)pMsgData;
    if (event == BLEAPPUTIL_ADV_REPORT && !targetFound)
    {
        BLEAppUtil_GapScan_Evt_AdvRpt_t *r = &scan->pBuf->pAdvReport;
        if (FoxRed_nameMatches(r->pData, r->dataLen))
        {
            targetFound = 1;
            memcpy(targetAddr, r->addr, B_ADDR_LEN);
            targetAddrType = r->addrType;
            MenuModule_printf(LINE_STATUS, 0, "FOXRED found %s RSSI %d", BLEAppUtil_convertBdAddr2Str(targetAddr), r->rssi);
            BLEAppUtil_scanStop();
        }
    }
    else if (event == BLEAPPUTIL_SCAN_DISABLED)
    {
        if (targetFound)
        {
            BLEAppUtil_ConnectParams_t cp = { .peerAddrType = targetAddrType, .phys = DEFAULT_INIT_PHY, .timeout = 1000 };
            memcpy(cp.pPeerAddress, targetAddr, B_ADDR_LEN);
            MenuModule_printf(LINE_STATUS, 0, "FOXRED connect status %d", BLEAppUtil_connect(&cp));
        }
        else
        {
            FoxRed_scan();   /* not seen in this 10 s window: scan again */
        }
    }
}

static void FoxRed_connHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    if (event == BLEAPPUTIL_LINK_ESTABLISHED_EVENT)
    {
        gapEstLinkReqEvent_t *e = (gapEstLinkReqEvent_t *)pMsgData;
        connHandle = e->connectionHandle;
        MenuModule_printf(LINE_STATUS, 0, "FOXRED connected, handle %d, interval %d x 1.25 ms", connHandle, e->connInterval);
        attReadByTypeReq_t req = { .startHandle = 1, .endHandle = 0xFFFF };
        req.type.len = ATT_UUID_SIZE;
        memcpy(req.type.uuid, led0Uuid, ATT_UUID_SIZE);
        MenuModule_printf(LINE_RESULT, 0, "FOXRED discover LED0 status %d",
                          GATT_DiscCharsByUUID(connHandle, &req, BLEAppUtil_getSelfEntity()));
    }
    else if (event == BLEAPPUTIL_LINK_TERMINATED_EVENT)
    {
        MenuModule_printf(LINE_STATUS, 0, "FOXRED disconnected after %d writes", writesDone);
        connHandle = LINKDB_STATUS_UPDATE_REMOVED;
    }
}

static void FoxRed_gattHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    gattMsgEvent_t *g = (gattMsgEvent_t *)pMsgData;
    MenuModule_printf(LINE_RESULT, 0, "FOXRED gatt method 0x%02x hdl %d status %d", g->method, g->connHandle, g->hdr.status);
    if (g->connHandle != connHandle) return;

    if (g->method == ATT_READ_BY_TYPE_RSP && g->msg.readByTypeRsp.numPairs > 0 && led0Handle == 0)
    {
        /* Each pair: declaration handle (2), properties (1), value handle (2), UUID (16) */
        uint8 *p = g->msg.readByTypeRsp.pDataList;
        led0Handle = BUILD_UINT16(p[3], p[4]);
        MenuModule_printf(LINE_RESULT, 0, "FOXRED LED0 value handle 0x%04x", led0Handle);
    }
    else if (g->method == ATT_READ_BY_TYPE_RSP && g->hdr.status == bleProcedureComplete && led0Handle && writesDone == 0)
    {
        /* One GATT procedure at a time: write only after discovery has finished */
        FoxRed_write(1);
    }
    else if (g->method == ATT_WRITE_RSP && led0Handle)
    {
        uint32 rtt = nowUs() - writeStart;
        rttSum += rtt; if (rtt < rttMin) rttMin = rtt; if (rtt > rttMax) rttMax = rtt;
        writesDone++;
        MenuModule_printf(LINE_RESULT, 0, "FOXRED write %d acked in %d us", writesDone, rtt);
        if (writesDone < TOGGLES)
        {
            FoxRed_write(writesDone & 1 ? 0 : 1);
        }
        else
        {
            MenuModule_printf(LINE_RESULT, 0, "FOXRED DONE %d writes, round trip min %d avg %d max %d us",
                              writesDone, rttMin, rttSum / writesDone, rttMax);
        }
    }
    else if (g->method == ATT_ERROR_RSP)
    {
        MenuModule_printf(LINE_RESULT, 0, "FOXRED ATT error req 0x%02x handle 0x%04x code 0x%02x",
                          g->msg.errorRsp.reqOpcode, g->msg.errorRsp.handle, g->msg.errorRsp.errCode);
    }
}

bStatus_t FoxRed_start(void)
{
    BLEAppUtil_registerEventHandler(&foxRedScan);
    BLEAppUtil_registerEventHandler(&foxRedConn);
    BLEAppUtil_registerEventHandler(&foxRedGatt);
    FoxRed_scan();
    return SUCCESS;
}
