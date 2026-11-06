/*
 * hid_gamepad.c: board glue for the FRL-GP1 port of ESP32-BLE-Gamepad (generic mode)
 * on the LP-EM-CC2340R5 with TI's BLE5-Stack (SimpleLink Low Power F3 SDK 9.21.00.36).
 * HID service layout after ports/ble-hid-remote (Fox Red Labs), whose attribute-table
 * style and security choices it follows; report map, report id and report bytes come
 * from the portable core (core/frl_gp_check.c, core/frl_gamepad.c).
 */
#include <string.h>
#include <stdarg.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/display/Display.h>
#include <FreeRTOS.h>
#include "ti_drivers_config.h"
#include "ti/ble/stack_util/icall/app/icall.h"
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/ble/services/battery/battery_server.h>
#include <ti/ble/services/dev_info/dev_info_service.h>
#include "hid_gamepad.h"
#include "frl_gp_check.h"

extern Display_Handle dispHandle;      /* opened by MenuModule_init (menu_module.c) */

/* ---------------------------------------------------------------- UUIDs */
#define HID_SERV_UUID        0x1812
#define HID_INFO_UUID        0x2A4A
#define HID_REPORT_MAP_UUID  0x2A4B
#define HID_CTRL_PT_UUID     0x2A4C
#define HID_REPORT_UUID      0x2A4D
#define HID_PROTO_MODE_UUID  0x2A4E
#define HID_REPORT_REF_UUID  0x2908

GATT_BT_UUID(hidServUUID,      HID_SERV_UUID);
GATT_BT_UUID(hidInfoUUID,      HID_INFO_UUID);
GATT_BT_UUID(hidReportMapUUID, HID_REPORT_MAP_UUID);
GATT_BT_UUID(hidCtrlPtUUID,    HID_CTRL_PT_UUID);
GATT_BT_UUID(hidReportUUID,    HID_REPORT_UUID);
GATT_BT_UUID(hidProtoModeUUID, HID_PROTO_MODE_UUID);
GATT_BT_UUID(hidReportRefUUID, HID_REPORT_REF_UUID);

/* ---------------------------------------------------- characteristic values */
static const gattAttrType_t hidService = { ATT_BT_UUID_SIZE, hidServUUID };

/* HID Information as BleGamepad sets it: setHidInfo(country 0x00, flags 0x01) with bcdHID 1.11 */
static uint8_t hidInfoProps = GATT_PROP_READ;
static uint8_t hidInfo[4] = { 0x11, 0x01, 0x00, 0x01 };

static uint8_t hidReportMapProps = GATT_PROP_READ;
/* Copy of the core's descriptor: the table's value pointer is const, so it must point at storage that exists at link time. */
#define HID_MAP_MAX 128
static uint8_t hidReportMap[HID_MAP_MAX];
static uint16_t hidReportMapLen;

static uint8_t hidCtrlPtProps = GATT_PROP_WRITE_NO_RSP;
static uint8_t hidCtrlPt = 0;

static uint8_t hidProtoModeProps = GATT_PROP_READ | GATT_PROP_WRITE_NO_RSP;
static uint8_t hidProtoMode = 1;

/* One input report; the report id is the library's default (3). */
static uint8_t hidReportProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t hidReport[FRL_GP_REPORT_MAX];
static uint8_t hidReportLen;
static gattCharCfg_t *hidReportCfg;
static uint8_t hidReportRef[2] = { 3, 1 }; /* id 3, type input */

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
#define HID_REPORT_VALUE_IDX 10

/* ----------------------------------------------------------- console glue */
/* Display_printf adds a line end, so frl_out() collects text up to each '\n'. */
static char lineBuf[300];
static unsigned lineLen;

void frl_out(const char *s)
{
    for (; *s; s++)
    {
        if (*s == '\n')
        {
            lineBuf[lineLen] = 0;
            if (dispHandle != NULL) { Display_printf(dispHandle, 0, 0, "%s", lineBuf); }
            lineLen = 0;
        }
        else if (*s != '\r' && lineLen < sizeof(lineBuf) - 1)
        {
            lineBuf[lineLen++] = *s;
        }
    }
}

/* Kernel tick: ClockP_getSystemTickPeriod() us per tick, from a hardware timer. */
uint32_t frl_micros(void)
{
    return (uint32_t)ClockP_getSystemTicks() * ClockP_getSystemTickPeriod();
}
uint32_t frl_uptime_ms(void)
{
    return (uint32_t)((uint64_t)ClockP_getSystemTicks() * ClockP_getSystemTickPeriod() / 1000U);
}
uint32_t frl_heap_free(void)
{
    return (uint32_t)xPortGetFreeHeapSize();
}

