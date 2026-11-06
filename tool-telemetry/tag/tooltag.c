/* Tool tag: reads the tool MCU's UART lines and advertises the tool's telemetry on two advertising sets at once.
 * Set 1 is TI's basic_ble peripheral set (legacy, connectable, scannable); this file reloads its data. Set 2 is created
 * here (extended, non-connectable, non-scannable). Formats: ../PROTOCOL.md, codec: ../proto/tt_proto.c.
 *
 * Threads: ttUartTask reads lines and updates the pending snapshot; ttTickTask wakes every 250 ms and asks the BLE
 * thread (BLEAppUtil_invokeFunctionNoData) to run TT_tick, which is the only place that touches advertising or NV.
 *
 * Demo mode, for a two-board demo with no tool MCU: while no tool line has arrived for TT_LINK_TIMEOUT_S, the
 * LaunchPad kit's left button (BTN-1) is the trigger (hold it and the "motor" runs) and the right button (BTN-2) is a jam
 * (an overcurrent trip that stops the run). The tool is then named BTN-DEMO and trips carry 0 mA (nothing measured). */
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/apps/Button.h>
#include "ti_drivers_config.h"
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "tt_proto.h"
#include "tt_model.h"
#include "tt_mdl.h"
#include "tt_key.h"          /* written by apply.py into the build copy only: the fleet signing key */
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"
#include "fd_frame.h"
#include "tooltag.h"

#define TT_FW              2   /* 2: models installed over the air, button counters kept across restarts */
#define TT_LINK_TIMEOUT_S  15
#define TT_NV_ID           (BLE_NVID_CUST_START)
#define TT_NV_MAGIC        0x54544736u   /* "TTG6": demo counters, model floor, command counter, lease rule and state */
#define TT_NV_MDL0         (BLE_NVID_CUST_START + 1)   /* installed models, one item per tree */
#define TT_NV_MDL1         (BLE_NVID_CUST_START + 2)

extern uint8_t peripheralAdvHandles[];

typedef struct {
    uint32_t   magic;
    uint16_t   boots;
    uint16_t   next_ev;
    uint8_t    nev;
    uint8_t    pad[3];
    tt_event_t ev[TT_MAX_EVENTS];    /* newest first */
    uint32_t   demo_trig, demo_run;  /* the button demo is the tool here, so its lifetime counts survive a restart */
    uint16_t   demo_oc;
    uint16_t   mdl_floor[2];         /* newest model version installed per tree: an older signed one is refused */
    uint32_t   cmd_ctr;              /* counter of the last signed command passed on: a command must carry a larger one */
    uint8_t    lease_min;            /* lease rule: lock the tool when no new lease came for this many minutes; 0 off */
    uint8_t    lease_locked;         /* 1: the lease ran out and the tool is locked; a restart keeps it locked */
} tt_nv_t;

/* Written by the UART task, read by TT_tick, guarded by a critical section. */
static tt_tool_t    g_pending;
static uint8_t      g_new;                   /* a good line arrived since the last snapshot */
static uint8_t      g_ev_new;                /* events to log, in arrival order */
static tt_line_t    g_ev_q[4];
static uint32_t     g_last_line_s = 0xFFFFFFFFu;
static uint16_t     g_rx_ok, g_rx_gap;
static uint8_t      g_rx_bad;

/* Pull summaries (TP lines): numbered here, newest first, kept in RAM only. */
static tt_line_t    g_pull_q[2];
static volatile uint8_t g_pull_new;
static tt_pull_t    g_pulls[TT_MAX_PULLS];
static uint8_t      g_npull;
static uint16_t     g_pull_next = 1;

/* Generic field-data lines ($FD, portability/fielddata): any device, any app. The latest line wins. */
static fd_fields_t  g_fd_pending, g_fd_cur;
static volatile uint8_t g_fd_new;
static uint8_t      g_fd_mode;                /* 1 once a $FD line has arrived: advertise the generic frame */

/* Models installed over the air (proto/tt_mdl.h). BLE thread only: the GATT callbacks and TT_tick both run there. */
static tt_mdl_store_t g_mdl;
static volatile uint8_t g_mdl_check;         /* bit k: slot k waits for its MAC check; bit 4+k: it came from flash */
/* One verdict per slot, overwritten by the newest check, never merged: 0 none, 1 pass, 2 fail; 0x80 the slot came from
   flash. It counts only for the generation it was computed on. */
static volatile uint8_t  g_mdl_vres[2];
static volatile uint16_t g_mdl_vgen[2];
static uint8_t      g_mdl_restore;           /* bit k: an upload into slot k failed or stalled: bring back the saved model */
static uint32_t     g_mdl_last_write;        /* uptime of the last write, to drop an upload left open */

