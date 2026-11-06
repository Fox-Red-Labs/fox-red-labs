/* Tool-telemetry gateway for the LP-EM-CC35X1, added to TI's network_terminal demo (apply.py hooks it in).
 *
 * At boot it runs the demo's own commands to start Bluetooth and a Wi-Fi access point, then scans continuously
 * (passive, extended scanning, so it hears both the tags' legacy and extended sets), keeps a table of tags and a log
 * of snapshots and events, serves the phone app and a JSON API on port 80, and prints each new log item on the UART
 * as an "@L {json}" line for a modem or the home base bridge. Formats: ../PROTOCOL.md. Codec: ../proto/tt_proto.c.
 *
 * The demo's terminal keeps working: type its commands on the same UART as before. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "lwip/sockets.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "network_terminal.h"
#include "uart_term.h"
#include "tt_proto.h"
#include "fd_frame.h"
#include "tt_app.h"
#include "tt_secrets.h"      /* written by apply.py into the build copy: hub password, optional site network */

#ifndef TTGW_AP_PASS
#error "TTGW_AP_PASS must be set: build.sh makes a random one per hub (no shared default password)"
#endif
#ifndef TTGW_AP_CHANNEL
#define TTGW_AP_CHANNEL  "6"
#endif
#define TTGW_FW          "tt-gw 1.1"   /* 1.1: installs signed models on tags */
#define TTGW_MAX_TAGS    24
#define TTGW_LOG_LEN     2048
#define TTGW_LINK_S      15
#define TTGW_GPAY        192          /* generic (field-data) records kept with their fields; heap. A phone or uplink
                                         must collect within that many generic records, or the older fields are lost */

extern cmdAction_t gCmdList[];
extern uint32_t gMaxCmd;
extern int8_t network_stack_get_if_ip(WlanRole_e role, uint32_t *ip, uint32_t *netmask, uint32_t *gw, uint32_t *dhcp);

typedef struct {
    uint8_t      used;
    uint8_t      addr[6];
    uint8_t      addr_type;
    char         name[20];
    int8_t       rssi;
    uint32_t     last_ms;
    uint32_t     nl, ne;
    tt_tool_t    tool;
    uint8_t      flags;
    uint8_t      has_tag;
    tt_tagstat_t tag;
    uint8_t      logged_seq_valid, logged_seq;
    uint8_t      ev_valid, pull_valid;
    uint16_t     ev_last, pull_last;
    uint32_t     pulls, pulls_missed;
    uint8_t      app;                 /* 1 tool telemetry, others from portability/fielddata/apps */
    fd_fields_t *fd;                  /* latest generic fields (heap), NULL for tool tags */
} ttgw_tag_t;

typedef struct {
    uint32_t idx;
    uint32_t up;          /* gateway uptime, s */
    uint8_t  tag;         /* index into g_tags */
    uint8_t  kind;        /* 's' snapshot, 'e' event, 'p' pull summary */
    union {
        struct { uint32_t trig, run; uint16_t oc, flt, rpm; uint8_t seq, st, soc; int8_t tc; } s;
        struct { uint32_t age; uint16_t id, v; uint8_t type; } e;   /* age: s before 'up', 0xFFFFFFFF before the tag restarted */
        tt_pull_t p;
        struct { uint32_t gen; uint8_t seq, app; } g;                 /* fields in g_gpay[gen % TTGW_GPAY] while fresh */
        struct { uint32_t ms; uint16_t ver; uint8_t ok, err; } m;     /* a model install: how long, which, and how it ended */
    } u;
} ttgw_item_t;

static ttgw_tag_t   g_tags[TTGW_MAX_TAGS];
static ttgw_item_t  g_log[TTGW_LOG_LEN];
static uint32_t     g_log_next;               /* index the next item gets */
static uint32_t     g_unix_base;              /* unix time at gateway uptime 0, 0 if unknown */
static uint32_t     g_adv_total, g_adv_ours, g_adv_legacy, g_adv_ext, g_adv_dropped, g_scan_restarts, g_adv_mismatched;
static char         g_hub[24] = "FRL-HUB";
static SemaphoreHandle_t g_lock;
static fd_fields_t *g_fdstate;        /* TTGW_MAX_TAGS, heap */
static fd_fields_t *g_gpay;           /* TTGW_GPAY, heap */
static uint32_t     g_gpay_next;
static volatile uint8_t  g_scan_restart;
#ifdef TTGW_UPLINK_URL
static uint32_t g_up_next, g_up_ok, g_up_fail, g_up_items;   /* the hub's own uploads to the home base */
static char    *g_up_buf;                                     /* heap */
#endif

/* ---------- model installs over the air (../proto/tt_mdl.h) ----------
   POST /api/model takes one package (hex); with both trees at the same version, the hub connects to each tool tag
   that reports another version, writes BEGIN, DATA and COMMIT for each tree, reads the result and disconnects.
   One tag at a time; scanning stops for the few seconds a connection takes. */
#define TTGW_PKG_MAX (6 + 40 * 5)
#define TTGW_PUSH_TRIES 3
enum { PUSH_IDLE, PUSH_CONNECT, PUSH_DISC_MDL, PUSH_DISC_STA, PUSH_WRITE, PUSH_READ, PUSH_DONE };
static uint8_t     *g_pkg[2];                 /* heap: static RAM is full */
static uint16_t     g_pkg_len[2], g_pkg_ver[2];
static volatile uint8_t g_push;                /* PUSH_* */
static int          g_push_tag = -1;
static uint16_t     g_push_conn, g_push_mdl_h, g_push_sta_h, g_push_off;
static uint8_t      g_push_k, g_push_phase, g_push_err, g_push_ok, g_push_buf[20];
static uint32_t     g_push_t0;
static uint8_t      g_push_tries[TTGW_MAX_TAGS];
static uint32_t     g_push_next_ms[TTGW_MAX_TAGS];
static uint32_t     g_pushes_ok, g_pushes_failed;
/* Signed commands (stage two): one waits at a time, for one tag; the hub only carries it, the tag checks it. */
static uint8_t      g_push_cmd;                /* 1 while the connection carries a command, not a model */
static volatile uint8_t g_cmd_pending, g_cmd_tries;
static uint8_t      g_cmd_addr[6], g_cmd_bytes[18], g_cmd_last_ok, g_cmd_last_err, g_cmd_last;
static uint32_t     g_cmd_queued_ms;           /* a command for a tag the hub never hears is dropped after 60 s */
/* Leases: the home base signs a batch per tool (it alone holds the key); the hub hands one over every "every" seconds
   while it hears the tool. Off site there is no hub, so no lease, and the tag locks the tool when the last one runs out. */
#define TTGW_LEASE_TAGS 4
#define TTGW_LEASES     20
typedef struct { uint8_t used, n, next; uint8_t addr[6]; uint16_t every_s; uint32_t due_ms; uint8_t cmd[TTGW_LEASES][18]; } ttgw_lease_t;
static ttgw_lease_t *g_lease;                  /* TTGW_LEASE_TAGS, heap */

static uint32_t now_ms(void) { return (uint32_t)(xTaskGetTickCount() * (1000 / configTICK_RATE_HZ)); }
static uint32_t now_s(void) { return now_ms() / 1000; }

/* ---------- running the demo's own commands ---------- */

static int32_t run_cmd(const char *line)
{
    static char buf[200];
    uint32_t i;
    size_t n = strlen(line), k = 0;
    if (n >= sizeof(buf)) return -1;
    memcpy(buf, line, n + 1);
    while (buf[k] && buf[k] != ' ') k++;
    for (i = 0; i < gMaxCmd; i++) {
        if (strlen(gCmdList[i].cmd) == k && !strncmp(gCmdList[i].cmd, buf, k)) {
            const char *pw = strstr(line, " -p ");
            if (!strncmp(line, "wlan_connect", 12)) Report("\r\n[ttgw] > wlan_connect (site network; name and password not shown)\r\n");
            else if (pw) Report("\r\n[ttgw] > %.*s -p ********\r\n", (int)(pw - line), line);   /* never echo a password */
            else Report("\r\n[ttgw] > %s\r\n", line);
            return gCmdList[i].callback((void *)(buf + k));
        }
    }
    Report("\r\n[ttgw] no command %s\r\n", line);
    return -1;
}

/* ---------- table and log (callers hold g_lock) ---------- */

