/* Tool tag on Zephyr: the same tag as tool-telemetry/tag/ (TI's BLE5-Stack), on any Zephyr board with a Bluetooth LE
 * controller that supports extended advertising. Same UART lines in, same two advertising sets out, same on-tag model,
 * so the same hub, app and home base read it. The format and the model are the shared files ../proto/tt_proto.c and
 * ../ai/out/tt_model.c, compiled unchanged; only this file is Zephyr glue.
 *
 * UART: the console UART (zephyr,console), 115200. Tool lines start with '$'; anything else is ignored.
 * Events and pulls are kept in RAM (the TI build keeps events in flash). */
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/printk.h>
#include "tt_proto.h"
#include "tt_model.h"

#define TT_FW              1
#define TT_LINK_TIMEOUT_S  15

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
K_MSGQ_DEFINE(rx_q, sizeof(char), 512, 1);

static K_MUTEX_DEFINE(lock);
static tt_tool_t   g_tool;
static tt_event_t  g_ev[TT_MAX_EVENTS];
static uint8_t     g_nev;
static uint16_t    g_next_ev = 1;
static tt_pull_t   g_pull[TT_MAX_PULLS];
static uint8_t     g_npull;
static uint16_t    g_next_pull = 1;
static uint8_t     g_seq;
static bool        g_dirty = true;
static int64_t     g_last_line_ms = -1;
static uint16_t    g_rx_ok;
static uint8_t     g_rx_bad;
static char        g_name[17];

static struct bt_le_ext_adv *adv_legacy, *adv_ext;
static uint8_t legacy_buf[TT_LEGACY_LEN], ext_buf[TT_EXT_MAX];

static uint32_t up_s(void) { return (uint32_t)(k_uptime_get() / 1000); }

static void uart_cb(const struct device *dev, void *user)
{
    uint8_t c;
    ARG_UNUSED(user);
    if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) return;
    while (uart_fifo_read(dev, &c, 1) == 1) k_msgq_put(&rx_q, &c, K_NO_WAIT);
}

static void add_event(uint8_t type, uint16_t v)
{
    memmove(&g_ev[1], &g_ev[0], sizeof(g_ev[0]) * (TT_MAX_EVENTS - 1));
    g_ev[0] = (tt_event_t){ .id = g_next_ev++, .type = type, .t = up_s(), .v = v };
    if (g_nev < TT_MAX_EVENTS) g_nev++;
}

static void handle_line(const char *s, size_t len)
{
    tt_line_t l;
    tt_tool_t t;
    k_mutex_lock(&lock, K_FOREVER);
    t = g_tool;
    if (tt_parse_line(s, len, &l, &t) == 0 && l.type[0] == 'T' && strchr("SBEP", l.type[1])) {
        g_tool = t;
        if (l.type[1] == 'E' && l.ev) add_event(l.ev, l.v);
        if (l.type[1] == 'P') {
            tt_pull_t p = l.pull;
            p.id = g_next_pull++;
            if (p.cls == 255) p.cls = tt_model_classify(&p);
            memmove(&g_pull[1], &g_pull[0], sizeof(g_pull[0]) * (TT_MAX_PULLS - 1));
            g_pull[0] = p;
            if (g_npull < TT_MAX_PULLS) g_npull++;
        }
        g_last_line_ms = k_uptime_get();
        g_rx_ok++;
        g_dirty = true;
    } else if (g_rx_bad < 255) {
        g_rx_bad++;
    }
    k_mutex_unlock(&lock);
}