/* BLE thread only. */
static tt_tool_t    g_cur;
static tt_nv_t      g_nv;
static uint8_t      g_nv_dirty;
static uint8_t      g_seq;
static uint8_t      g_ext_handle = 0xFF;
static uint8_t      g_legacy_buf[31];
static uint8_t      g_ext_buf[TT_EXT_MAX];
static uint8_t      g_scan_rsp[2 + 16];
static uint8_t      g_scan_rsp_len;
static uint8_t      g_dirty_legacy = 1, g_dirty_ext = 1, g_scan_rsp_pending = 1;
static uint8_t      g_last_flags = 0xFF;
static uint8_t      g_phase;
static volatile uint8_t g_status_due;
static char         g_name[17];
static bStatus_t    g_ext_status = 0xFF;

static UART2_Handle g_uart;
static Button_Handle g_btn[2];

/* demo mode: set by the button callbacks, consumed by TT_tick */
static volatile uint8_t g_btn_trig, g_btn_jam_req;
static uint8_t  g_demo_running;
static uint32_t g_demo_ms, g_demo_pull_ms, g_demo_run_ms;

static void buttonCb(Button_Handle h, Button_EventMask ev)
{
    if (h == g_btn[0]) {
        if (ev & Button_EV_PRESSED) g_btn_trig = 1;
        if (ev & Button_EV_RELEASED) g_btn_trig = 0;
    } else if (ev & Button_EV_PRESSED) {
        g_btn_jam_req = 1;
    }
}

static uint32_t uptime_s(void);

/* GATT service: the hub writes model packages to FAB1 and reads FAB2 for the result. */
#define TT_SVC_UUID 0xFAB0
#define TT_MDL_UUID 0xFAB1
#define TT_STA_UUID 0xFAB2
static GATT_BT_UUID(ttSvcUUID, TT_SVC_UUID);
static GATT_BT_UUID(ttMdlUUID, TT_MDL_UUID);
static GATT_BT_UUID(ttStaUUID, TT_STA_UUID);
static const gattAttrType_t ttSvc = { ATT_BT_UUID_SIZE, ttSvcUUID };
static uint8 ttMdlProps = GATT_PROP_WRITE, ttStaProps = GATT_PROP_READ;
static uint8 ttMdlVal, ttStaVal[8], ttCmdVal, ttCmdSta[5];
static GATT_BT_UUID(ttCmdUUID, 0xFAB3);
static GATT_BT_UUID(ttCstUUID, 0xFAB4);
static uint8 ttCmdProps = GATT_PROP_WRITE, ttCstProps = GATT_PROP_READ;
/* Signed commands (stage two): the GATT callback only copies; the tick task checks the MAC and talks to the tool. */
static uint8_t           g_cmd_buf[TT_CMD_LEN];
static volatile uint8_t  g_cmd_state;        /* 1: a command waits for its check (tick task) */
static volatile uint8_t  g_cmd_done;         /* 1: checked, result in g_cmd_r (BLE thread logs and saves it) */
static uint8_t           g_cmd_r = 0xFF, g_cmd_c, g_cmd_d;
static char              g_cmd_line[42];     /* the $TC line, made on the tick task, sent once its counter is in flash */
static volatile uint8_t  g_cmd_send;         /* 1: counter saved, the tick task may send the line (one writer, no interleaving) */
static uint32_t          g_lease_until;      /* uptime when the lease runs out; 0 no lease rule */
static uint8_t           g_lease_locked;     /* 1: this tag locked the tool because the lease ran out */
static const char TC_LOCK[] = "\r\n$TC,cmd=lock*67\r\n", TC_UNLOCK[] = "\r\n$TC,cmd=unlock*7C\r\n";
static uint32_t          g_cmd_n;
static uint8_t           g_addr[6];
static gattAttribute_t ttAttrTbl[] = {
    GATT_BT_ATT(primaryServiceUUID, GATT_PERMIT_READ, (uint8 *)&ttSvc),
    GATT_BT_ATT(characterUUID, GATT_PERMIT_READ, &ttMdlProps),
    GATT_BT_ATT(ttMdlUUID, GATT_PERMIT_WRITE, &ttMdlVal),
    GATT_BT_ATT(characterUUID, GATT_PERMIT_READ, &ttStaProps),
    GATT_BT_ATT(ttStaUUID, GATT_PERMIT_READ, ttStaVal),
    GATT_BT_ATT(characterUUID, GATT_PERMIT_READ, &ttCmdProps),
    GATT_BT_ATT(ttCmdUUID, GATT_PERMIT_WRITE, &ttCmdVal),
    GATT_BT_ATT(characterUUID, GATT_PERMIT_READ, &ttCstProps),
    GATT_BT_ATT(ttCstUUID, GATT_PERMIT_READ, ttCmdSta),
};