static int tag_index(const ble_addr_t *a, int create)
{
    int i, free_i = -1, oldest = 0;
    for (i = 0; i < TTGW_MAX_TAGS; i++) {
        if (g_tags[i].used && !memcmp(g_tags[i].addr, a->val, 6)) return i;
        if (!g_tags[i].used && free_i < 0) free_i = i;
        if (g_tags[i].last_ms < g_tags[oldest].last_ms) oldest = i;
    }
    if (!create) return -1;
    i = free_i >= 0 ? free_i : oldest;
    memset(&g_tags[i], 0, sizeof(g_tags[i]));
    g_tags[i].used = 1;
    memcpy(g_tags[i].addr, a->val, 6);
    g_tags[i].addr_type = a->type;
    g_tags[i].tool.soc = 0xFF;
    return i;
}

static ttgw_item_t *log_new(int tag, uint8_t kind)
{
    ttgw_item_t *it = &g_log[g_log_next % TTGW_LOG_LEN];
    memset(it, 0, sizeof(*it));
    it->idx = g_log_next++;
    it->up = now_s();
    it->tag = (uint8_t)tag;
    it->kind = kind;
    return it;
}

/* A tag names itself "Tag XXXX" after the last two bytes of its address. A frame whose name does not match the address
   it arrived with belongs to another tag (a scanner joining two tags' extended packets on one set ID): drop it. */
static int frame_mismatch(const struct ble_gap_ext_disc_desc *d, const char *name)
{
    char want[10];
    if (strncmp(name, "Tag ", 4) || strlen(name) != 8) return 0;
    snprintf(want, sizeof(want), "Tag %02X%02X", d->addr.val[1], d->addr.val[0]);
    return strcmp(name, want) != 0;
}

static void on_frame(const struct ble_gap_ext_disc_desc *d, const tt_frame_t *f)
{
    int i, k;
    ttgw_tag_t *t;
    if (frame_mismatch(d, f->name)) { g_adv_mismatched++; return; }
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(50)) != pdTRUE) { g_adv_dropped++; return; }
    i = tag_index(&d->addr, 1);
    t = &g_tags[i];
    t->rssi = d->rssi;
    t->last_ms = now_ms();
    t->app = 1;
    if (f->name[0]) { strncpy(t->name, f->name, sizeof(t->name) - 1); }
    if (f->vt == TT_VT_LEGACY) {
        t->nl++;
        t->tool.trig = f->tool.trig; t->tool.oc = f->tool.oc; t->tool.flt = f->tool.flt; t->tool.run = f->tool.run;
        t->tool.tc = f->tool.tc; t->tool.st = f->tool.st;
        if (f->tool.has_bat) { t->tool.has_bat = 1; t->tool.soc = f->tool.soc; }
        t->flags = f->flags;
    } else {
        uint8_t keep_soc = t->tool.soc, keep_bat = t->tool.has_bat;
        t->ne++;
        t->tool = f->tool;
        if (!f->has_bat) { t->tool.soc = keep_soc; t->tool.has_bat = keep_bat; }
        if (f->has_tag) {
            /* the tag restarted (its uptime went backwards): its event and pull numbers may start again at 1 */
            if (t->has_tag && f->tag.up + 5 < t->tag.up) { t->ev_valid = 0; t->pull_valid = 0; }
            t->tag = f->tag; t->has_tag = 1;
        }
        t->flags = tt_flags(&t->tool, f->has_tag && f->tag.link <= TTGW_LINK_S, 1);
        /* events, oldest first so the log reads in order */
        for (k = (int)f->nev - 1; k >= 0; k--) {
            const tt_event_t *e = &f->ev[k];
            if (t->ev_valid && (int16_t)(e->id - t->ev_last) <= 0) continue;
            ttgw_item_t *it = log_new(i, 'e');
            it->u.e.id = e->id; it->u.e.type = e->type; it->u.e.v = e->v;
            it->u.e.age = (f->has_tag && e->t <= f->tag.up) ? f->tag.up - e->t : 0xFFFFFFFFu;
            t->ev_last = e->id; t->ev_valid = 1;
        }
        /* pull summaries, oldest first; a jump in the tag's numbering means pulls this hub never heard */
        for (k = (int)f->npull - 1; k >= 0; k--) {
            const tt_pull_t *q = &f->pull[k];
            if (t->pull_valid && (int16_t)(q->id - t->pull_last) <= 0) continue;
            if (t->pull_valid && (uint16_t)(q->id - t->pull_last) > 1) t->pulls_missed += (uint16_t)(q->id - t->pull_last) - 1u;
            ttgw_item_t *it = log_new(i, 'p');
            it->u.p = *q;
            t->pull_last = q->id; t->pull_valid = 1; t->pulls++;
        }
    }
    if (!t->logged_seq_valid || t->logged_seq != f->seq) {
        ttgw_item_t *it = log_new(i, 's');
        it->u.s.seq = f->seq; it->u.s.st = t->tool.st; it->u.s.trig = t->tool.trig; it->u.s.run = t->tool.run;
        it->u.s.oc = t->tool.oc; it->u.s.flt = t->tool.flt; it->u.s.rpm = t->tool.rpm;
        it->u.s.soc = t->tool.has_bat ? t->tool.soc : 0xFF; it->u.s.tc = t->tool.tc;
        t->logged_seq = f->seq; t->logged_seq_valid = 1;
    }
    xSemaphoreGive(g_lock);
}

static void on_fd_frame(const struct ble_gap_ext_disc_desc *d, const fd_fields_t *fds, uint8_t seq, const char *name)
{
    int i;
    ttgw_tag_t *t;
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(50)) != pdTRUE) { g_adv_dropped++; return; }
    i = tag_index(&d->addr, 1);
    t = &g_tags[i];
    t->rssi = d->rssi;
    t->last_ms = now_ms();
    t->app = fds->app;
    if (!t->fd) { t->fd = &g_fdstate[i]; memset(t->fd, 0, sizeof(*t->fd)); }   /* a new device in this slot */
    if (name[0]) strncpy(t->name, name, sizeof(t->name) - 1);
    if (d->props & BLE_HCI_ADV_LEGACY_MASK) t->nl++; else t->ne++;
    /* a legacy frame may carry fewer fields: merge, so the extended frame's extra fields are kept */
    {
        uint8_t a, b;
        for (a = 0; a < fds->n; a++) {
            for (b = 0; b < t->fd->n && t->fd->id[b] != fds->id[a]; b++) {}
            if (b < t->fd->n) t->fd->v[b] = fds->v[a];
            else if (t->fd->n < FD_MAX_FIELDS) { t->fd->id[t->fd->n] = fds->id[a]; t->fd->v[t->fd->n] = fds->v[a]; t->fd->n++; }
        }
        t->fd->app = fds->app;
    }
    if (!t->logged_seq_valid || t->logged_seq != seq) {
        ttgw_item_t *it = log_new(i, 'g');
        it->u.g.gen = g_gpay_next++;
        it->u.g.seq = seq; it->u.g.app = fds->app;
        g_gpay[it->u.g.gen % TTGW_GPAY] = *t->fd;
        t->logged_seq = seq; t->logged_seq_valid = 1;
    }
    xSemaphoreGive(g_lock);
}

/* Not one of the tool frames: look for a generic field-data frame and the name. */
static int try_fd(const struct ble_gap_ext_disc_desc *d)
{
    const uint8_t *a = d->data;
    size_t i = 0, len = d->length_data;
    char name[20] = "";
    fd_fields_t f;
    uint8_t seq;
    int ok = -1;
    while (i + 1 < len) {
        uint8_t l = a[i];
        if (l == 0 || i + 1 + l > len) break;
        if (a[i + 1] == 0x09 || a[i + 1] == 0x08) {
            size_t n = l - 1 < sizeof(name) - 1 ? l - 1 : sizeof(name) - 1;
            memcpy(name, &a[i + 2], n); name[n] = 0;
        } else if (a[i + 1] == 0xFF && ok != 0) {
            ok = fd_decode_msd(&a[i + 2], l - 1, &f, &seq);
        }
        i += 1 + l;
    }
    if (ok == 0) on_fd_frame(d, &f, seq, name);
    return ok;
}

/* ---------- Bluetooth scanning ---------- */