/* Rebuild both advertising payloads and hand them to the controller. */
static void refresh(void)
{
    struct bt_data ad_l[2], ad_e[3];
    size_t nl, ne;
    tt_tagstat_t tag;
    uint32_t now = up_s();
    int link;

    k_mutex_lock(&lock, K_FOREVER);
    if (g_dirty) { g_seq++; g_dirty = false; }
    link = g_last_line_ms >= 0 && (k_uptime_get() - g_last_line_ms) <= TT_LINK_TIMEOUT_S * 1000;
    nl = tt_build_legacy(legacy_buf, g_seq, &g_tool, tt_flags(&g_tool, link, 1));
    tag.up = now;
    tag.link = g_last_line_ms < 0 ? 0xFFFF : (uint16_t)MIN((k_uptime_get() - g_last_line_ms) / 1000, 0xFFFE);
    tag.rx_ok = g_rx_ok; tag.rx_bad = g_rx_bad; tag.fw = TT_FW;
    ne = tt_build_ext(ext_buf, sizeof(ext_buf), g_seq, &g_tool, g_ev, MIN(g_nev, 5), g_pull, g_npull, &tag, g_name);
    k_mutex_unlock(&lock);

    /* The builders write whole AD structures; Zephyr wants them one by one, so split them back up. */
    size_t i = 0, k = 0;
    while (i < nl && k < ARRAY_SIZE(ad_l)) {
        uint8_t n = legacy_buf[i];
        if (n == 0 || legacy_buf[i + 1] == BT_DATA_FLAGS) { i += n + 1; continue; }   /* the stack adds flags itself */
        ad_l[k++] = (struct bt_data){ .type = legacy_buf[i + 1], .data_len = n - 1, .data = &legacy_buf[i + 2] };
        i += n + 1;
    }
    bt_le_ext_adv_set_data(adv_legacy, ad_l, k, NULL, 0);
    i = 0; k = 0;
    while (i < ne && k < ARRAY_SIZE(ad_e)) {
        uint8_t n = ext_buf[i];
        if (n == 0 || ext_buf[i + 1] == BT_DATA_FLAGS) { i += n + 1; continue; }
        ad_e[k++] = (struct bt_data){ .type = ext_buf[i + 1], .data_len = n - 1, .data = &ext_buf[i + 2] };
        i += n + 1;
    }
    bt_le_ext_adv_set_data(adv_ext, ad_e, k, NULL, 0);
}

int main(void)
{
    static char line[TT_LINE_MAX + 2];
    size_t len = 0;
    bool in_line = false;
    bt_addr_le_t addr;
    size_t count = 1;
    int64_t last_refresh = 0, last_status = 0;
    int err;

    memset(&g_tool, 0, sizeof(g_tool));
    g_tool.soc = 0xFF;
    add_event(TT_EV_TAGBOOT, 1);

    err = bt_enable(NULL);
    if (err) { printk("@TAG bt_enable failed %d\n", err); return 0; }
    bt_id_get(&addr, &count);
    snprintf(g_name, sizeof(g_name), "Tag %02X%02X", addr.a.val[1], addr.a.val[0]);

    /* Set 1: legacy, non-connectable, every 500 ms. Set 2: extended, non-connectable, non-scannable, every 1 s. */
    struct bt_le_adv_param p_legacy = BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_USE_IDENTITY, 800,   /* 500 ms in 0.625 ms units */
                                                           800,   /* 500 ms in 0.625 ms units */ NULL);
    struct bt_le_adv_param p_ext = BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_NO_2M,
                                                        1600, 1600, NULL);   /* 1 s */
    p_ext.sid = 1;
    err = bt_le_ext_adv_create(&p_legacy, NULL, &adv_legacy);
    if (!err) err = bt_le_ext_adv_create(&p_ext, NULL, &adv_ext);
    if (err) { printk("@TAG advertising set failed %d\n", err); return 0; }
    refresh();
    bt_le_ext_adv_start(adv_legacy, BT_LE_EXT_ADV_START_DEFAULT);
    bt_le_ext_adv_start(adv_ext, BT_LE_EXT_ADV_START_DEFAULT);
    printk("@TAG boot name=%s zephyr fw=%d board=%s\n", g_name, TT_FW, CONFIG_BOARD);

    uart_irq_callback_user_data_set(uart, uart_cb, NULL);
    uart_irq_rx_enable(uart);

    for (;;) {
        char c;
        while (k_msgq_get(&rx_q, &c, K_MSEC(100)) == 0) {
            if (c == '$') { len = 0; in_line = true; }
            if (!in_line) continue;
            if (c == '\n') { handle_line(line, len); len = 0; in_line = false; }
            else if (len < TT_LINE_MAX) line[len++] = c;
            else { len = 0; in_line = false; }
        }
        int64_t now = k_uptime_get();
        if (now - last_refresh >= 500) { last_refresh = now; refresh(); }
        if (now - last_status >= 10000) {
            last_status = now;
            printk("@TAG up=%u name=%s seq=%u ok=%u bad=%u trig=%u oc=%u run=%u pulls=%u\n", up_s(), g_name, g_seq,
                   g_rx_ok, g_rx_bad, g_tool.trig, g_tool.oc, g_tool.run, (unsigned)(g_next_pull - 1));
        }
    }
}