/* ------------------------------------------------------------- callbacks */
static bStatus_t HidGp_readAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                  uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                  uint16_t maxLen, uint8_t method)
{
    const uint8_t *src;
    uint16_t srcLen;
    (void)connHandle; (void)method;

    if (pAttr->type.len != ATT_BT_UUID_SIZE) { return ATT_ERR_ATTR_NOT_FOUND; }
    switch (BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]))
    {
    case HID_INFO_UUID:       src = hidInfo;       srcLen = sizeof(hidInfo); break;
    case HID_REPORT_MAP_UUID: src = hidReportMap;  srcLen = hidReportMapLen; break;
    case HID_PROTO_MODE_UUID: src = &hidProtoMode; srcLen = 1;               break;
    case HID_REPORT_UUID:     src = hidReport;     srcLen = hidReportLen;    break;
    case HID_REPORT_REF_UUID: src = hidReportRef;  srcLen = sizeof(hidReportRef); break;
    default: *pLen = 0; return ATT_ERR_ATTR_NOT_FOUND;
    }
    if (offset > srcLen) { return ATT_ERR_INVALID_OFFSET; }
    *pLen = (uint16_t)(srcLen - offset);
    if (*pLen > maxLen) { *pLen = maxLen; }
    memcpy(pValue, src + offset, *pLen);
    return SUCCESS;
}

static bStatus_t HidGp_writeAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                   uint8_t *pValue, uint16_t len, uint16_t offset, uint8_t method)
{
    (void)method;
    if (pAttr->type.len != ATT_BT_UUID_SIZE) { return ATT_ERR_ATTR_NOT_FOUND; }
    switch (BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]))
    {
    case GATT_CLIENT_CHAR_CFG_UUID:
        if (pAttr != &hidAttrTbl[HID_REPORT_VALUE_IDX + 1]) { return ATT_ERR_ATTR_NOT_FOUND; }
        return GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len, offset, GATT_CLIENT_CFG_NOTIFY);
    case HID_CTRL_PT_UUID:
        if (offset != 0 || len != 1) { return ATT_ERR_INVALID_VALUE_SIZE; }
        if (pValue[0] > 1) { return ATT_ERR_INVALID_VALUE; }
        hidCtrlPt = pValue[0];
        return SUCCESS;
    case HID_PROTO_MODE_UUID:
        if (offset != 0 || len != 1) { return ATT_ERR_INVALID_VALUE_SIZE; }
        if (pValue[0] > 1) { return ATT_ERR_INVALID_VALUE; }
        hidProtoMode = pValue[0];
        return SUCCESS;
    default:
        return ATT_ERR_ATTR_NOT_FOUND;
    }
}

static const gattServiceCBs_t hidCBs = { HidGp_readAttrCB, HidGp_writeAttrCB, NULL };

/* ------------------------------------------------------ example loop (500 ms) */
/* Gamepad.ino: phase A = press 5, 16, start; axes max; hat down-right. Phase B =
 * release 5 and start; hat centred; axes 0. One notification per phase (the
 * library's auto-report would send one per call). Runs in the BLE app task. */
static frl_gp_t loopGp;
static uint8_t loopPhase;

static void HidGp_notify(void)
{
    hidReportLen = frl_gp_pack(&loopGp, hidReport);
    GATTServApp_ProcessCharCfg(hidReportCfg, hidReport, FALSE, hidAttrTbl,
                               GATT_NUM_ATTRS(hidAttrTbl), INVALID_TASK_ID, HidGp_readAttrCB);
}