static int gap_cb(struct ble_gap_event *ev, void *arg)
{
    (void)arg;
    if (ev->type == BLE_GAP_EVENT_EXT_DISC) {
        const struct ble_gap_ext_disc_desc *d = &ev->ext_disc;
        tt_frame_t f;
        g_adv_total++;
        if (d->data_status != BLE_GAP_EXT_ADV_DATA_STATUS_COMPLETE) return 0;
        if (tt_decode_adv(d->data, d->length_data, &f) == 0) {
            g_adv_ours++;
            if (f.vt == TT_VT_LEGACY) g_adv_legacy++; else g_adv_ext++;
            on_frame(d, &f);
        } else if (try_fd(d) == 0) {
            g_adv_ours++;
            if (d->props & BLE_HCI_ADV_LEGACY_MASK) g_adv_legacy++; else g_adv_ext++;
        } else if (d->length_data && (d->props & BLE_HCI_ADV_SCAN_RSP_MASK)) {
            /* scan responses carry the legacy set's name; passive scanning means we rarely get them */
        }
    } else if (ev->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        g_scan_restart = 1;
    }
    return 0;
}

static int start_scan(void)
{
    struct ble_gap_ext_disc_params p;
    memset(&p, 0, sizeof(p));
    p.itvl = 160; p.window = 112; p.passive = 1;     /* 100 ms interval, 70 ms window, leaves air time for Wi-Fi */
    ble_gap_disc_cancel();
    return ble_gap_ext_disc(BLE_OWN_ADDR_PUBLIC, 0, 0, 0, 0, 0, &p, NULL, gap_cb, NULL);
}

/* ---------- installing a model on a tag ---------- */

static uint32_t g_push_read_at;
static uint8_t  g_push_reads;
static int push_gap_cb(struct ble_gap_event *ev, void *arg);

/* Ends an install: logs it, backs off that tag, and hangs up (the DISCONNECT event restarts the scan). */
static void push_end(uint8_t ok, uint8_t err)
{
    int t = g_push_tag;
    if (g_push == PUSH_IDLE || g_push == PUSH_DONE) return;
    if (g_push_cmd) {
        if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (t >= 0) {
                ttgw_item_t *it = log_new(t, 'c');
                it->u.m.ver = (uint16_t)(g_cmd_bytes[0] | g_cmd_bytes[1] << 8); it->u.m.ok = ok; it->u.m.err = err;
                it->u.m.ms = now_ms() - g_push_t0;
            }
            xSemaphoreGive(g_lock);
        }
        g_cmd_last_ok = ok; g_cmd_last_err = err; g_cmd_last = g_cmd_bytes[0];
        if (ok || ++g_cmd_tries >= TTGW_PUSH_TRIES) g_cmd_pending = 0;   /* refused by the tag (MAC, old counter): no retry helps */
        if (err >= 1 && err <= 3) g_cmd_pending = 0;
        Report("\r\n[ttgw] command %u to tag %d: %s (code %u)\r\n", g_cmd_bytes[0], t, ok ? "delivered" : "failed", err);
        if (g_push_conn != 0xFFFF) { g_push = PUSH_DONE; ble_gap_terminate(g_push_conn, BLE_ERR_REM_USER_CONN_TERM); }
        else { g_push = PUSH_IDLE; g_scan_restart = 1; }
        return;
    }
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (t >= 0) {
            ttgw_item_t *it = log_new(t, 'm');
            it->u.m.ver = g_pkg_ver[0]; it->u.m.ok = ok; it->u.m.err = err; it->u.m.ms = now_ms() - g_push_t0;
            if (ok) g_push_tries[t] = 0; else g_push_tries[t]++;
            g_push_next_ms[t] = now_ms() + (ok ? 30000u : 15000u);   /* give the tag time to advertise its new version */
        }
        xSemaphoreGive(g_lock);
    }
    if (ok) g_pushes_ok++; else g_pushes_failed++;
    g_push_ok = ok; g_push_err = err;
    Report("\r\n[ttgw] model v%u on tag %d: %s (code %u, %lu ms)\r\n", g_pkg_ver[0], t, ok ? "installed" : "failed", err,
           (unsigned long)(now_ms() - g_push_t0));
    if (g_push_conn != 0xFFFF) { g_push = PUSH_DONE; ble_gap_terminate(g_push_conn, BLE_ERR_REM_USER_CONN_TERM); }
    else { g_push = PUSH_IDLE; g_scan_restart = 1; }
}

static int push_write_cb(uint16_t conn, const struct ble_gatt_error *e, struct ble_gatt_attr *a, void *arg);

/* The next write: BEGIN, DATA chunks, COMMIT with the MAC, for tree 0 then tree 1; then read the result. */
static void push_next(void)
{
    if (g_push_cmd) {                                  /* one 18-byte write, then read the tag's verdict */
        if (g_push_phase == 0) {
            g_push_phase = 1;
            if (ble_gattc_write_flat(g_push_conn, g_push_mdl_h, g_cmd_bytes, 18, push_write_cb, NULL) != 0) push_end(0, 20);
        } else {
            g_push = PUSH_READ; g_push_reads = 0; g_push_read_at = now_ms() + 300;
        }
        return;
    }
    const uint8_t *pkg = g_pkg[g_push_k];
    uint16_t plen = g_pkg_len[g_push_k], body = (uint16_t)(plen - 6 - 16), c;
    uint8_t n;
    if (g_push_phase == 0) {
        g_push_buf[0] = 1; memcpy(&g_push_buf[1], pkg, 6); n = 7; g_push_off = 0; g_push_phase = 1;
    } else if (g_push_phase == 1) {
        c = (uint16_t)(body - g_push_off); if (c > 17) c = 17;
        g_push_buf[0] = 2; g_push_buf[1] = (uint8_t)g_push_off; g_push_buf[2] = (uint8_t)(g_push_off >> 8);
        memcpy(&g_push_buf[3], pkg + 6 + g_push_off, c); n = (uint8_t)(3 + c);
        g_push_off = (uint16_t)(g_push_off + c);
        if (g_push_off >= body) g_push_phase = 2;
    } else if (g_push_phase == 2) {
        g_push_buf[0] = 3; memcpy(&g_push_buf[1], pkg + plen - 16, 16); n = 17; g_push_phase = 3;
    } else {
        if (g_push_k == 0) { g_push_k = 1; g_push_phase = 0; push_next(); return; }
        g_push = PUSH_READ; g_push_reads = 0; g_push_read_at = now_ms() + 600;    /* the tag checks the MAC meanwhile */
        return;
    }
    if (ble_gattc_write_flat(g_push_conn, g_push_mdl_h, g_push_buf, n, push_write_cb, NULL) != 0) push_end(0, 20);
}

static int push_write_cb(uint16_t conn, const struct ble_gatt_error *e, struct ble_gatt_attr *a, void *arg)
{
    (void)conn; (void)a; (void)arg;
    if (g_push != PUSH_WRITE) return 0;
    if (e->status) push_end(0, (uint8_t)(e->status & 0xFF));         /* the tag's own codes are 0x81 to 0x86 */
    else push_next();
    return 0;
}

static int push_read_cb(uint16_t conn, const struct ble_gatt_error *e, struct ble_gatt_attr *a, void *arg)
{
    uint8_t st[8];
    uint16_t len = 0, v0, v1;
    (void)conn; (void)arg;
    if (g_push != PUSH_READ) return 0;
    if (g_push_cmd) {                                  /* counter(4) result(1): 0 passed to the tool, 7 still checking */
        uint32_t want = (uint32_t)g_cmd_bytes[2] | (uint32_t)g_cmd_bytes[3] << 8 | (uint32_t)g_cmd_bytes[4] << 16 | (uint32_t)g_cmd_bytes[5] << 24;
        if (e->status || !a || ble_hs_mbuf_to_flat(a->om, st, sizeof(st), &len) != 0 || len < 5) { push_end(0, 21); return 0; }
        if (st[4] == 7 && g_push_reads < 10) { g_push_read_at = now_ms() + 300; return 0; }
        push_end((uint8_t)(st[4] == 0 && ((uint32_t)st[0] | (uint32_t)st[1] << 8 | (uint32_t)st[2] << 16 | (uint32_t)st[3] << 24) == want), st[4]);
        return 0;
    }
    if (e->status || !a || ble_hs_mbuf_to_flat(a->om, st, sizeof(st), &len) != 0 || len < 8) { push_end(0, 21); return 0; }
    v0 = (uint16_t)(st[0] | st[1] << 8); v1 = (uint16_t)(st[2] | st[3] << 8);
    if (st[5] == 7 && g_push_reads < 8) { g_push_read_at = now_ms() + 300; return 0; }   /* still checking the MAC */
    push_end((uint8_t)(st[4] == 3 && v0 == g_pkg_ver[0] && v1 == g_pkg_ver[1]), st[5]);
    return 0;
}