static bStatus_t ttReadCB(uint16_t conn, gattAttribute_t *a, uint8_t *v, uint16_t *len, uint16_t off, uint16_t max, uint8_t method)
{
    (void)conn; (void)method;
    if (off > 0) return ATT_ERR_ATTR_NOT_LONG;
    if (a->pValue == ttCmdSta) {                     /* last command counter and its result (7 still checking) */
        uint32_t n = g_cmd_n;
        ttCmdSta[0] = (uint8_t)n; ttCmdSta[1] = (uint8_t)(n >> 8); ttCmdSta[2] = (uint8_t)(n >> 16); ttCmdSta[3] = (uint8_t)(n >> 24);
        ttCmdSta[4] = g_cmd_state ? TT_CMD_PENDING : g_cmd_r;
        *len = max < 5 ? max : 5;
        memcpy(v, ttCmdSta, *len);
        return SUCCESS;
    }
    if (a->pValue != ttStaVal) return ATT_ERR_ATTR_NOT_FOUND;
    tt_mdl_status(&g_mdl, ttStaVal);
    *len = max < 8 ? max : 8;
    memcpy(v, ttStaVal, *len);
    return SUCCESS;
}

static bStatus_t ttWriteCB(uint16_t conn, gattAttribute_t *a, uint8_t *v, uint16_t len, uint16_t off, uint8_t method)
{
    int r;
    uint8_t k;
    (void)conn; (void)method;
    if (a->pValue == &ttCmdVal) {
        if (off > 0 || len != TT_CMD_LEN) return ATT_ERR_INVALID_VALUE_SIZE;
        if (g_cmd_state) return (bStatus_t)0x87;    /* one command at a time */
        memcpy(g_cmd_buf, v, TT_CMD_LEN);
        g_cmd_state = 1;
        return SUCCESS;
    }
    if (a->pValue != &ttMdlVal) return ATT_ERR_ATTR_NOT_FOUND;
    if (off > 0) return ATT_ERR_ATTR_NOT_LONG;
    k = g_mdl.loading;                                  /* the slot a COMMIT closes: kind + 1 */
    r = tt_mdl_write(&g_mdl, v, len);
    g_mdl_last_write = uptime_s();
    if (r == 1 && k) g_mdl_check |= (uint8_t)(1u << (k - 1));      /* HMAC runs on the tick task's stack, not here */
    if (r >= 10 && k && !g_mdl.loading) g_mdl_restore |= (uint8_t)(1u << (k - 1));   /* a failed COMMIT */
    return r >= 10 ? (bStatus_t)(0x80 + r - 10) : SUCCESS;      /* application error codes 0x81 to 0x85 */
}

static const gattServiceCBs_t ttCBs = { ttReadCB, ttWriteCB, NULL };

static uint32_t uptime_s(void) { return (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ); }

static void uart_print(const char *s)
{
    if (g_uart) UART2_write(g_uart, s, strlen(s), NULL);
}

static void add_event(uint8_t type, uint16_t v, uint32_t t)
{
    tt_event_t e;
    e.id = g_nv.next_ev++;
    e.type = type; e.t = t; e.v = v;
    memmove(&g_nv.ev[1], &g_nv.ev[0], sizeof(tt_event_t) * (TT_MAX_EVENTS - 1));
    g_nv.ev[0] = e;
    if (g_nv.nev < TT_MAX_EVENTS) g_nv.nev++;
    g_nv_dirty = 1;
}

static void build_and_load(uint8_t legacy)
{
    uint32_t now = uptime_s();
    int link = (g_last_line_s != 0xFFFFFFFFu) && (now - g_last_line_s) <= TT_LINK_TIMEOUT_S;
    if (g_fd_mode) {                       /* a generic device: both sets carry the field-data frame */
        static fd_fields_t lf;
        size_t n;
        if (legacy ? !g_dirty_legacy : (!g_dirty_ext || g_ext_handle == 0xFF)) return;
        if (legacy) {
            /* a legacy packet holds 31 bytes: flags (3) and as many fields as fit after the 9-byte header */
            lf = g_fd_cur;
            do { n = fd_build_ad(g_legacy_buf + 3, sizeof(g_legacy_buf) - 3, g_seq, &lf); } while (!n && lf.n-- > 0);
            g_legacy_buf[0] = 2; g_legacy_buf[1] = 0x01; g_legacy_buf[2] = 0x06;
            if (GapAdv_prepareLoadByHandle(peripheralAdvHandles[0], GAP_ADV_FREE_OPTION_DONT_FREE) != SUCCESS) return;
            GapAdv_loadByHandle(peripheralAdvHandles[0], GAP_ADV_DATA_TYPE_ADV, (uint16)(n + 3), g_legacy_buf);
            g_dirty_legacy = 0;
        } else {
            size_t nl = strlen(g_name);
            g_ext_buf[0] = 2; g_ext_buf[1] = 0x01; g_ext_buf[2] = 0x06;
            g_ext_buf[3] = (uint8_t)(nl + 1); g_ext_buf[4] = 0x09; memcpy(&g_ext_buf[5], g_name, nl);
            n = 5 + nl;
            n += fd_build_ad(g_ext_buf + n, sizeof(g_ext_buf) - n, g_seq, &g_fd_cur);
            if (GapAdv_prepareLoadByHandle(g_ext_handle, GAP_ADV_FREE_OPTION_DONT_FREE) != SUCCESS) return;
            GapAdv_loadByHandle(g_ext_handle, GAP_ADV_DATA_TYPE_ADV, (uint16)n, g_ext_buf);
            g_dirty_ext = 0;
        }
        return;
    }
    if (legacy) {
        uint8_t flags = tt_flags(&g_cur, link, g_ext_handle != 0xFF);
        if (!g_dirty_legacy && flags == g_last_flags) return;
        if (GapAdv_prepareLoadByHandle(peripheralAdvHandles[0], GAP_ADV_FREE_OPTION_DONT_FREE) != SUCCESS) return;
        tt_build_legacy(g_legacy_buf, g_seq, &g_cur, flags);
        GapAdv_loadByHandle(peripheralAdvHandles[0], GAP_ADV_DATA_TYPE_ADV, TT_LEGACY_LEN, g_legacy_buf);
        g_last_flags = flags;
        g_dirty_legacy = 0;
    } else {
        tt_tagstat_t tag;
        size_t n;
        if (g_ext_handle == 0xFF || !g_dirty_ext) return;
        tag.up = now;
        tag.link = (g_last_line_s == 0xFFFFFFFFu) ? 0xFFFF : (uint16_t)((now - g_last_line_s) > 0xFFFE ? 0xFFFE : now - g_last_line_s);
        tag.rx_ok = g_rx_ok; tag.rx_bad = g_rx_bad; tag.fw = TT_FW; tag.mv = tt_mdl_version(&g_mdl);
        if (GapAdv_prepareLoadByHandle(g_ext_handle, GAP_ADV_FREE_OPTION_DONT_FREE) != SUCCESS) return;
        n = tt_build_ext(g_ext_buf, sizeof(g_ext_buf), g_seq, &g_cur, g_nv.ev, g_nv.nev > 5 ? 5 : g_nv.nev, g_pulls, g_npull,
                         &tag, g_name);
        GapAdv_loadByHandle(g_ext_handle, GAP_ADV_DATA_TYPE_ADV, (uint16)n, g_ext_buf);
        g_dirty_ext = 0;
    }
}

