/*
 * gamepad.c: glue for the ESP32-BLE-Gamepad generic-mode core on the LP-EM-CC2340R5
 * (TI BLE5-Stack, F3 SDK 9.21). The HID-over-GATT attribute table is adapted from
 * Fox Red Labs ports/ble-hid-remote/app/hid_remote.c (report map, input report with
 * CCC and Report Reference, protocol mode, HID info, control point). The report map
 * and the report bytes come from gp_core.c.
 */
#include <string.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_drivers_config.h"
#include "ti/ble/stack_util/icall/app/icall.h"
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <ti/ble/services/battery/battery_server.h>
#include <app_main.h>
#include <ti/display/Display.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "gp_core.h"
#include "gamepad.h"

/* ---------------------------------------------------------------- UUIDs */
#define FRL_HID_SERV_UUID        0x1812
#define FRL_HID_INFO_UUID        0x2A4A
#define FRL_HID_REPORT_MAP_UUID  0x2A4B
#define FRL_HID_CTRL_PT_UUID     0x2A4C
#define FRL_HID_REPORT_UUID      0x2A4D
#define FRL_HID_PROTO_MODE_UUID  0x2A4E
#define FRL_REPORT_REF_UUID      0x2908

GATT_BT_UUID(hidServUUID,      FRL_HID_SERV_UUID);
GATT_BT_UUID(hidInfoUUID,      FRL_HID_INFO_UUID);
GATT_BT_UUID(hidReportMapUUID, FRL_HID_REPORT_MAP_UUID);
GATT_BT_UUID(hidCtrlPtUUID,    FRL_HID_CTRL_PT_UUID);
GATT_BT_UUID(hidReportUUID,    FRL_HID_REPORT_UUID);
GATT_BT_UUID(hidProtoModeUUID, FRL_HID_PROTO_MODE_UUID);
GATT_BT_UUID(hidReportRefUUID, FRL_REPORT_REF_UUID);

/* ------------------------------------------------------------ report map */
static gp_t gp;
static uint8_t hidReportMap[GP_MAX_DESC];
static uint16_t hidReportMapLen;
static uint8_t hidReport[32];
static uint8_t hidReportLen;

/* ------------------------------------------------------- characteristic values */
static const gattAttrType_t hidService = { ATT_BT_UUID_SIZE, hidServUUID };

/* HID Information: bcdHID 1.11, country 0, flags 0x02 (normally connectable) */
static uint8_t hidInfoProps = GATT_PROP_READ;
static uint8_t hidInfo[4] = { 0x11, 0x01, 0x00, 0x01 };  /* upstream setHidInfo(0x00, 0x01) */

static uint8_t hidReportMapProps = GATT_PROP_READ;

/* HID Control Point: 0 suspend, 1 exit suspend. Stored, nothing else to do. */
static uint8_t hidCtrlPtProps = GATT_PROP_WRITE_NO_RSP;
static uint8_t hidCtrlPt = 0;

/* Protocol Mode: 1 = report protocol (the only one this device has). */
static uint8_t hidProtoModeProps = GATT_PROP_READ | GATT_PROP_WRITE_NO_RSP;
static uint8_t hidProtoMode = 1;

/* Input report: one byte of HID_CC_* bits, notified on every button edge. */
static uint8_t hidReportProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static gattCharCfg_t *hidReportCfg;
static uint8_t hidReportRef[2] = { 3, 1 }  /* report id 3 (library default), input */;

/* ------------------------------------------------------------ attribute table */
static gattAttribute_t hidAttrTbl[] =
{
    GATT_BT_ATT( primaryServiceUUID, GATT_PERMIT_READ,          (uint8_t *) &hidService ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidInfoProps ),
    GATT_BT_ATT( hidInfoUUID,        GATT_PERMIT_ENCRYPT_READ,  hidInfo ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidReportMapProps ),
    GATT_BT_ATT( hidReportMapUUID,   GATT_PERMIT_ENCRYPT_READ,  hidReportMap ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidCtrlPtProps ),
    GATT_BT_ATT( hidCtrlPtUUID,      GATT_PERMIT_ENCRYPT_WRITE, &hidCtrlPt ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidProtoModeProps ),
    GATT_BT_ATT( hidProtoModeUUID,   GATT_PERMIT_ENCRYPT_READ | GATT_PERMIT_ENCRYPT_WRITE, &hidProtoMode ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidReportProps ),
    GATT_BT_ATT( hidReportUUID,      GATT_PERMIT_ENCRYPT_READ,  hidReport ),
    GATT_BT_ATT( clientCharCfgUUID,  GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE, (uint8_t *) &hidReportCfg ),
    GATT_BT_ATT( hidReportRefUUID,   GATT_PERMIT_ENCRYPT_READ,  hidReportRef ),
};

/* Index of the input report value in hidAttrTbl, for the CCC write check. */
#define HID_REPORT_VALUE_IDX 10

