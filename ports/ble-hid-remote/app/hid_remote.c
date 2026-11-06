/*
 * hid_remote.c: HID over GATT media remote for TI's BLE5-Stack (SimpleLink Low
 * Power F3 SDK 9.21.00.36), LP-EM-CC2340R5. Fox Red Labs, ports/ble-hid-remote.
 *
 * Neither TI Bluetooth SDK ships a HID profile, so this file is the HID service
 * (0x1812) written by hand in the attribute-table style of TI's own services
 * (simple_gatt_profile.c, battery_server.c). The Battery service is TI's
 * (services/battery); Device Information with PnP ID is TI's (services/dev_info,
 * started by the example).
 *
 * Security: every HID value needs an encrypted link (GATT_PERMIT_ENCRYPT_*), as
 * the HID over GATT Profile asks (security mode 1, level 2). The board does not
 * ask to pair: the host's first read of an HID value is refused with Insufficient
 * Authentication and the host pairs, as Apple's accessory guidelines ask. The bond
 * manager in SysConfig: Just Works, LE Secure Connections allowed, bonding on, so
 * the host pairs once and reconnects encrypted. Battery and Device Information
 * stay readable without pairing.
 */
#include <string.h>
#include <ti/drivers/apps/Button.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_drivers_config.h"
#include "ti/ble/stack_util/icall/app/icall.h"
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <ti/ble/services/battery/battery_server.h>
#include <app_main.h>
#include "hid_remote.h"

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
/* Consumer control, report ID 1, one byte: five one-bit buttons and three
 * bits of padding. host/report_map.py parses this array from this file and
 * decodes it, so keep it a plain list of hex bytes. */
static const uint8_t hidReportMap[] =
{
    0x05, 0x0C,       /* Usage Page (Consumer)               */
    0x09, 0x01,       /* Usage (Consumer Control)            */
    0xA1, 0x01,       /* Collection (Application)            */
    0x85, 0x01,       /*   Report ID (1)                     */
    0x15, 0x00,       /*   Logical Minimum (0)               */
    0x25, 0x01,       /*   Logical Maximum (1)               */
    0x75, 0x01,       /*   Report Size (1)                   */
    0x95, 0x05,       /*   Report Count (5)                  */
    0x09, 0xCD,       /*   Usage (Play/Pause)                */
    0x09, 0xB5,       /*   Usage (Scan Next Track)           */
    0x09, 0xB6,       /*   Usage (Scan Previous Track)       */
    0x09, 0xE9,       /*   Usage (Volume Increment)          */
    0x09, 0xEA,       /*   Usage (Volume Decrement)          */
    0x81, 0x02,       /*   Input (Data, Variable, Absolute)  */
    0x95, 0x03,       /*   Report Count (3)                  */
    0x81, 0x01,       /*   Input (Constant): padding         */
    0xC0              /* End Collection                      */
};

/* ------------------------------------------------------- characteristic values */
static const gattAttrType_t hidService = { ATT_BT_UUID_SIZE, hidServUUID };

/* HID Information: bcdHID 1.11, country 0, flags 0x02 (normally connectable) */
static uint8_t hidInfoProps = GATT_PROP_READ;
static uint8_t hidInfo[4] = { 0x11, 0x01, 0x00, 0x02 };

static uint8_t hidReportMapProps = GATT_PROP_READ;

/* HID Control Point: 0 suspend, 1 exit suspend. Stored, nothing else to do. */
static uint8_t hidCtrlPtProps = GATT_PROP_WRITE_NO_RSP;
static uint8_t hidCtrlPt = 0;

/* Protocol Mode: 1 = report protocol (the only one this device has). */
static uint8_t hidProtoModeProps = GATT_PROP_READ | GATT_PROP_WRITE_NO_RSP;
static uint8_t hidProtoMode = 1;

/* Input report: one byte of HID_CC_* bits, notified on every button edge. */
static uint8_t hidReportProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t hidReport = 0;
static gattCharCfg_t *hidReportCfg;
static uint8_t hidReportRef[2] = { HID_REPORT_ID_CC, HID_REPORT_TYPE_IN };

/* ------------------------------------------------------------ attribute table */
static gattAttribute_t hidAttrTbl[] =
{
    GATT_BT_ATT( primaryServiceUUID, GATT_PERMIT_READ,          (uint8_t *) &hidService ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidInfoProps ),
    GATT_BT_ATT( hidInfoUUID,        GATT_PERMIT_ENCRYPT_READ,  hidInfo ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidReportMapProps ),
    GATT_BT_ATT( hidReportMapUUID,   GATT_PERMIT_ENCRYPT_READ,  (uint8_t *) hidReportMap ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidCtrlPtProps ),
    GATT_BT_ATT( hidCtrlPtUUID,      GATT_PERMIT_ENCRYPT_WRITE, &hidCtrlPt ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidProtoModeProps ),
    GATT_BT_ATT( hidProtoModeUUID,   GATT_PERMIT_ENCRYPT_READ | GATT_PERMIT_ENCRYPT_WRITE, &hidProtoMode ),

    GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,          &hidReportProps ),
    GATT_BT_ATT( hidReportUUID,      GATT_PERMIT_ENCRYPT_READ,  &hidReport ),
    GATT_BT_ATT( clientCharCfgUUID,  GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE, (uint8_t *) &hidReportCfg ),
    GATT_BT_ATT( hidReportRefUUID,   GATT_PERMIT_ENCRYPT_READ,  hidReportRef ),
};