static int push_disc_cb(uint16_t conn, const struct ble_gatt_error *e, const struct ble_gatt_chr *chr, void *arg)
{
    (void)conn; (void)arg;
    if (e->status == 0 && chr) {
        if (g_push == PUSH_DISC_MDL) g_push_mdl_h = chr->val_handle; else if (g_push == PUSH_DISC_STA) g_push_sta_h = chr->val_handle;
        return 0;
    }
    if (e->status != BLE_HS_EDONE) { push_end(0, 30); return 0; }
    if (g_push == PUSH_DISC_MDL) {
        if (!g_push_mdl_h) { push_end(0, 31); return 0; }             /* no model service: older tag firmware */
        g_push = PUSH_DISC_STA;
        if (ble_gattc_disc_chrs_by_uuid(g_push_conn, 1, 0xFFFF, g_push_cmd ? BLE_UUID16_DECLARE(0xFAB4) : BLE_UUID16_DECLARE(0xFAB2),
                                        push_disc_cb, NULL)) push_end(0, 32);
    } else if (g_push == PUSH_DISC_STA) {
        if (!g_push_sta_h) { push_end(0, 33); return 0; }
        g_push = PUSH_WRITE; g_push_k = 0; g_push_phase = 0;
        push_next();                                     /* a model: BEGIN; a command: its one write */
    }
    return 0;
}

static int push_gap_cb(struct ble_gap_event *ev, void *arg)
{
    (void)arg;
    if (ev->type == BLE_GAP_EVENT_CONNECT) {
        if (ev->connect.status) { g_push_conn = 0xFFFF; push_end(0, 40); return 0; }
        g_push_conn = ev->connect.conn_handle; g_push_mdl_h = g_push_sta_h = 0;
        g_push = PUSH_DISC_MDL;
        if (ble_gattc_disc_chrs_by_uuid(g_push_conn, 1, 0xFFFF, g_push_cmd ? BLE_UUID16_DECLARE(0xFAB3) : BLE_UUID16_DECLARE(0xFAB1),
                                        push_disc_cb, NULL)) push_end(0, 34);
    } else if (ev->type == BLE_GAP_EVENT_DISCONNECT) {
        if (g_push != PUSH_DONE && g_push != PUSH_IDLE) { g_push_conn = 0xFFFF; push_end(0, 41); }
        g_push_conn = 0xFFFF; g_push = PUSH_IDLE; g_scan_restart = 1;
    }
    return 0;
}

/* Main task, every 200 ms: start an install when a tag reports another version, read results, give up after 20 s. */
static void push_poll(void)
{
    uint32_t now = now_ms();
    if (g_push == PUSH_IDLE && !g_cmd_pending && g_lease) {   /* a lease due for a tool: queue it like any command */
        int k;
        for (k = 0; k < TTGW_LEASE_TAGS; k++) {
            ttgw_lease_t *l = &g_lease[k];
            if (!l->used || l->next >= l->n || (int32_t)(now - l->due_ms) < 0) continue;
            memcpy(g_cmd_addr, l->addr, 6); memcpy(g_cmd_bytes, l->cmd[l->next++], 18);
            l->due_ms = now + 1000u * l->every_s;
            g_cmd_tries = 0; g_cmd_queued_ms = now; g_cmd_pending = 1;
            break;
        }
    }
    if (g_push == PUSH_IDLE && g_cmd_pending) {      /* a command goes before any model install */
        int i, pick = -1;
        ble_addr_t peer;
        if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(50)) != pdTRUE) return;
        for (i = 0; i < TTGW_MAX_TAGS && pick < 0; i++)
            if (g_tags[i].used && !memcmp(g_tags[i].addr, g_cmd_addr, 6) && now - g_tags[i].last_ms < 5000) pick = i;
        xSemaphoreGive(g_lock);
        if (pick < 0) {                                  /* not in range: wait for it, but not forever */
            if (now - g_cmd_queued_ms > 60000) { g_cmd_pending = 0; g_cmd_last_ok = 0; g_cmd_last_err = 50; g_cmd_last = g_cmd_bytes[0]; }
            return;
        }
        peer.type = BLE_ADDR_PUBLIC; memcpy(peer.val, g_cmd_addr, 6);
        g_push_cmd = 1; g_push_tag = pick; g_push_t0 = now; g_push_conn = 0xFFFF; g_push_phase = 0; g_push = PUSH_CONNECT;
        ble_gap_disc_cancel();
        if (ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer, 4000, NULL, push_gap_cb, NULL) != 0) push_end(0, 60);
        return;
    }
    if (g_push == PUSH_IDLE) {
        int i, pick = -1;
        ble_addr_t peer;
        if (!g_pkg_len[0] || !g_pkg_len[1] || g_pkg_ver[0] != g_pkg_ver[1]) return;
        if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(50)) != pdTRUE) return;
        for (i = 0; i < TTGW_MAX_TAGS && pick < 0; i++) {
            const ttgw_tag_t *t = &g_tags[i];
            if (t->used && t->app == 1 && t->has_tag && t->tag.fw >= 2 && t->tag.mv != g_pkg_ver[0] && now - t->last_ms < 3000
                && g_push_tries[i] < TTGW_PUSH_TRIES && (int32_t)(now - g_push_next_ms[i]) >= 0) {
                pick = i; peer.type = BLE_ADDR_PUBLIC; memcpy(peer.val, t->addr, 6);
            }
        }
        xSemaphoreGive(g_lock);
        if (pick < 0) return;
        g_push_cmd = 0; g_push_tag = pick; g_push_t0 = now; g_push_conn = 0xFFFF; g_push = PUSH_CONNECT;
        ble_gap_disc_cancel();
        if (ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer, 4000, NULL, push_gap_cb, NULL) != 0) push_end(0, 60);
        return;
    }
    if (g_push == PUSH_READ && g_push_read_at && (int32_t)(now - g_push_read_at) >= 0) {
        g_push_read_at = 0; g_push_reads++;
        if (ble_gattc_read(g_push_conn, g_push_sta_h, push_read_cb, NULL) != 0) push_end(0, 22);
    }
    if (g_push != PUSH_IDLE && g_push != PUSH_DONE && now - g_push_t0 > 20000) {
        if (g_push == PUSH_CONNECT) ble_gap_conn_cancel();
        push_end(0, 50);
    }
}

/* ---------- HTTP ---------- */

/* Responses are written into memory while g_lock is held and sent after it is released, so a slow or vanished
   client can never hold up the Bluetooth side. */
#define TTGW_OUT_MAX (16 * 1024)
typedef struct { char *b; size_t n, cap; } out_t;
#define TTGW_WORKERS 2               /* requests served at once: a phone's app and one more client */
static char *g_resp[TTGW_WORKERS];   /* TTGW_OUT_MAX bytes each, from the FreeRTOS heap (326 kB): static RAM is full */
static QueueHandle_t g_conns;

static void out_put(out_t *o, const char *p, size_t n)
{
    if (o->n + n > o->cap) n = o->cap - o->n;     /* truncates; api_log pages keep well under the cap */
    memcpy(o->b + o->n, p, n); o->n += n;
}
static void out_send(int s, const char *p, size_t n)
{
    while (n) {
        int k = send(s, p, n > 2920 ? 2920 : n, 0);
        if (k <= 0) return;
        p += k; n -= (size_t)k;
    }
}
static void out_f(out_t *o, const char *fmt, ...)
{
    char t[256]; va_list ap; int n;
    va_start(ap, fmt); n = vsnprintf(t, sizeof(t), fmt, ap); va_end(ap);
    if (n > 0) out_put(o, t, (size_t)n < sizeof(t) ? (size_t)n : sizeof(t) - 1);
}

static void addr_str(const uint8_t *a, char *s)
{
    sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}

static void json_esc(out_t *o, const char *s)
{
    out_put(o, "\"", 1);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') out_put(o, "\\", 1);
        if ((unsigned char)*s >= 0x20) out_put(o, s, 1);
    }
    out_put(o, "\"", 1);
}