/* ---------------------------------------------------------------- logging */
static volatile uint8_t gpNotifyEnabled = 0;

static uint32_t HidRemote_ms(void)
{
    return (uint32_t)((uint64_t)ClockP_getSystemTicks() * ClockP_getSystemTickPeriod() / 1000U);
}

/* ------------------------------------------------------------- callbacks */
static bStatus_t HidRemote_readAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                      uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                      uint16_t maxLen, uint8_t method)
{
    const uint8_t *src;
    uint16_t srcLen;
    (void)connHandle;

    if (pAttr->type.len != ATT_BT_UUID_SIZE)
    {
        return ATT_ERR_ATTR_NOT_FOUND;
    }
    switch (BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]))
    {
    case FRL_HID_INFO_UUID:       src = hidInfo;      srcLen = sizeof(hidInfo);      break;
    case FRL_HID_REPORT_MAP_UUID: src = hidReportMap; srcLen = hidReportMapLen; break;
    case FRL_HID_PROTO_MODE_UUID: src = &hidProtoMode; srcLen = 1;                   break;
    case FRL_HID_REPORT_UUID:     src = hidReport;    srcLen = hidReportLen;                    break;
    case FRL_REPORT_REF_UUID:     src = hidReportRef; srcLen = sizeof(hidReportRef); break;
    default:
        *pLen = 0;
        return ATT_ERR_ATTR_NOT_FOUND;
    }

    /* The report map (33 bytes) is longer than ATT_MTU - 1 = 22 at the default
     * MTU of 23, so the host reads the rest with Read Blob. */
    if (offset > srcLen)
    {
        return ATT_ERR_INVALID_OFFSET;
    }
    *pLen = (uint16_t)(srcLen - offset);
    if (*pLen > maxLen)
    {
        *pLen = maxLen;
    }
    memcpy(pValue, src + offset, *pLen);

    return SUCCESS;
}

static bStatus_t HidRemote_writeAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t len, uint16_t offset,
                                       uint8_t method)
{
    (void)method;
    if (pAttr->type.len != ATT_BT_UUID_SIZE)
    {
        return ATT_ERR_ATTR_NOT_FOUND;
    }
    switch (BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]))
    {
    case GATT_CLIENT_CHAR_CFG_UUID:
    {
        bStatus_t st;
        if (pAttr != &hidAttrTbl[HID_REPORT_VALUE_IDX + 1])
        {
            return ATT_ERR_ATTR_NOT_FOUND;
        }
        st = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len, offset,
                                            GATT_CLIENT_CFG_NOTIFY);
        gpNotifyEnabled = (len > 0) && (pValue[0] & 1);
        return st;
    }
    case FRL_HID_CTRL_PT_UUID:
        if (offset != 0 || len != 1)
        {
            return ATT_ERR_INVALID_VALUE_SIZE;
        }
        if (pValue[0] > 1)
        {
            return ATT_ERR_INVALID_VALUE;
        }
        hidCtrlPt = pValue[0];
        return SUCCESS;
    case FRL_HID_PROTO_MODE_UUID:
        if (offset != 0 || len != 1)
        {
            return ATT_ERR_INVALID_VALUE_SIZE;
        }
        if (pValue[0] > 1)
        {
            return ATT_ERR_INVALID_VALUE;
        }
        hidProtoMode = pValue[0];
        return SUCCESS;
    default:
        return ATT_ERR_ATTR_NOT_FOUND;
    }
}

static const gattServiceCBs_t hidCBs = { HidRemote_readAttrCB, HidRemote_writeAttrCB, NULL };


/* ------------------------------------------------------------ send (BLE app task) */
static void Gamepad_sendNow(char *unused)
{
    uint8_t m[32];
    int n = gp_pack(&gp, m);
    (void)unused;
    memcpy(hidReport, m, n);
    hidReportLen = (uint8_t)n;
    if (gpNotifyEnabled)
    {
        GATTServApp_ProcessCharCfg(hidReportCfg, hidReport, FALSE, hidAttrTbl,
                                   GATT_NUM_ATTRS(hidAttrTbl), INVALID_TASK_ID,
                                   HidRemote_readAttrCB);
    }
}

/* ------------------------------------------------------------ console block */
static Display_Handle disp;
static char blockLines[4][120];   /* RPT1..RPT4 */
static char mapLine[GP_MAX_DESC * 2 + 32];
static uint32_t descUs, packUs;

static void hexline(char *out, const char *tag, const uint8_t *p, int n)
{
    int o = sprintf(out, "%s len=%d ", tag, n);
    for (int i = 0; i < n; i++) { o += sprintf(out + o, "%02x", p[i]); }
}

/* 1 tick = ClockP_getSystemTickPeriod() microseconds (system clock, 10 us on F3) */
static uint32_t nowUs(void)
{
    return (uint32_t)((uint64_t)ClockP_getSystemTicks() * ClockP_getSystemTickPeriod());
}