#ifdef FRL_LIVE
/* Live build (build.sh LIVE=1): the LaunchPad kit's BTN-1 and BTN-2 drive gamepad
 * buttons 1 and 2, read every 20 ms, one notification per change. Buttons pull
 * the pin low when pressed. */
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
static uint8_t liveLast = 0xFF;
static void HidGp_loopStep(char *p)
{
    (void)p;
    uint8_t now = (GPIO_read(CONFIG_GPIO_BUTTON_0_INPUT) == 0 ? 1 : 0)
                | (GPIO_read(CONFIG_GPIO_BUTTON_1_INPUT) == 0 ? 2 : 0);
    if (now == liveLast) return;
    liveLast = now;
    if (dispHandle != NULL) { Display_printf(dispHandle, 0, 0, "BTN b1=%d b2=%d report sent", now & 1, (now >> 1) & 1); }
    if (now & 1) frl_gp_press(&loopGp, FRL_BUTTON_1); else frl_gp_release(&loopGp, FRL_BUTTON_1);
    if (now & 2) frl_gp_press(&loopGp, FRL_BUTTON_2); else frl_gp_release(&loopGp, FRL_BUTTON_2);
    HidGp_notify();
}
#else
static void HidGp_loopStep(char *p)
{
    (void)p;
    if (loopPhase == 0)
    {
        frl_gp_press(&loopGp, FRL_BUTTON_5); frl_gp_press(&loopGp, FRL_BUTTON_16); frl_gp_pressStart(&loopGp);
        frl_gp_setAxes(&loopGp, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
        frl_gp_setHat1(&loopGp, FRL_HAT_DOWN_RIGHT);
    }
    else
    {
        frl_gp_release(&loopGp, FRL_BUTTON_5); frl_gp_releaseStart(&loopGp);
        frl_gp_setHat1(&loopGp, FRL_HAT_CENTERED);
        frl_gp_setAxes(&loopGp, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    loopPhase ^= 1;
    HidGp_notify();
}
#endif

static void HidGp_printStep(char *p)
{
    (void)p;
    frl_gp_check_print();
}

static ClockP_Struct loopClk, printClk;
static void HidGp_loopTick(uintptr_t a)  { (void)a; BLEAppUtil_invokeFunctionNoData(HidGp_loopStep); }
static void HidGp_printTick(uintptr_t a) { (void)a; BLEAppUtil_invokeFunctionNoData(HidGp_printStep); }

/* ------------------------------------------------------------------ start */
bStatus_t HidGamepad_start(void)
{
    bStatus_t status;
    uint8_t battery = 100;
    uint16_t appearance = 0x03C4;            /* HID_GAMEPAD, as BleGamepad sets */
    static const uint8_t pnp[DEVINFO_PNP_ID_LEN] = { 0x01, 0x02, 0xE5, 0xAB, 0xBB, 0x10, 0x01 }; /* src 1, vid 0xe502, pid 0xbbab, ver 0x0110 */
    ClockP_Params cp;
    const frl_gp_t *g;

    if (frl_gp_check_init() != 0) { return FAILURE; }
    g = frl_gp_check_gamepad();
    if (g->descLen > HID_MAP_MAX) { return FAILURE; }
    memcpy(hidReportMap, g->desc, g->descLen);
    hidReportMapLen = g->descLen;
    hidReportLen = g->reportSize;
    memset(hidReport, 0, sizeof(hidReport));
    {   /* the loop's own gamepad: default config, state zero (same defaults as the check) */
        frl_gp_config_t cfg;
        frl_gp_config_default(&cfg);
        frl_gp_begin(&loopGp, &cfg);
    }

    GGS_SetParameter(GGS_APPEARANCE_ATT, sizeof(appearance), &appearance);

    /* Device Information: the strings BleGamepadConfiguration sets by default */
    #ifdef FRL_LIVE
    DevInfo_setParameter(DEVINFO_MANUFACTURER_NAME, 12, "Fox Red Labs");
#else
    DevInfo_setParameter(DEVINFO_MANUFACTURER_NAME, 9, "Espressif");
#endif
    DevInfo_setParameter(DEVINFO_MODEL_NUMBER, 5, "1.0.0");
    DevInfo_setParameter(DEVINFO_SERIAL_NUMBER, 10, "0123456789");
    DevInfo_setParameter(DEVINFO_SOFTWARE_REV, 5, "1.0.0");
    DevInfo_setParameter(DEVINFO_FIRMWARE_REV, 5, "0.8.0");
    DevInfo_setParameter(DEVINFO_HARDWARE_REV, 5, "1.0.0");
    DevInfo_setParameter(DEVINFO_PNP_ID, DEVINFO_PNP_ID_LEN, (void *)pnp);

    status = BAS_addService();
    if (status != SUCCESS) { return status; }
    BAS_setParameter(BAS_LEVEL_ID, sizeof(battery), &battery);

    hidReportCfg = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
    if (hidReportCfg == NULL) { return bleMemAllocError; }
    GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, hidReportCfg);

    status = GATTServApp_RegisterService(hidAttrTbl, GATT_NUM_ATTRS(hidAttrTbl),
                                         GATT_MAX_ENCRYPT_KEY_SIZE, &hidCBs);
    if (status != SUCCESS) { return status; }

    ClockP_Params_init(&cp);
    #ifdef FRL_LIVE
    cp.period = 20000U / ClockP_getSystemTickPeriod();
#else
    cp.period = 500000U / ClockP_getSystemTickPeriod();
#endif
    cp.startFlag = true;
    ClockP_construct(&loopClk, HidGp_loopTick, cp.period, &cp);
    cp.period = 3000000U / ClockP_getSystemTickPeriod();
    ClockP_construct(&printClk, HidGp_printTick, cp.period, &cp);
    return SUCCESS;
}