/* Runs in the BLE thread every 250 ms. */
static void TT_tick(char *unused)
{
    uint8_t i, nq;
    static tt_line_t q[4];                   /* static: TI's BLE thread has a 1 kB stack */
    static uint32_t last_ext_refresh, last_nv_save;
    (void)unused;
    uint32_t now = uptime_s();

    /* demo mode: only while no tool MCU is talking */
    if (g_last_line_s == 0xFFFFFFFFu || now - g_last_line_s > TT_LINK_TIMEOUT_S) {
        uint8_t jam = g_btn_jam_req;
        g_btn_jam_req = 0;
        g_demo_ms += 250;
        taskENTER_CRITICAL();
        if (g_btn_trig && !g_demo_running && !jam) {
            g_demo_running = 1; g_demo_pull_ms = g_demo_ms;
            strcpy(g_pending.id, "BTN-DEMO");
            g_pending.trig++; g_pending.st = TT_ST_RUN; g_pending.rpm = 3200; g_new = 1;
            g_nv.demo_trig = g_pending.trig; g_nv_dirty = 1;
        }
        if (g_demo_running) {
            g_demo_run_ms += 250;
            g_pending.run = g_demo_run_ms / 1000;
            g_pending.rpm = (uint16_t)(3100 + (g_demo_ms / 250) % 7 * 30);
            if (g_pending.rpm > g_pending.rpmx) g_pending.rpmx = g_pending.rpm;
            g_new = 1;
        }
        if (g_demo_running && (jam || !g_btn_trig)) {
            uint32_t dur = g_demo_ms - g_demo_pull_ms;
            if (jam && g_ev_new < 4) { g_ev_q[g_ev_new].ev = TT_EV_OC; g_ev_q[g_ev_new].v = 0; g_ev_new++; g_pending.oc++; }
            g_nv.demo_oc = g_pending.oc; g_nv.demo_run = g_pending.run; g_nv_dirty = 1;
            if (g_ev_new < 4) { g_ev_q[g_ev_new].ev = TT_EV_TRIG; g_ev_q[g_ev_new].v = (uint16_t)(dur > 65535 ? 65535 : dur); g_ev_new++; }
            if (g_pull_new < 2) {            /* a pull summary too: no current measured, jam known from the button */
                tt_pull_t *pl = &g_pull_q[g_pull_new++].pull;
                memset(pl, 0, sizeof(*pl));
                pl->dur10 = (uint16_t)(dur / 10); pl->ravg = jam ? 1900 : 3150; pl->rmax = 3300;
                pl->tc = g_pending.tc; pl->cls = jam ? 3 : 255;
            }
            g_demo_running = 0; g_pending.st = TT_ST_IDLE; g_pending.rpm = 0; g_new = 1;
        } else if (jam) {
            strcpy(g_pending.id, "BTN-DEMO");
            if (g_ev_new < 4) { g_ev_q[g_ev_new].ev = TT_EV_OC; g_ev_q[g_ev_new].v = 0; g_ev_new++; }
            g_pending.oc++; g_new = 1;
            g_nv.demo_oc = g_pending.oc; g_nv_dirty = 1;
        }
        taskEXIT_CRITICAL();
        GPIO_write(CONFIG_GPIO_LED_GREEN, g_demo_running);
    }

    taskENTER_CRITICAL();
    {
        uint8_t k;
        for (k = 0; k < g_pull_new; k++) {
            tt_pull_t pl = g_pull_q[k].pull;
            pl.id = g_pull_next++;
            if (pl.cls == 255) {                /* the tool did not classify it: the tag does, with the installed model if any */
                int c = tt_mdl_classify(&g_mdl, &pl);
                pl.cls = c >= 0 ? (uint8_t)c : tt_model_classify(&pl);
            }
            memmove(&g_pulls[1], &g_pulls[0], sizeof(tt_pull_t) * (TT_MAX_PULLS - 1));
            g_pulls[0] = pl;
            if (g_npull < TT_MAX_PULLS) g_npull++;
            g_dirty_ext = 1;
        }
        g_pull_new = 0;
    }
    if (g_fd_new) {
        g_fd_cur = g_fd_pending; g_fd_new = 0; g_fd_mode = 1;
        g_seq++; g_dirty_legacy = g_dirty_ext = 1;
    }
    nq = g_ev_new; memcpy(q, g_ev_q, sizeof(tt_line_t) * nq); g_ev_new = 0;
    if (g_new) {
        g_cur = g_pending;
        g_pending.rpmx = g_pending.rpm;
        g_new = 0;
        g_seq++;
        g_dirty_legacy = g_dirty_ext = 1;
    }
    taskEXIT_CRITICAL();

    for (i = 0; i < nq; i++) {
        add_event(q[i].ev, q[i].v, now);
        g_dirty_ext = 1;
    }
    if (now - last_ext_refresh >= 5) { g_dirty_ext = 1; last_ext_refresh = now; }
    {                                         /* the tick task checked a model's MAC: use it (and keep it) or refuse it */
        uint8_t k, r, from_flash;
        uint16_t g;
        for (k = 0; k < 2; k++) {
            taskENTER_CRITICAL(); r = g_mdl_vres[k]; g = g_mdl_vgen[k]; g_mdl_vres[k] = 0; taskEXIT_CRITICAL();
            if (!r || g_mdl.loading == k + 1 || g != g_mdl.gen[k]) continue;   /* the slot changed after the check: a newer upload owns it */
            from_flash = (r & 0x80) != 0;
            g_dirty_ext = 1;
            if ((r & 0x7F) == 1) {
                g_mdl.live[k] = 1;
                if (!from_flash) g_mdl.result = TT_MDL_OK;     /* a restore after a refusal keeps reporting the refusal */
                if (g_mdl.slot[k].ver > g_nv.mdl_floor[k]) { g_nv.mdl_floor[k] = g_mdl.slot[k].ver; g_nv_dirty = 1; }   /* also for one kept in flash across a firmware update */
                if (!from_flash) {
                    osal_snv_write(k ? TT_NV_MDL1 : TT_NV_MDL0, sizeof(tt_mdl_t), &g_mdl.slot[k]);
                    if (k == 0) add_event(TT_EV_MODEL, g_mdl.slot[0].ver, now);
                }
            } else {
                g_mdl.live[k] = 0; g_mdl.result = TT_MDL_ERR_AUTH;
                if (!from_flash) g_mdl_restore |= (uint8_t)(1u << k);    /* refused: the last good model comes back */
            }
        }
    }
    if (g_cmd_done) {                         /* a command was checked: remember its counter, log it */
        if (g_cmd_r == TT_CMD_OK) {           /* save the counter first: a restart can never make this command valid again */
            g_nv.cmd_ctr = g_cmd_n;
            if (g_cmd_c == 5) {                   /* a lease: start (or end) the countdown; a lock it caused is lifted */
                g_nv.lease_min = g_cmd_d;
                g_lease_until = g_cmd_d ? now + 60u * g_cmd_d : 0;
                if (g_lease_locked) { strcpy(g_cmd_line, TC_UNLOCK); g_lease_locked = 0; }
                g_nv.lease_locked = 0;
            } else if (g_cmd_c == 3 && g_lease_locked) {   /* a person unlocked it: the lease starts again */
                g_lease_locked = 0; g_nv.lease_locked = 0;
                g_lease_until = g_nv.lease_min ? now + 60u * g_nv.lease_min : 0;
            }
            add_event(TT_EV_CMD, (uint16_t)(g_cmd_c | g_cmd_d << 8), now);
            if (osal_snv_write(TT_NV_ID, sizeof(g_nv), &g_nv) == SUCCESS) {
                last_nv_save = now; g_nv_dirty = 0;
                if (g_cmd_line[0]) g_cmd_send = 1;   /* the tick task sends it: the only task that writes lines to the UART */
            } else {
                g_cmd_r = TT_CMD_ERR_BAD; g_nv_dirty = 1;   /* not saved: not passed on */
            }
        }
        g_cmd_done = 0;
    }
    if (g_lease_until && !g_lease_locked && (int32_t)(now - g_lease_until) >= 0 && !g_cmd_send) {
        g_lease_locked = 1;                   /* no site hub renewed the lease in time: off site, or taken */
        g_nv.lease_locked = 1; g_nv_dirty = 1;
        strcpy(g_cmd_line, TC_LOCK);
        g_cmd_send = 1;
        add_event(TT_EV_CMD, (uint16_t)(2u | 0xFFu << 8), now);    /* a lock with argument 255: the lease ran out */
    }
    if (g_mdl.loading && now - g_mdl_last_write > 30) {           /* an upload left open (the hub went away) */
        g_mdl_restore |= (uint8_t)(1u << (g_mdl.loading - 1));
        g_mdl.loading = 0;
    }
    if (g_mdl_restore) {                      /* reload what flash holds; it goes live again only after its MAC check */
        uint8_t k;
        for (k = 0; k < 2; k++)
            if ((g_mdl_restore & (1u << k)) && g_mdl.loading != k + 1
                && osal_snv_read(k ? TT_NV_MDL1 : TT_NV_MDL0, sizeof(tt_mdl_t), &g_mdl.slot[k]) == SUCCESS) {
                g_mdl.gen[k]++;                   /* new bytes in the slot: any check still running on the old ones is stale */
                g_mdl_check |= (uint8_t)(0x11u << k);
            }
        g_mdl_restore = 0;
    }
    if (g_nv_dirty && now - last_nv_save >= 2) {
        osal_snv_write(TT_NV_ID, sizeof(g_nv), &g_nv);
        g_nv_dirty = 0; last_nv_save = now;
    }

    if (g_scan_rsp_pending) {
        if (GapAdv_prepareLoadByHandle(peripheralAdvHandles[0], GAP_ADV_FREE_OPTION_DONT_FREE) == SUCCESS) {
            GapAdv_loadByHandle(peripheralAdvHandles[0], GAP_ADV_DATA_TYPE_SCAN_RSP, g_scan_rsp_len, g_scan_rsp);
            g_scan_rsp_pending = 0;
        }
        return;
    }
    /* One prepare/load per tick: the stack wants each to finish before the next. */
    build_and_load(g_phase ^= 1);

    g_status_due = 1;
}