static void item_json(out_t *o, const ttgw_item_t *it)
{
    char a[18], upbuf[16];
    long up = (long)it->up;
    int known = 1;
    uint32_t unix_t = 0;
    addr_str(g_tags[it->tag].addr, a);
    if (it->kind == 'e') {
        if (it->u.e.age != 0xFFFFFFFFu) up -= (long)it->u.e.age;   /* may be negative: before this gateway started */
        else known = 0;                                             /* before the tag's last restart: time unknown */
    }
    if (g_unix_base && known) unix_t = (uint32_t)((long)g_unix_base + up);
    if (known) snprintf(upbuf, sizeof(upbuf), "%ld", up); else strcpy(upbuf, "null");
    out_f(o, "{\"i\":%lu,\"k\":\"%c\",\"a\":\"%s\",\"up\":%s,\"unix\":%lu,", (unsigned long)it->idx, it->kind, a,
          upbuf, (unsigned long)unix_t);
    if (it->kind == 's')
        out_f(o, "\"seq\":%u,\"st\":%u,\"trig\":%lu,\"oc\":%u,\"flt\":%u,\"run\":%lu,\"rpm\":%u,\"soc\":%u,\"tc\":%d}",
              it->u.s.seq, it->u.s.st, (unsigned long)it->u.s.trig, it->u.s.oc, it->u.s.flt, (unsigned long)it->u.s.run,
              it->u.s.rpm, it->u.s.soc, it->u.s.tc);
    else if (it->kind == 'e')
        out_f(o, "\"id\":%u,\"type\":%u,\"v\":%u}", it->u.e.id, it->u.e.type, it->u.e.v);
    else if (it->kind == 'c')
        out_f(o, "\"cmd\":%u,\"duty\":%u,\"ok\":%u,\"err\":%u,\"ms\":%lu}", it->u.m.ver & 0xFF, it->u.m.ver >> 8, it->u.m.ok,
              it->u.m.err, (unsigned long)it->u.m.ms);
    else if (it->kind == 'm')
        out_f(o, "\"ver\":%u,\"ok\":%u,\"err\":%u,\"ms\":%lu}", it->u.m.ver, it->u.m.ok, it->u.m.err, (unsigned long)it->u.m.ms);
    else if (it->kind == 'g') {
        const fd_fields_t *g = &g_gpay[it->u.g.gen % TTGW_GPAY];
        int k;
        if (g_gpay_next - it->u.g.gen > TTGW_GPAY) {     /* its fields were overwritten: say so, never send others' */
            out_f(o, "\"app\":%u,\"seq\":%u,\"f\":null,\"lost\":1}", it->u.g.app, it->u.g.seq);
            return;
        }
        out_f(o, "\"app\":%u,\"seq\":%u,\"f\":{", it->u.g.app, it->u.g.seq);
        for (k = 0; k < g->n; k++) out_f(o, "%s\"%u\":%ld", k ? "," : "", g->id[k], (long)g->v[k]);
        out_put(o, "}}", 2);
    }
    else
        out_f(o, "\"pid\":%u,\"dur\":%lu,\"ravg\":%u,\"rmax\":%u,\"iavg\":%u,\"ipk\":%u,\"vmin\":%u,\"tc\":%d,\"cls\":%u}",
              it->u.p.id, (unsigned long)it->u.p.dur10 * 10UL, it->u.p.ravg, it->u.p.rmax, it->u.p.iavg, it->u.p.ipk,
              it->u.p.vmin, it->u.p.tc, it->u.p.cls);
}

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

/* POST /api/model: one package as hex. Both trees at one version start the installs. */
static void api_model_post(out_t *o, const char *body)
{
    uint8_t p[TTGW_PKG_MAX + 16];
    size_t n = 0;
    int hi, lo, k;
    while (*body == '\r' || *body == '\n' || *body == ' ') body++;
    while ((hi = hexval(body[0])) >= 0 && (lo = hexval(body[1])) >= 0 && n < sizeof(p)) { p[n++] = (uint8_t)(hi << 4 | lo); body += 2; }
    if (n < 6 + 5 + 16 || p[0] > 1 || p[3] == 0 || p[3] > 40 || n != (size_t)(6 + 5 * p[3] + 16)) {
        out_f(o, "{\"ok\":0,\"error\":\"not a model package\"}");
        return;
    }
    k = p[0];
    if (g_pkg_len[k] && (uint16_t)(p[1] | p[2] << 8) < g_pkg_ver[k]) {           /* never step back to an older model */
        out_f(o, "{\"ok\":0,\"error\":\"older than v%u\"}", g_pkg_ver[k]);
        return;
    }
    if (g_push != PUSH_IDLE) { out_f(o, "{\"ok\":0,\"error\":\"an install is in progress, try again\"}"); return; }
    if (!g_pkg_len[k] || (uint16_t)(p[1] | p[2] << 8) != g_pkg_ver[k]) {       /* a new version: every tag gets fresh tries */
        memset(g_push_tries, 0, sizeof(g_push_tries)); memset(g_push_next_ms, 0, sizeof(g_push_next_ms));
    }
    memcpy(g_pkg[k], p, n); g_pkg_len[k] = (uint16_t)n; g_pkg_ver[k] = (uint16_t)(p[1] | p[2] << 8);   /* serve() holds g_lock */
    out_f(o, "{\"ok\":1,\"kind\":%d,\"ver\":%u,\"ready\":%d}", k, g_pkg_ver[k],
          g_pkg_len[0] && g_pkg_len[1] && g_pkg_ver[0] == g_pkg_ver[1]);
}