static void Gamepad_runChecks(void)
{
    gp_config_t c;
    uint8_t m[32];
    int n;
    uint32_t t;
    gp_config_default(&c);
    gp_begin(&gp, &c);
    hexline(mapLine, "MAP", gp.desc, gp.desc_len);
    n = gp_pack(&gp, m); hexline(blockLines[0], "RPT1", m, n);
    gp_press(&gp, 5); gp_press(&gp, 16); gp_press_start(&gp);
    gp_set_axes(&gp, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
    gp_set_hat1(&gp, GP_HAT_DOWN_RIGHT);
    n = gp_pack(&gp, m); hexline(blockLines[1], "RPT2", m, n);
    gp_release(&gp, 5); gp_release_start(&gp); gp_set_hat1(&gp, GP_HAT_CENTERED);
    gp_set_axes(&gp, 0, 0, 0, 0, 0, 0, 0, 0);
    n = gp_pack(&gp, m); hexline(blockLines[2], "RPT3", m, n);
    gp_press(&gp, 1); gp_press(&gp, 8); gp_press(&gp, 9); gp_press_select(&gp);
    gp_set_axes(&gp, -32768, -1, 1, 255, 256, -256, 12345, -12345);
    gp_set_hat1(&gp, GP_HAT_UP_LEFT);
    n = gp_pack(&gp, m); hexline(blockLines[3], "RPT4", m, n);

    t = nowUs();
    for (int i = 0; i < 200; i++) { gp.desc_len = 0; gp_build_descriptor(&gp); }
    descUs = (nowUs() - t) / 200;
    t = nowUs();
    for (int i = 0; i < 1000; i++) { gp_pack(&gp, m); }
    packUs = (nowUs() - t) / 1000;

    /* leave the gamepad idle and publish the map for GATT */
    gp_reset_buttons(&gp); gp.special_bits = 0; gp_set_hat1(&gp, GP_HAT_CENTERED);
    gp_set_axes(&gp, 0, 0, 0, 0, 0, 0, 0, 0);
    memcpy(hidReportMap, gp.desc, gp.desc_len);
    hidReportMapLen = (uint16_t)gp.desc_len;
    hidReportLen = (uint8_t)gp_pack(&gp, hidReport);
}

static void Gamepad_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    (void)arg;
    for (;;)
    {
        Display_printf(disp, 0, 0, "FRL-GP1 begin");
        Display_printf(disp, 0, 0, "INFO heap_free=%u uptime_ms=%u",
                       (unsigned)xPortGetFreeHeapSize(), (unsigned)HidRemote_ms());
        Display_printf(disp, 0, 0, "TIME desc_us=%u pack_us=%u", (unsigned)descUs, (unsigned)packUs);
        Display_printf(disp, 0, 0, "%s", mapLine);
        for (int i = 0; i < 4; i++) { Display_printf(disp, 0, 0, "%s", blockLines[i]); }
        Display_printf(disp, 0, 0, "FRL-GP1 end");
        /* 3 s block period, with the example's 500 ms press/release loop in between */
        for (int k = 0; k < 3; k++)
        {
            gp_press(&gp, 5); gp_press(&gp, 16); gp_press_start(&gp);
            gp_set_axes(&gp, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
            gp_set_hat1(&gp, GP_HAT_DOWN_RIGHT);
            BLEAppUtil_invokeFunctionNoData(Gamepad_sendNow);
            vTaskDelayUntil(&last, pdMS_TO_TICKS(500));
            gp_release(&gp, 5); gp_release_start(&gp); gp_set_hat1(&gp, GP_HAT_CENTERED);
            gp_set_axes(&gp, 0, 0, 0, 0, 0, 0, 0, 0);
            BLEAppUtil_invokeFunctionNoData(Gamepad_sendNow);
            vTaskDelayUntil(&last, pdMS_TO_TICKS(500));
        }
    }
}

bStatus_t Gamepad_start(void)
{
    bStatus_t status;
    uint8_t battery = 100;
    uint16_t appearance = 0x03C4;           /* HID_GAMEPAD, as upstream setAppearance */

    Display_init();
    disp = Display_open(Display_Type_UART, NULL);
    Gamepad_runChecks();

    GGS_SetParameter(GGS_APPEARANCE_ATT, sizeof(appearance), &appearance);
    status = BAS_addService();
    if (status != SUCCESS) { return status; }
    BAS_setParameter(BAS_LEVEL_ID, sizeof(battery), &battery);

    hidReportCfg = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
    if (hidReportCfg == NULL) { return bleMemAllocError; }
    GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, hidReportCfg);
    status = GATTServApp_RegisterService(hidAttrTbl, GATT_NUM_ATTRS(hidAttrTbl),
                                         GATT_MAX_ENCRYPT_KEY_SIZE, &hidCBs);
    if (status != SUCCESS) { return status; }
    xTaskCreate(Gamepad_task, "gp", 512, NULL, 1, NULL);
    return SUCCESS;
}