/* The status line is formatted here, on this task's own stack, never on the BLE thread. */
static void ttTickTask(void *arg)
{
    static char s[160];
    uint32_t last_status = 0;
    (void)arg;
    for (;;) {
        uint32_t now;
        vTaskDelay(pdMS_TO_TICKS(250));
        if (g_cmd_send) { uart_print(g_cmd_line); g_cmd_send = 0; }   /* the tool MCU acts and reports st in its next TS line */
        if (g_cmd_state == 1 && !g_cmd_done) { /* a signed command: check it here, then pass it to the tool */
            static const char *const CN[] = { "", "stop", "lock", "unlock", "run" };
            uint8_t c = 0, d = 0, x = 0;
            uint32_t n = 0;
            int r = tt_cmd_check(TT_MDL_KEY, g_addr, g_cmd_buf, g_nv.cmd_ctr, &c, &d, &n);
            if (r == TT_CMD_OK && c == 5) {
                g_cmd_line[0] = 0;                /* a lease is the tag's own business: nothing goes to the tool */
            } else if (r == TT_CMD_OK) {          /* formatted here; sent by TT_tick only after the counter is saved */
                char *q = g_cmd_line;
                int k, len = c == 4 ? snprintf(q, sizeof(g_cmd_line), "\r\n$TC,cmd=%s,duty=%u", CN[c], d) : snprintf(q, sizeof(g_cmd_line), "\r\n$TC,cmd=%s", CN[c]);
                for (k = 3; k < len; k++) x ^= (uint8_t)q[k];                  /* XOR of the bytes between '$' and '*' */
                snprintf(q + len, sizeof(g_cmd_line) - (size_t)len, "*%02X\r\n", x);
            }
            g_cmd_r = (uint8_t)r; g_cmd_c = c; g_cmd_d = d; g_cmd_n = r == TT_CMD_OK ? n : g_cmd_n;
            g_cmd_done = 1;
            g_cmd_state = 0;
        }
        if (g_mdl_check) {                    /* MAC checks: SHA-256 wants a few hundred bytes of stack, which this task has */
            uint8_t k, c, r;
            uint16_t gen;
            taskENTER_CRITICAL(); c = g_mdl_check; g_mdl_check = 0; taskEXIT_CRITICAL();
            for (k = 0; k < 2; k++) {
                if (!(c & (1u << k))) continue;
                gen = g_mdl.gen[k];               /* read before hashing: a BEGIN during the check moves it */
                r = (tt_mdl_trusted(&g_mdl.slot[k], k, TT_MDL_KEY) && g_mdl.slot[k].ver >= g_nv.mdl_floor[k]) ? 1 : 2;
                if (c & (0x10u << k)) r |= 0x80;  /* from flash: no save, no event */
                taskENTER_CRITICAL();
                if (g_mdl.gen[k] == gen) { g_mdl_vres[k] = r; g_mdl_vgen[k] = gen; }   /* a stale check is dropped */
                taskEXIT_CRITICAL();
            }
        }
        BLEAppUtil_invokeFunctionNoData(TT_tick);
        now = uptime_s();
        if (g_status_due && now - last_status >= 10) {
            last_status = now;
            snprintf(s, sizeof(s), "@TAG up=%lu name=%s seq=%u ok=%u bad=%u gap=%u link=%d trig=%lu oc=%u run=%lu ev=%u pulls=%u ext=%d\r\n",
                     (unsigned long)now, g_name, g_seq, g_rx_ok, g_rx_bad, g_rx_gap,
                     g_last_line_s == 0xFFFFFFFFu ? -1 : (int)(now - g_last_line_s), (unsigned long)g_cur.trig, g_cur.oc,
                     (unsigned long)g_cur.run, g_nv.next_ev, (unsigned)(g_pull_next - 1), (int)g_ext_status);
            uart_print(s);
        }
    }
}