/* POST /api/cmd body "addr=40:F3:B0:xx:xx:xx&cmd=<36 hex digits>": a command the home base signed for that tag. */
static void api_cmd_post(out_t *o, const char *body)
{
    const char *a = strstr(body, "addr="), *c = strstr(body, "cmd=");
    unsigned v[6];
    int i, hi, lo;
    if (!a || !c || sscanf(a + 5, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        out_f(o, "{\"ok\":0,\"error\":\"addr and cmd needed\"}"); return;
    }
    if (g_push != PUSH_IDLE || g_cmd_pending) { out_f(o, "{\"ok\":0,\"error\":\"busy, try again\"}"); return; }
    c += 4;
    for (i = 0; i < 18; i++, c += 2) {
        if ((hi = hexval(c[0])) < 0 || (lo = hexval(c[1])) < 0) { out_f(o, "{\"ok\":0,\"error\":\"cmd is 36 hex digits\"}"); return; }
        g_cmd_bytes[i] = (uint8_t)(hi << 4 | lo);
    }
    for (i = 0; i < 6; i++) g_cmd_addr[i] = (uint8_t)v[5 - i];     /* shown most significant first, stored as advertised */
    g_cmd_tries = 0; g_cmd_queued_ms = now_ms(); g_cmd_pending = 1;
    out_f(o, "{\"ok\":1,\"queued\":1}");
}

/* POST /api/lease body "addr=40:F3:B0:xx:xx:xx&every=60&cmds=<36 hex digits per lease, up to 20>", or
   "addr=...&stop=1" to stop handing leases to that tool (what leaving the site looks like to the tag). */
static void api_lease_post(out_t *o, const char *body)
{
    const char *a = strstr(body, "addr="), *c = strstr(body, "cmds="), *e = strstr(body, "every=");
    unsigned v[6];
    uint8_t ad[6];
    int i, k, slot = -1, hi, lo, n = 0;
    if (!g_lease || !a || sscanf(a + 5, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        out_f(o, "{\"ok\":0,\"error\":\"addr needed\"}"); return;
    }
    for (i = 0; i < 6; i++) ad[i] = (uint8_t)v[5 - i];
    for (k = 0; k < TTGW_LEASE_TAGS; k++) if (g_lease[k].used && !memcmp(g_lease[k].addr, ad, 6)) slot = k;
    if (strstr(body, "stop=1")) { if (slot >= 0) g_lease[slot].used = 0; out_f(o, "{\"ok\":1,\"stopped\":%d}", slot >= 0); return; }
    if (!c) { out_f(o, "{\"ok\":0,\"error\":\"cmds needed\"}"); return; }
    if (slot < 0) for (k = 0; k < TTGW_LEASE_TAGS && slot < 0; k++) if (!g_lease[k].used) slot = k;
    if (slot < 0) { out_f(o, "{\"ok\":0,\"error\":\"no free lease slot\"}"); return; }
    {
        ttgw_lease_t *l = &g_lease[slot];
        c += 5;
        while (n < TTGW_LEASES) {
            for (i = 0; i < 18; i++, c += 2) {
                if ((hi = hexval(c[0])) < 0 || (lo = hexval(c[1])) < 0) break;
                l->cmd[n][i] = (uint8_t)(hi << 4 | lo);
            }
            if (i < 18) break;
            n++;
        }
        if (!n) { out_f(o, "{\"ok\":0,\"error\":\"no whole lease in cmds\"}"); return; }
        memcpy(l->addr, ad, 6); l->n = (uint8_t)n; l->next = 0;
        l->every_s = (uint16_t)(e ? atoi(e + 6) : 60); if (l->every_s < 10) l->every_s = 10;
        l->due_ms = now_ms(); l->used = 1;
        out_f(o, "{\"ok\":1,\"leases\":%d,\"every\":%u}", n, l->every_s);
    }
}

static void api_cmd(out_t *o)
{
    int k, first = 1;
    out_f(o, "{\"pending\":%u,\"last\":{\"cmd\":%u,\"ok\":%u,\"err\":%u},\"leases\":[", g_cmd_pending, g_cmd_last, g_cmd_last_ok, g_cmd_last_err);
    for (k = 0; g_lease && k < TTGW_LEASE_TAGS; k++) {
        char a[18];
        if (!g_lease[k].used) continue;
        addr_str(g_lease[k].addr, a);
        out_f(o, "%s{\"addr\":\"%s\",\"left\":%u,\"every\":%u}", first ? "" : ",", a, g_lease[k].n - g_lease[k].next, g_lease[k].every_s);
        first = 0;
    }
    out_put(o, "]}", 2);
}

static void api_model(out_t *o)
{
    int i, have = 0, done = 0;
    for (i = 0; i < TTGW_MAX_TAGS; i++)
        if (g_tags[i].used && g_tags[i].app == 1 && g_tags[i].has_tag) { have++; done += g_pkg_len[0] && g_tags[i].tag.mv == g_pkg_ver[0]; }
    out_f(o, "{\"ver\":[%u,%u],\"ready\":%d,\"state\":%u,\"tags\":%d,\"on_version\":%d,\"installs_ok\":%lu,\"installs_failed\":%lu,"
             "\"last\":{\"ok\":%u,\"err\":%u}}", g_pkg_len[0] ? g_pkg_ver[0] : 0, g_pkg_len[1] ? g_pkg_ver[1] : 0,
          g_pkg_len[0] && g_pkg_len[1] && g_pkg_ver[0] == g_pkg_ver[1], g_push, have, done, (unsigned long)g_pushes_ok,
          (unsigned long)g_pushes_failed, g_push_ok, g_push_err);
}

static void api_info(out_t *o)
{
    int i, n = 0;
    for (i = 0; i < TTGW_MAX_TAGS; i++) n += g_tags[i].used;
    out_f(o, "{\"heap\":%u,\"heapmin\":%u,", (unsigned)xPortGetFreeHeapSize(), (unsigned)xPortGetMinimumEverFreeHeapSize());
    out_f(o, "\"hub\":\"%s\",\"fw\":\"%s\",\"up\":%lu,\"unix\":%lu,\"tags\":%d,\"recs\":%lu,\"first\":%lu,",
          g_hub, TTGW_FW, (unsigned long)now_s(), (unsigned long)(g_unix_base ? g_unix_base + now_s() : 0), n,
          (unsigned long)g_log_next, (unsigned long)(g_log_next > TTGW_LOG_LEN ? g_log_next - TTGW_LOG_LEN : 0));
    out_f(o, "\"scan\":{\"adv\":%lu,\"ours\":%lu,\"legacy\":%lu,\"ext\":%lu,\"dropped\":%lu,\"restarts\":%lu,\"mismatched\":%lu}",
          (unsigned long)g_adv_total, (unsigned long)g_adv_ours, (unsigned long)g_adv_legacy, (unsigned long)g_adv_ext,
          (unsigned long)g_adv_dropped, (unsigned long)g_scan_restarts, (unsigned long)g_adv_mismatched);
#ifdef TTGW_SITE_JSON
    out_f(o, "%s", TTGW_SITE_JSON);                  /* where this hub stands, written at build time */
#endif
#ifdef TTGW_UPLINK_URL
    out_f(o, ",\"uplink\":{\"sent\":%lu,\"posts\":%lu,\"failed\":%lu,\"behind\":%lu}", (unsigned long)g_up_items,
          (unsigned long)g_up_ok, (unsigned long)g_up_fail, (unsigned long)(g_log_next - g_up_next));
#endif
    out_put(o, "}", 1);
}

static void api_tags(out_t *o)
{
    int i, first = 1;
    uint32_t now = now_ms();
    out_put(o, "[", 1);
    for (i = 0; i < TTGW_MAX_TAGS; i++) {
        const ttgw_tag_t *t = &g_tags[i];
        char a[18];
        if (!t->used) continue;
        addr_str(t->addr, a);
        out_f(o, "%s{\"addr\":\"%s\",\"app\":%u,", first ? "" : ",", a, t->app ? t->app : 1);
        if (t->fd) {
            int k;
            out_put(o, "\"f\":{", 5);
            for (k = 0; k < t->fd->n; k++) out_f(o, "%s\"%u\":%ld", k ? "," : "", t->fd->id[k], (long)t->fd->v[k]);
            out_put(o, "},", 2);
        }
        out_f(o, "\"name\":");
        json_esc(o, t->name);
        out_f(o, ",\"tool\":");
        json_esc(o, t->tool.id);
        out_f(o, ",\"rssi\":%d,\"age\":%lu,\"nl\":%lu,\"ne\":%lu,\"trig\":%lu,\"oc\":%u,\"flt\":%u,\"run\":%lu,",
              t->rssi, (unsigned long)((now - t->last_ms) / 1000), (unsigned long)t->nl, (unsigned long)t->ne,
              (unsigned long)t->tool.trig, t->tool.oc, t->tool.flt, (unsigned long)t->tool.run);
        out_f(o, "\"rpm\":%u,\"rpmx\":%u,\"ipk\":%u,\"vb\":%u,\"tc\":%d,\"st\":%u,\"flags\":%u,\"soc\":%u,\"cyc\":%u,"
                 "\"bvb\":%u,\"btc\":%d,", t->tool.rpm, t->tool.rpmx, t->tool.ipk, t->tool.vb, t->tool.tc, t->tool.st,
              t->flags, t->tool.has_bat ? t->tool.soc : 255, t->tool.cyc, t->tool.bvb, t->tool.btc);
        out_f(o, "\"pulls\":%lu,\"pmiss\":%lu,", (unsigned long)t->pulls, (unsigned long)t->pulls_missed);
        if (t->has_tag)
            out_f(o, "\"tup\":%lu,\"link\":%u,\"rxok\":%u,\"rxbad\":%u,\"fw\":%u,\"mv\":%u}", (unsigned long)t->tag.up, t->tag.link,
                  t->tag.rx_ok, t->tag.rx_bad, t->tag.fw, t->tag.mv);
        else
            out_f(o, "\"tup\":null,\"link\":null,\"rxok\":null,\"rxbad\":null}");
        first = 0;
    }
    out_put(o, "]", 1);
}

static void api_log(out_t *o, uint32_t since)
{
    uint32_t first = g_log_next > TTGW_LOG_LEN ? g_log_next - TTGW_LOG_LEN : 0, i, end, lost = 0;
    if (since < first) { lost = first - since; since = first; }
    end = g_log_next;
    if (end - since > 80) end = since + 80;            /* page size (under 16 kB): the app asks again with "next" */
    out_f(o, "{\"next\":%lu,\"lost\":%lu,\"more\":%d,\"items\":[", (unsigned long)end, (unsigned long)lost,
          end < g_log_next);
    for (i = since; i < end; i++) {
        if (i != since) out_put(o, ",", 1);
        item_json(o, &g_log[i % TTGW_LOG_LEN]);
    }
    out_put(o, "]}", 2);
}

/* Each worker owns its buffers: two workers serve at once, so nothing here may be static. */
typedef struct { char req[1024]; char head[256]; char *resp; } ttgw_worker_t;
static ttgw_worker_t g_workers[TTGW_WORKERS];

static void serve(int s, ttgw_worker_t *w)
{
    char *req = w->req, *head = w->head;
    out_t o = { w->resp, 0, TTGW_OUT_MAX };
    int n = 0, r, hn;
    char *path, *sp, *body;
    /* One request at a time, so an idle connection must not hold the server: browsers open spare connections and
       may never send on them. 400 ms is long enough for a request from a phone on the same access point. */
    struct timeval tv = { 0, 400000 }, stv = { 3, 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &stv, sizeof(stv));
    while (n < (int)sizeof(w->req) - 1 && (r = recv(s, req + n, sizeof(w->req) - 1 - n, 0)) > 0) {
        n += r; req[n] = 0;
        if ((body = strstr(req, "\r\n\r\n")) != NULL) {
            char *cl = strstr(req, "Content-Length:");
            int want = cl ? atoi(cl + 15) : 0;
            if (n - (int)(body + 4 - req) >= want) break;
        }
    }
    if (n <= 0) return;
    req[n] = 0;
    path = strchr(req, ' ');
    if (!path) return;
    path++;
    sp = strchr(path, ' ');
    if (sp) *sp = 0;
    body = sp ? strstr(sp + 1, "\r\n\r\n") : NULL;
    if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
        hn = snprintf(head, sizeof(w->head), "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Encoding: gzip\r\n"
                      "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: %u\r\n\r\n", (unsigned)sizeof(tt_app_gz));
        out_send(s, head, (size_t)hn);
        out_send(s, (const char *)tt_app_gz, sizeof(tt_app_gz));
        return;
    }
    if (!strncmp(req, "OPTIONS", 7)) {
        static const char opt[] = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST\r\n"
                                  "Access-Control-Allow-Headers: Content-Type, X-Token\r\nConnection: close\r\n\r\n";
        out_send(s, opt, sizeof(opt) - 1);
        return;
    }
#ifdef TTGW_CTRL_TOKEN                                 /* SECURE=1 builds only; bench builds leave these open */
    if (!strncmp(req, "POST", 4) && (!strcmp(path, "/api/model") || !strcmp(path, "/api/cmd") || !strcmp(path, "/api/lease"))) {
        /* models, commands and leases change what tools do: only the home base, which holds the token, may post them */
        const char *h = NULL, *q;
        for (q = sp ? strstr(sp + 1, "\r\n") : NULL; q && !h; q = strstr(q + 2, "\r\n")) {   /* headers follow the cut path; names in any case */
            static const char nm[] = "x-token: ";
            size_t k;
            for (k = 0; k < 9 && q[2 + k] && (q[2 + k] | (q[2 + k] >= 'A' && q[2 + k] <= 'Z' ? 0x20 : 0)) == nm[k]; k++) {}
            if (k == 9) h = q;
        }
        const char *want = TTGW_CTRL_TOKEN;
        size_t i, wl = strlen(want);
        uint8_t diff = h ? 0 : 1;
        if (h) { h += 11; for (i = 0; i < wl; i++) diff |= (uint8_t)(h[i] ^ want[i]); diff |= (uint8_t)(h[wl] != '\r'); }
        if (diff) {
            static const char no[] = "HTTP/1.1 401 Unauthorized\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: 27\r\n\r\n{\"ok\":0,\"error\":\"token\"}";
            out_send(s, no, sizeof(no) - 1);
            return;
        }
    }
#endif
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(500)) != pdTRUE) return;
    if (!strcmp(path, "/api/info")) api_info(&o);
    else if (!strcmp(path, "/api/tags")) api_tags(&o);
    else if (!strncmp(path, "/api/log", 8)) {
        char *q = strstr(path, "since=");
        api_log(&o, q ? (uint32_t)strtoul(q + 6, NULL, 10) : 0);
    } else if (!strcmp(path, "/api/model") && !strncmp(req, "POST", 4) && body) {
        api_model_post(&o, body + 4);
    } else if (!strcmp(path, "/api/model")) {
        api_model(&o);
    } else if (!strcmp(path, "/api/cmd") && !strncmp(req, "POST", 4) && body) {
        api_cmd_post(&o, body + 4);
    } else if (!strcmp(path, "/api/cmd")) {
        api_cmd(&o);
    } else if (!strcmp(path, "/api/lease") && !strncmp(req, "POST", 4) && body) {
        api_lease_post(&o, body + 4);
    } else if (!strcmp(path, "/api/time") && !strncmp(req, "POST", 4) && body) {
        char *u = strstr(body, "unix=");
        uint32_t t = u ? (uint32_t)strtoul(u + 5, NULL, 10) : 0;
        if (t > 1700000000u) g_unix_base = t - now_s();
        out_f(&o, "{\"ok\":%d}", t > 1700000000u);
    } else out_f(&o, "{\"error\":\"no such path\"}");
    xSemaphoreGive(g_lock);
    hn = snprintf(head, sizeof(w->head), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n"
                  "Access-Control-Allow-Origin: *\r\nConnection: close\r\nContent-Length: %u\r\n\r\n", (unsigned)o.n);
    out_send(s, head, (size_t)hn);
    out_send(s, o.b, o.n);
}