/* Index of the input report value in hidAttrTbl, for the CCC write check. */
#define HID_REPORT_VALUE_IDX 10

/* ---------------------------------------------------------------- logging */
static uint32_t hidLogCount = 0;

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
    case FRL_HID_REPORT_MAP_UUID: src = hidReportMap; srcLen = sizeof(hidReportMap); break;
    case FRL_HID_PROTO_MODE_UUID: src = &hidProtoMode; srcLen = 1;                   break;
    case FRL_HID_REPORT_UUID:     src = &hidReport;   srcLen = 1;                    break;
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

    if (src == hidReportMap && method != ATT_HANDLE_VALUE_NOTI)
    {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0,
                          "HID map read off=%u len=%u t=%u", offset, *pLen, HidRemote_ms());
    }
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
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE2, 0,
                          "HID report CCC=%u st=%u t=%u", (len > 0) ? pValue[0] : 0xFF, st, HidRemote_ms());
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
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE3, 0, "HID control point=%u t=%u", hidCtrlPt, HidRemote_ms());
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
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE3, 0, "HID protocol mode=%u t=%u", hidProtoMode, HidRemote_ms());
        return SUCCESS;
    default:
        return ATT_ERR_ATTR_NOT_FOUND;
    }
}

static const gattServiceCBs_t hidCBs = { HidRemote_readAttrCB, HidRemote_writeAttrCB, NULL };

/* --------------------------------------------------------------- buttons */
/* Runs in the BLE app task (moved there by BLEAppUtil_invokeFunctionNoData),
 * so calling the GATT server here is safe. One notification per button edge:
 * press sends the bit, release sends 0, which is what a host expects from a
 * consumer-control key. */
static void HidRemote_send(uint8_t bits)
{
    bStatus_t st;
    hidReport = bits;
    st = GATTServApp_ProcessCharCfg(hidReportCfg, &hidReport, FALSE, hidAttrTbl,
                                    GATT_NUM_ATTRS(hidAttrTbl), INVALID_TASK_ID,
                                    HidRemote_readAttrCB);
    hidLogCount++;
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0, "HID tx n=%u bits=0x%02x st=%u t=%u",
                      hidLogCount, bits, st, HidRemote_ms());
}

static uint8_t hidButtonsDown = 0;

static void HidRemote_leftDown(char *p)  { (void)p; hidButtonsDown |= HID_CC_PLAY_PAUSE;  HidRemote_send(hidButtonsDown); }
static void HidRemote_leftUp(char *p)    { (void)p; hidButtonsDown &= ~HID_CC_PLAY_PAUSE; HidRemote_send(hidButtonsDown); }
static void HidRemote_rightDown(char *p) { (void)p; hidButtonsDown |= HID_CC_NEXT;        HidRemote_send(hidButtonsDown); }
static void HidRemote_rightUp(char *p)   { (void)p; hidButtonsDown &= ~HID_CC_NEXT;       HidRemote_send(hidButtonsDown); }

static Button_Handle hidLeft;
static Button_Handle hidRight;

/* Button driver callback: interrupt context, so only hand the work to the app task. */
static void HidRemote_buttonCB(Button_Handle h, Button_EventMask ev)
{
    if (h == hidLeft)
    {
        if (ev & Button_EV_PRESSED)  { BLEAppUtil_invokeFunctionNoData(HidRemote_leftDown); }
        if (ev & Button_EV_RELEASED) { BLEAppUtil_invokeFunctionNoData(HidRemote_leftUp); }
    }
    else if (h == hidRight)
    {
        if (ev & Button_EV_PRESSED)  { BLEAppUtil_invokeFunctionNoData(HidRemote_rightDown); }
        if (ev & Button_EV_RELEASED) { BLEAppUtil_invokeFunctionNoData(HidRemote_rightUp); }
    }
}

/* ------------------------------------------------------------------ start */
bStatus_t HidRemote_start(void)
{
    bStatus_t status;
    uint8_t battery = 100;                  /* USB powered: report full */
    uint16_t appearance = GAP_APPEARE_GENERIC_HID;
    Button_Params bp;

    /* GAP service Appearance characteristic, to match the advertisement. */
    GGS_SetParameter(GGS_APPEARANCE_ATT, sizeof(appearance), &appearance);

    status = BAS_addService();
    if (status != SUCCESS)
    {
        return status;
    }
    BAS_setParameter(BAS_LEVEL_ID, sizeof(battery), &battery);

    hidReportCfg = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
    if (hidReportCfg == NULL)
    {
        return bleMemAllocError;
    }
    GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, hidReportCfg);

    status = GATTServApp_RegisterService(hidAttrTbl, GATT_NUM_ATTRS(hidAttrTbl),
                                         GATT_MAX_ENCRYPT_KEY_SIZE, &hidCBs);
    if (status != SUCCESS)
    {
        return status;
    }

    Button_Params_init(&bp);
    bp.buttonEventMask = Button_EV_PRESSED | Button_EV_RELEASED;
    bp.buttonCallback = HidRemote_buttonCB;
    hidLeft = Button_open(CONFIG_BUTTON_0, &bp);
    hidRight = Button_open(CONFIG_BUTTON_1, &bp);

    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE4, 0, "FRL Remote: HID up, %u attrs, map %u B, buttons %s t=%u",
                      (unsigned)GATT_NUM_ATTRS(hidAttrTbl), (unsigned)sizeof(hidReportMap),
                      (hidLeft && hidRight) ? "ok" : "FAIL", HidRemote_ms());
    return (hidLeft && hidRight) ? SUCCESS : FAILURE;
}