static void ttUartTask(void *arg)
{
    static char line[TT_LINE_MAX + 2];
    size_t len = 0;
    int in_line = 0;
    int32_t last_n = -1;
    (void)arg;
    for (;;) {
        char buf[32];
        size_t got = 0, i;
        if (UART2_read(g_uart, buf, sizeof(buf), &got) != UART2_STATUS_SUCCESS && got == 0) continue;
        for (i = 0; i < got; i++) {
            char c = buf[i];
            if (c == '$') { len = 0; in_line = 1; }
            if (!in_line) continue;          /* text that is not a telemetry line (a tool console) is ignored */
            if (c == '\n') {
                tt_line_t l;
                tt_tool_t t;
                int r;
                if (len > 3 && line[1] == 'F' && line[2] == 'D') {
                    fd_fields_t f;
                    int32_t fn;
                    if (fd_parse_line(line, len, &f, &fn) == 0) {
                        taskENTER_CRITICAL();
                        g_fd_pending = f; g_fd_new = 1; g_last_line_s = uptime_s(); g_rx_ok++;
                        taskEXIT_CRITICAL();
                        GPIO_toggle(CONFIG_GPIO_LED_GREEN);
                    } else if (g_rx_bad < 255) {
                        g_rx_bad++;
                    }
                    len = 0; in_line = 0;
                    continue;
                }
                taskENTER_CRITICAL(); t = g_pending; taskEXIT_CRITICAL();
                r = tt_parse_line(line, len, &l, &t);
                if (r == 0 && l.type[0] == 'T' && (l.type[1] == 'S' || l.type[1] == 'B' || l.type[1] == 'E' || l.type[1] == 'P')) {
                    if (l.n >= 0 && last_n >= 0 && l.n != ((last_n + 1) & 0xFFFF)) g_rx_gap++;
                    if (l.n >= 0) last_n = l.n;
                    taskENTER_CRITICAL();
                    g_pending = t;
                    g_new = 1;
                    if (l.type[1] == 'E' && l.ev && g_ev_new < 4) g_ev_q[g_ev_new++] = l;
                    if (l.type[1] == 'P' && g_pull_new < 2) g_pull_q[g_pull_new++] = l;
                    g_last_line_s = uptime_s();
                    g_rx_ok++;
                    taskEXIT_CRITICAL();
                    GPIO_toggle(CONFIG_GPIO_LED_GREEN);
                    GPIO_write(CONFIG_GPIO_LED_RED, t.st == TT_ST_FAULT);
                } else if (len > 0) {
                    if (g_rx_bad < 255) g_rx_bad++;
                    uart_print("#bad\r\n");
                }
                len = 0; in_line = 0;
            } else if (len < TT_LINE_MAX) {
                line[len++] = c;
            } else {
                len = 0; in_line = 0;        /* too long: drop until the next '$' */
                if (g_rx_bad < 255) g_rx_bad++;
            }
        }
    }
}