static void http_worker(void *arg)
{
    ttgw_worker_t *w = (ttgw_worker_t *)arg;
    int s;
    for (;;) {
        if (xQueueReceive(g_conns, &s, portMAX_DELAY) != pdTRUE) continue;
        serve(s, w);
        shutdown(s, SHUT_RDWR);
        close(s);
    }
}

static void http_task(void *arg)
{
    int ls, s, i;
    struct sockaddr_in a;
    (void)arg;
    for (;;) {
        ls = socket(AF_INET, SOCK_STREAM, 0);
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons(80); a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (ls >= 0 && bind(ls, (struct sockaddr *)&a, sizeof(a)) == 0 && listen(ls, 4) == 0) break;
        if (ls >= 0) close(ls);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    g_conns = xQueueCreate(4, sizeof(int));
    for (i = 0; i < TTGW_WORKERS; i++) {
        g_workers[i].resp = g_resp[i];
        xTaskCreate(http_worker, "ttHttpW", 1536, &g_workers[i], 3, NULL);
    }
    Report("\r\n[ttgw] http listening on port 80 (%d workers)\r\n", TTGW_WORKERS);
    for (;;) {
        s = accept(ls, NULL, NULL);
        if (s < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (xQueueSend(g_conns, &s, pdMS_TO_TICKS(1000)) != pdTRUE) close(s);   /* all busy: refuse rather than queue forever */
    }
}

/* ---------- uploads to the home base, over whatever network the hub joined ---------- */
#ifdef TTGW_UPLINK_URL
#define TTGW_UP_MAX   (12 * 1024)
#define TTGW_UP_ITEMS 50

/* "http://a.b.c.d:port/path": IP addresses only (the hub has no DNS resolver configured yet). */
static int up_target(uint32_t *ip, uint16_t *port, const char **path)
{
    unsigned a, b, c, d, p = 80;
    const char *u = TTGW_UPLINK_URL + 7, *sl = strchr(u, '/');
    if (sscanf(u, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &p) < 4 || a > 255 || b > 255 || c > 255 || d > 255 || !sl) return -1;
    *ip = (uint32_t)(a | b << 8 | c << 16 | d << 24); *port = (uint16_t)p; *path = sl;
    return 0;
}

static void uplink_once(void)
{
    uint32_t sip = 0, mask, gw, dhcp, first, end, ip;
    uint16_t port;
    const char *path;
    out_t o = { g_up_buf, 0, TTGW_UP_MAX - 400 };   /* room left for the request head */
    char head[300], resp[160];
    int s, hn, n = 0, r;
    struct sockaddr_in sa;
    struct timeval tv = { 4, 0 };
    if (network_stack_get_if_ip(WLAN_ROLE_STA, &sip, &mask, &gw, &dhcp) != 0 || !sip) return;   /* no uplink now */
    if (up_target(&ip, &port, &path) != 0) return;
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(200)) != pdTRUE) return;
    first = g_log_next > TTGW_LOG_LEN ? g_log_next - TTGW_LOG_LEN : 0;
    if (g_up_next < first) g_up_next = first;       /* older items left the ring before an uplink came back */
    end = g_log_next; if (end - g_up_next > TTGW_UP_ITEMS) end = g_up_next + TTGW_UP_ITEMS;
    out_f(&o, "{\"via\":\"hub\",\"hub\":\"%s\",\"hub_up\":%lu,\"items\":[", g_hub, (unsigned long)now_s());
    for (uint32_t i = g_up_next; i < end; i++) { if (i != g_up_next) out_put(&o, ",", 1); item_json(&o, &g_log[i % TTGW_LOG_LEN]); }
    out_f(&o, "],\"tags\":");
    api_tags(&o);
    out_put(&o, "}", 1);
    xSemaphoreGive(g_lock);
    if (o.n >= o.cap) { g_up_fail++; return; }       /* never send a cut-off body */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { g_up_fail++; return; }
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    memset(&sa, 0, sizeof(sa)); sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = ip;
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(s); g_up_fail++; return; }
    hn = snprintf(head, sizeof(head), "POST %s HTTP/1.1\r\nHost: %u.%u.%u.%u\r\nContent-Type: application/json\r\n%s%s%s"
                  "Content-Length: %u\r\nConnection: close\r\n\r\n", path, (unsigned)(ip & 255), (unsigned)(ip >> 8 & 255),
                  (unsigned)(ip >> 16 & 255), (unsigned)(ip >> 24), TTGW_UPLINK_TOKEN[0] ? "X-Token: " : "",
                  TTGW_UPLINK_TOKEN, TTGW_UPLINK_TOKEN[0] ? "\r\n" : "", (unsigned)o.n);
    out_send(s, head, (size_t)hn);
    out_send(s, o.b, o.n);
    while (n < (int)sizeof(resp) - 1 && (r = recv(s, resp + n, sizeof(resp) - 1 - n, 0)) > 0) n += r;
    resp[n] = 0;
    close(s);
    if (!strncmp(resp, "HTTP/1.", 7) && strstr(resp, " 200 ")) {      /* stored (or already stored): move on */
        g_up_items += end - g_up_next; g_up_next = end; g_up_ok++;
    } else g_up_fail++;
}