static StaticTask_t g_uart_tcb, g_tick_tcb;
static StackType_t  g_uart_stack[320], g_tick_stack[256];

bStatus_t ToolTag_start(void)
{
    UART2_Params p;
    const uint8_t *addr;
    BLEAppUtil_AdvInit_t init;
    BLEAppUtil_AdvStart_t start = { .enableOptions = GAP_ADV_ENABLE_OPTIONS_USE_MAX, .durationOrMaxEvents = 0 };
    static GapAdv_params_t extParams = {
        .eventProps = 0,
        .primIntMin = 1600, .primIntMax = 1600,          /* 1 s in 0.625 ms units */
        .primChanMap = GAP_ADV_CHAN_ALL,
        .peerAddrType = PEER_ADDRTYPE_PUBLIC_OR_PUBLIC_ID,
        .peerAddr = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa },
        .filterPolicy = GAP_ADV_AL_POLICY_ANY_REQ,
        .txPower = GAP_ADV_TX_POWER_NO_PREFERENCE,
        .primPhy = GAP_ADV_PRIM_PHY_1_MBPS,
        .secPhy = GAP_ADV_SEC_PHY_1_MBPS,
        .sid = 1,
        .zeroDelay = 0
    };
    size_t n;
    static char s[112];

    UART2_Params_init(&p);
    p.baudRate = 115200;
    p.readMode = UART2_Mode_BLOCKING;
    p.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    g_uart = UART2_open(CONFIG_UART2_0, &p);
    uart_print("\r\n@TAG start\r\n");

    memset(&g_pending, 0, sizeof(g_pending));
    g_pending.soc = 0xFF;
    g_cur = g_pending;

    if (osal_snv_read(TT_NV_ID, sizeof(g_nv), &g_nv) != SUCCESS || g_nv.magic != TT_NV_MAGIC) {
        memset(&g_nv, 0, sizeof(g_nv));
        g_nv.magic = TT_NV_MAGIC;
    }
    g_nv.boots++;
    add_event(TT_EV_TAGBOOT, g_nv.boots, 0);
    /* A restart neither ends the rule nor buys a full lease: a locked tool locks again at once, an unlocked one gets
       one minute to hear a hub (restarting it every few minutes cannot keep a taken tool usable). */
    if (g_nv.lease_min) g_lease_until = g_nv.lease_locked ? 1u : 60u;
    if (g_nv.demo_trig || g_nv.demo_oc) {           /* the button demo ran before this restart: carry on its counts */
        strcpy(g_pending.id, "BTN-DEMO");
        g_pending.trig = g_nv.demo_trig; g_pending.oc = g_nv.demo_oc; g_pending.run = g_nv.demo_run;
        g_demo_run_ms = g_nv.demo_run * 1000u;
        g_cur = g_pending;
    }
    {
        uint8_t k;
        for (k = 0; k < 2; k++)                     /* installed models come back only once their MAC checks out again */
            if (osal_snv_read(k ? TT_NV_MDL1 : TT_NV_MDL0, sizeof(tt_mdl_t), &g_mdl.slot[k]) == SUCCESS)
                g_mdl_check |= (uint8_t)(0x11u << k);
    }
    GATTServApp_RegisterService(ttAttrTbl, GATT_NUM_ATTRS(ttAttrTbl), GATT_MAX_ENCRYPT_KEY_SIZE, &ttCBs);

    addr = GAP_GetDevAddress(TRUE);
    memcpy(g_addr, addr, 6);                         /* signed commands are bound to this address */
    /* Each tag its own set ID (1 to 15, from its address): a scanner joins an extended advertisement's packets by set
       ID, so two tags on the same ID were sometimes reported under each other's address (seen with two tags). */
    extParams.sid = (uint8_t)(1 + (addr[0] ^ addr[1]) % 15);
    snprintf(g_name, sizeof(g_name), "Tag %02X%02X", addr[1], addr[0]);
    n = strlen(g_name);
    g_scan_rsp[0] = (uint8_t)(n + 1); g_scan_rsp[1] = 0x09; memcpy(&g_scan_rsp[2], g_name, n);
    g_scan_rsp_len = (uint8_t)(n + 2);

    n = tt_build_ext(g_ext_buf, sizeof(g_ext_buf), g_seq, &g_cur, g_nv.ev, g_nv.nev > 5 ? 5 : g_nv.nev, NULL, 0, NULL, g_name);
    init.advDataLen = (uint16)n; init.advData = g_ext_buf;
    init.scanRespDataLen = 0; init.scanRespData = NULL;
    init.advParam = &extParams;
    g_ext_status = BLEAppUtil_initAdvSet(&g_ext_handle, &init);
    if (g_ext_status == SUCCESS) g_ext_status = BLEAppUtil_advStart(g_ext_handle, &start);
    if (g_ext_status != SUCCESS) g_ext_handle = 0xFF;

    snprintf(s, sizeof(s), "\r\n@TAG boot name=%s addr=%02X:%02X:%02X:%02X:%02X:%02X boots=%u ext=%d fw=%d model=%u\r\n", g_name,
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0], g_nv.boots, (int)g_ext_status, TT_FW, tt_mdl_version(&g_mdl));
    uart_print(s);

    {
        Button_Params bp;
        Button_Params_init(&bp);
        bp.buttonEventMask = Button_EV_PRESSED | Button_EV_RELEASED;
        bp.buttonCallback = buttonCb;
        g_btn[0] = Button_open(CONFIG_BUTTON_0, &bp);
        g_btn[1] = Button_open(CONFIG_BUTTON_1, &bp);
    }
    if (g_uart) xTaskCreateStatic(ttUartTask, "ttUart", 320, NULL, 2, g_uart_stack, &g_uart_tcb);
    xTaskCreateStatic(ttTickTask, "ttTick", 256, NULL, 2, g_tick_stack, &g_tick_tcb);
    return g_uart ? SUCCESS : FAILURE;
}