static void uplink_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(20000));
    for (;;) {
        uplink_once();
        vTaskDelay(pdMS_TO_TICKS(g_log_next - g_up_next > TTGW_UP_ITEMS ? 2000 : 30000));   /* catch up fast after a gap */
    }
}
#endif

/* ---------- start-up and the modem line ---------- */

static void modem_print(uint32_t *printed)
{
    static char line[480];
    while (*printed < g_log_next) {
        out_t o = { line, 0, sizeof(line) - 1 };
        uint32_t first = g_log_next > TTGW_LOG_LEN ? g_log_next - TTGW_LOG_LEN : 0;
        if (*printed < first) *printed = first;
        if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(200)) != pdTRUE) return;
        item_json(&o, &g_log[*printed % TTGW_LOG_LEN]);
        xSemaphoreGive(g_lock);
        line[o.n] = 0;
        Report("@L %s\r\n", line);
        (*printed)++;
    }
}

static void main_task(void *arg)
{
    ble_addr_t ba;
    char cmd[160];
    uint32_t ip = 0, mask, gw, dhcp, printed = 0, last_info = 0;
    int rc, tries;
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(1500));
    run_cmd("wlan_start");            /* the radio must be on before Bluetooth starts */
    run_cmd("ble_start");
    for (tries = 0; tries < 50 && !ble_hs_synced(); tries++) vTaskDelay(pdMS_TO_TICKS(100));
    if (ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, ba.val, NULL) == 0)
        snprintf(g_hub, sizeof(g_hub), "FRL-HUB-%02X%02X", ba.val[1], ba.val[0]);

    snprintf(cmd, sizeof(cmd), "wlan_ap_role_up -s \"%s\" -t WPA2 -c %s -p \"%s\"", g_hub, TTGW_AP_CHANNEL, TTGW_AP_PASS);
    run_cmd(cmd);
    network_stack_get_if_ip(WLAN_ROLE_AP, &ip, &mask, &gw, &dhcp);
    Report("\r\n[ttgw] AP %s up, address %lu.%lu.%lu.%lu\r\n", g_hub, (unsigned long)(ip & 0xFF),
           (unsigned long)((ip >> 8) & 0xFF), (unsigned long)((ip >> 16) & 0xFF), (unsigned long)(ip >> 24));
#ifdef TTGW_SITE_SSID
    run_cmd("wlan_sta_role_up");
    snprintf(cmd, sizeof(cmd), "wlan_connect -s \"%s\" -t WPA2 -p \"%s\"", TTGW_SITE_SSID, TTGW_SITE_PASS);
    run_cmd(cmd);
    for (tries = 0; tries < 30; tries++) {
        uint32_t sip = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (network_stack_get_if_ip(WLAN_ROLE_STA, &sip, &mask, &gw, &dhcp) == 0 && sip) {
            Report("\r\n[ttgw] site uplink up, address %lu.%lu.%lu.%lu\r\n", (unsigned long)(sip & 0xFF),
                   (unsigned long)((sip >> 8) & 0xFF), (unsigned long)((sip >> 16) & 0xFF), (unsigned long)(sip >> 24));
            break;
        }
    }
#endif

    xTaskCreate(http_task, "ttHttp", 1536, NULL, 3, NULL);
#ifdef TTGW_UPLINK_URL
    g_up_buf = pvPortMalloc(TTGW_UP_MAX);
    if (g_up_buf) xTaskCreate(uplink_task, "ttUp", 1536, NULL, 2, NULL);
    Report("\r\n[ttgw] uplink to the home base %s\r\n", g_up_buf ? "on" : "off (no heap)");
#endif
    rc = start_scan();
    Report("\r\n[ttgw] scanning (rc=%d)\r\n@I {\"hub\":\"%s\",\"fw\":\"%s\"}\r\n", rc, g_hub, TTGW_FW);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(200));
        push_poll();
        if (g_scan_restart && g_push == PUSH_IDLE) { g_scan_restart = 0; start_scan(); }
        {   /* watchdog: a room is never silent for 10 s, so a frozen count means the scan stopped */
            static uint32_t last_total, last_change_s;
            if (g_adv_total != last_total || g_push != PUSH_IDLE) { last_total = g_adv_total; last_change_s = now_s(); }
            else if (now_s() - last_change_s >= 10) {
                g_scan_restarts++; last_change_s = now_s();
                Report("\r\n[ttgw] no advertising heard for 10 s: restarting the scan (%lu)\r\n", (unsigned long)g_scan_restarts);
                start_scan();
            }
        }
        modem_print(&printed);
#ifdef TTGW_SITE_SSID
        {   /* the site link drops (a router restarts, a hotspot leaves): try again every 30 s while it has no address */
            static uint32_t last_try_s;
            uint32_t sip = 0;
            if (now_s() - last_try_s >= 30 && g_push == PUSH_IDLE &&
                (network_stack_get_if_ip(WLAN_ROLE_STA, &sip, &mask, &gw, &dhcp) != 0 || !sip)) {
                last_try_s = now_s();
                snprintf(cmd, sizeof(cmd), "wlan_connect -s \"%s\" -t WPA2 -p \"%s\"", TTGW_SITE_SSID, TTGW_SITE_PASS);
                run_cmd(cmd);
            }
        }
#endif
        if (now_s() - last_info >= 30) {
            static char ib[400];
            out_t o = { ib, 0, sizeof(ib) - 1 };
            last_info = now_s();
            if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
                api_info(&o);
                xSemaphoreGive(g_lock);
                ib[o.n] = 0;
                Report("@I %s\r\n", ib);
            }
        }
    }
}

void TTGW_start(void)
{
    for (int i = 0; i < TTGW_WORKERS; i++) {
        g_resp[i] = pvPortMalloc(TTGW_OUT_MAX);
        if (!g_resp[i]) { Report("\r\n[ttgw] no heap for response buffer %d; gateway not started\r\n", i); return; }
    }
    g_fdstate = pvPortMalloc(sizeof(fd_fields_t) * TTGW_MAX_TAGS);
    g_gpay = pvPortMalloc(sizeof(fd_fields_t) * TTGW_GPAY);
    if (!g_fdstate || !g_gpay) { Report("\r\n[ttgw] no heap for field-data state; gateway not started\r\n"); return; }
    memset(g_fdstate, 0, sizeof(fd_fields_t) * TTGW_MAX_TAGS);
    g_lease = pvPortMalloc(sizeof(ttgw_lease_t) * TTGW_LEASE_TAGS);
    if (g_lease) memset(g_lease, 0, sizeof(ttgw_lease_t) * TTGW_LEASE_TAGS);
    g_pkg[0] = pvPortMalloc(TTGW_PKG_MAX + 16);
    g_pkg[1] = pvPortMalloc(TTGW_PKG_MAX + 16);
    if (!g_pkg[0] || !g_pkg[1]) { Report("\r\n[ttgw] no heap for model packages; gateway not started\r\n"); return; }
    g_lock = xSemaphoreCreateMutex();
    xTaskCreate(main_task, "ttGw", 2048, NULL, 2, NULL);
}
