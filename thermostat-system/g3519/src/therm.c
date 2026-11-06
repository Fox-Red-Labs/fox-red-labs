/* Thermostat screen for the LP-MSPM0G3519 (thermostat-system/PROTOCOL.md).
 * Radar bytes come in on UART1 (PA9, the wired radar) or UART0 (PA11, the Mac replaying a capture); the same parser
 * reads both. A thermostat screen shows presence, a fixed temperature and a setpoint with plus and minus buttons.
 * Each radar frame also sends one status line out of UART1 (PA8) to the CC35X1, and prints SHOWN,<seq>,<ms>,<us>
 * on UART0 once the frame has been drawn on the panel. */
#include <stdio.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "lvgl.h"
#include "port.h"
#include "board.h"
#include "tlv_parser.h"
#include "app.h"

/* Fox Red Labs palette: Fox Red is the only accent, everything else is warm neutral */
#define C_RED   lv_color_hex(0xB42418)
#define C_EMBER lv_color_hex(0x8E1C13)
#define C_INK   lv_color_hex(0x161312)
#define C_STONE lv_color_hex(0x6B625B)
#define C_PAPER lv_color_hex(0xF4EFE6)
#define C_CREAM lv_color_hex(0xF7F3EC)
#define C_LINE  lv_color_hex(0xE2D8C9)
LV_FONT_DECLARE(frl_barlow_14) LV_FONT_DECLARE(frl_barlow_20) LV_FONT_DECLARE(frl_barlow_48) LV_FONT_DECLARE(frl_barlow_64)
LV_IMAGE_DECLARE(frl_logo);
#define RB 1024u
typedef struct { volatile uint16_t h, t; uint8_t b[RB]; volatile uint32_t total, drops; } ring_t;
static ring_t rb_mac, rb_wire;      /* UART0 (Mac replay), UART1 (wired radar) */
static tlv_parser_t par_mac, par_wire;

static inline void ring_put(ring_t *r, uint8_t c)
{
    uint16_t n = (uint16_t)((r->h + 1u) % RB);
    if (n == r->t) { r->drops++; return; }
    r->b[r->h] = c; r->h = n; r->total++;
}
static inline int ring_get(ring_t *r)
{
    if (r->t == r->h) return -1;
    uint8_t c = r->b[r->t]; r->t = (uint16_t)((r->t + 1u) % RB); return c;
}
void DBG_UART_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(DBG_UART_INST) == DL_UART_MAIN_IIDX_RX)
        while (!DL_UART_Main_isRXFIFOEmpty(DBG_UART_INST)) ring_put(&rb_mac, DL_UART_Main_receiveData(DBG_UART_INST));
}
void LINK_UART_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(LINK_UART_INST) == DL_UART_MAIN_IIDX_RX)
        while (!DL_UART_Main_isRXFIFOEmpty(LINK_UART_INST)) ring_put(&rb_wire, DL_UART_Main_receiveData(LINK_UART_INST));
}

static void link_puts(const char *s)
{
    while (*s) DL_UART_Main_transmitDataBlocking(LINK_UART_INST, (uint8_t)*s++);
}

/* ---- state ---- */
static int presence, zones = -1, zbits = -1;
static int setpoint = 215;                 /* tenths of a degree C */
static const int temp_now = 208;           /* fixed: no sensor is read (see README) */
static uint32_t last_seq, frames_total, frames_prev;
static uint32_t pend_seq; static uint32_t pend_rx_us;   /* waiting for the next REFR_READY */
static uint32_t last_lat_us, lat_max_us;
static const char *src_name = "none yet";
static int wifi_cls = -1, wifi_p100 = 0; static uint32_t wifi_ms; static int wifi_dirty = 1;   /* W,<cls>,<p> lines on UART0 */
static char wl[40]; static int wn;
/* radar details per frame, for the thermostat chip and the sensor page */
static int motion_now, near_mm = -1, rpts, snr10 = -1, zstate[4], fps_radar;
static int wifi_age10; static uint32_t wifi_age_ms;                         /* age in the last W line (tenths of s) and when it arrived */
static uint32_t wres_ms[40]; static int wres_n, wres_prev_age10 = -1;      /* when each new Wi-Fi result was noticed */
#define HIST_FR 116      /* radar strip: one 2 px column per frame, about 30 s */
#define HIST_W  30       /* Wi-Fi strip: one 7 px column per second, 30 s */
static lv_obj_t *scr_th, *scr_sn, *chip, *chip_lbl, *rad_lbl, *wif_lbl, *ch_held, *ch_motion, *ch_wifi;
static lv_chart_series_t *se_held, *se_motion, *se_wifi;

/* ---- widgets ---- */
static lv_obj_t *wbadge, *wbadge_lbl, *badge, *badge_lbl, *frame_lbl, *temp_lbl, *set_lbl, *heat_lbl, *radio_lbl, *stats_lbl;

static void fmt_t(char *o, size_t n, int c10) { snprintf(o, n, "%d.%d C", c10 / 10, c10 % 10); }

static void send_status(void)
{
    char l[64];
    snprintf(l, sizeof l, "T,%lu,%d,%d-%d,%d,%d\n", (unsigned long)last_seq, presence, zones < 0 ? 0 : zones, zbits < 0 ? 0 : zbits, setpoint, temp_now);
    link_puts(l);
}

static void ui_update_setpoint(void)
{
    char b[24]; fmt_t(b, sizeof b, setpoint); lv_label_set_text(set_lbl, b);
    int need = setpoint - temp_now;
    lv_label_set_text(heat_lbl, need > 5 ? "Heating" : (need < -5 ? "Cooling" : "Holding"));
}
static void ui_update_presence(void)
{
    lv_obj_set_style_bg_color(badge, presence ? C_RED : C_INK, 0);
    lv_label_set_text(badge_lbl, presence ? "Someone is here" : "Room is empty");
    char b[64]; snprintf(b, sizeof b, "radar frame %lu", (unsigned long)last_seq);
    lv_label_set_text(frame_lbl, b);
    lv_obj_set_style_bg_color(chip, motion_now ? C_INK : C_CREAM, 0);
    lv_obj_set_style_border_width(chip, motion_now ? 0 : 1, 0);
    lv_obj_set_style_text_color(chip_lbl, motion_now ? C_CREAM : C_STONE, 0);
    lv_label_set_text(chip_lbl, motion_now ? "Motion now" : "No motion");
}

static void sensors_update(void);
static void on_frame(const tlv_frame_t *f, const char *src)
{
    last_seq = f->frame; frames_total++; src_name = src;
    if (f->pres_b0 >= 0) {
        zones = f->pres_b0; zbits = f->pres_z0 < 0 ? 0 : f->pres_z0;
        int z = zones > 4 ? 4 : zones;
        presence = (z > 0 && (zbits & ((1 << (2 * z)) - 1))) ? 1 : 0;
    }
    motion_now = f->pts301 > 0; rpts = (int)f->pts301; near_mm = f->near301_mm; snr10 = f->snr301_x10;
    for (int i = 0; i < 4; i++) zstate[i] = (zones > i && zbits >= 0) ? (zbits >> (2 * i)) & 3 : 0;
    ui_update_presence();
    lv_chart_set_next_value(ch_held, se_held, presence); lv_chart_set_next_value(ch_motion, se_motion, motion_now);
    if (lv_screen_active() == scr_sn) sensors_update();
    pend_seq = last_seq; pend_rx_us = port_us();
    send_status();
}

static void wifi_line(void)
{
    /* W,<cls>,<p>[,<age_s>]  with p like 0.98 and age like 1.3 */
    if (wl[0] != 'W' || wl[1] != ',' || (wl[2] != '0' && wl[2] != '1') || wl[3] != ',') return;
    int ip = 0, v, age = -1; const char *s = wl + 4;
    while (*s >= '0' && *s <= '9') ip = ip * 10 + (*s++ - '0');
    v = ip * 100;
    if (*s == '.') { s++; int d = 0; while (*s >= '0' && *s <= '9') { if (d < 2) v += (*s - '0') * (d ? 1 : 10); d++; s++; } }
    if (*s == ',') {
        s++; int a = 0;
        while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0');
        a *= 10; if (*s == '.' && s[1] >= '0' && s[1] <= '9') a += s[1] - '0';
        age = a;
    }
    wifi_cls = wl[2] - '0'; wifi_p100 = v; wifi_ms = port_ms(); wifi_dirty = 1;
    if (age >= 0) {
        wifi_age10 = age; wifi_age_ms = wifi_ms;
        if (wres_prev_age10 < 0 || age + 5 < wres_prev_age10) {      /* the age fell: a new result arrived */
            if (wres_n < 40) wres_ms[wres_n++] = wifi_ms; else { memmove(wres_ms, wres_ms + 1, 39 * 4); wres_ms[39] = wifi_ms; }
        }
        wres_prev_age10 = age;
    }
}
static void wifi_feed(uint8_t c)
{
    if (c == '\n') { wl[wn] = 0; if (wn > 3) wifi_line(); wn = 0; }
    else if (c >= 0x20 && c < 0x7f && wn < (int)sizeof wl - 1) wl[wn++] = (char)c;
    else if (c != '\r') wn = 0;
}
static void drain(ring_t *r, tlv_parser_t *p, const char *src)
{
    int c, n = 0;
    while (n++ < 2048 && (c = ring_get(r)) >= 0) {
        if (r == &rb_mac) wifi_feed((uint8_t)c);
        if (tlv_feed(p, (uint8_t)c)) on_frame(&p->f, src);
    }
}
/* a Wi-Fi pill: Cream fill, border width and color, label color */
static void pill(lv_obj_t *o, int bw, lv_color_t bc, lv_color_t tc)
{
    lv_obj_set_style_border_width(o, bw, 0); lv_obj_set_style_border_color(o, bc, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(o, 0), tc, 0);
}
static void wifi_ui(void)
{
    char b[48];
    int fresh = wifi_cls >= 0 && (port_ms() - wifi_ms) < 10000u;
    /* No Wi-Fi presence source (the radio runs Matter, not the presence program): hide the badge rather than show "no data". */
    if (!fresh) { lv_obj_add_flag(wbadge, LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_remove_flag(wbadge, LV_OBJ_FLAG_HIDDEN);
    snprintf(b, sizeof b, "Wi-Fi: %s (%d.%02d)", wifi_cls ? "someone is here" : "empty", wifi_p100 / 100, wifi_p100 % 100);
    if (wifi_cls) pill(wbadge, 2, C_RED, C_INK); else pill(wbadge, 1, C_LINE, C_STONE);
    lv_label_set_text(wbadge_lbl, b);
}
static void rx_timer_cb(lv_timer_t *t) { (void)t; drain(&rb_wire, &par_wire, "UART1 wired"); drain(&rb_mac, &par_mac, "UART0 replay"); if (wifi_dirty) { wifi_dirty = 0; wifi_ui(); } }

static void refr_ready_cb(lv_event_t *e)
{
    (void)e;
    if (pend_seq) {
        char l[64];
        last_lat_us = port_us() - pend_rx_us; if (last_lat_us > lat_max_us) lat_max_us = last_lat_us;
        snprintf(l, sizeof l, "SHOWN,%lu,%lu,%lu\r\n", (unsigned long)pend_seq, (unsigned long)port_ms(), (unsigned long)last_lat_us);
        port_uart_puts(l);
        pend_seq = 0;
    }
}


/* ---- sensor page ---- */
static const char *zone_name(void)
{
    static const char *nm[4] = {"left near", "left far", "right near", "right far"};
    int best = -1;
    for (int i = 0; i < 4; i++) if (zstate[i] && (best < 0 || zstate[i] > zstate[best])) best = i;
    return best < 0 ? "none" : nm[best];
}
static lv_obj_t *mk_strip(lv_obj_t *scr, int y, uint32_t color, int n, lv_chart_series_t **se)
{
    lv_obj_t *c = lv_chart_create(scr);
    lv_obj_set_size(c, 304, 26); lv_obj_set_pos(c, 8, y);
    lv_chart_set_type(c, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(c, (uint32_t)n);
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
    lv_chart_set_div_line_count(c, 0, 0);
    lv_obj_set_style_pad_all(c, 2, 0); lv_obj_set_style_pad_column(c, 0, 0);
    lv_obj_set_style_bg_color(c, C_CREAM, 0); lv_obj_set_style_border_width(c, 1, 0); lv_obj_set_style_border_color(c, C_LINE, 0);
    lv_obj_set_style_radius(c, 0, 0);
    *se = lv_chart_add_series(c, lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(c, *se, 0);
    return c;
}
static void sensors_update(void)
{
    char b[200];
    int fresh = wifi_cls >= 0 && (port_ms() - wifi_ms) < 10000u;
    char dist[16], snr[16];
    if (near_mm >= 0) snprintf(dist, sizeof dist, "%d.%d m", near_mm / 1000, (near_mm % 1000) / 100); else snprintf(dist, sizeof dist, "-");
    if (snr10 >= 0) snprintf(snr, sizeof snr, "%d.%d dB", snr10 / 10, snr10 % 10); else snprintf(snr, sizeof snr, "-");
    snprintf(b, sizeof b, "Held: %s\nMotion now: %s\nNearest: %s\nPoints: %d\nMax SNR: %s\nZone: %s\nFrames: %d/s",
        presence ? "yes" : "no", motion_now ? "yes" : "no", dist, rpts, snr, zone_name(), fps_radar);
    lv_label_set_text(rad_lbl, b);
    if (fresh) {
        uint32_t age10 = (uint32_t)wifi_age10 + (port_ms() - wifi_age_ms) / 100u;
        uint32_t now = port_ms(); int per_min = 0;
        for (int i = 0; i < wres_n; i++) if (now - wres_ms[i] < 60000u) per_min++;
        snprintf(b, sizeof b, "Result: %s\nProb: %d.%02d\nAge: %lu.%lu s\nResults: %d/min", wifi_cls ? "present" : "empty",
            wifi_p100 / 100, wifi_p100 % 100, (unsigned long)(age10 / 10), (unsigned long)(age10 % 10), per_min);
    } else snprintf(b, sizeof b, "Result: no data\nProb: -\nAge: -\nResults: 0/min");
    lv_label_set_text(wif_lbl, b);
}
static void goto_cb(lv_event_t *e)
{
    int to_sensors = (int)(intptr_t)lv_event_get_user_data(e);
    if (to_sensors) { sensors_update(); lv_screen_load(scr_sn); }
    else { ui_update_presence(); wifi_ui(); lv_screen_load(scr_th); }
}

static void btn_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    setpoint += d; if (setpoint < 150) setpoint = 150; if (setpoint > 300) setpoint = 300;
    ui_update_setpoint(); send_status();
}

static void stats_cb(lv_timer_t *t)
{
    static port_stats_t prev; static uint32_t b_mac, b_wire;
    char b[200], l[200]; lv_mem_monitor_t m; (void)t;
    lv_mem_monitor(&m);
    uint32_t n = port_stats.refr_cnt - prev.refr_cnt, rs = port_stats.refr_us_sum - prev.refr_us_sum;
    uint32_t busy = rs / 10000, fr = frames_total - frames_prev;
    uint32_t dm = rb_mac.total - b_mac, dw = rb_wire.total - b_wire;
    fps_radar = (int)fr;
    { int fresh = wifi_cls >= 0 && (port_ms() - wifi_ms) < 10000u; uint8_t v = !fresh ? 0 : (wifi_cls ? 2 : 1);
      lv_chart_set_next_value(ch_wifi, se_wifi, v == 2 ? 1 : (v == 1 ? 0 : LV_CHART_POINT_NONE)); }
    wifi_ui(); if (lv_screen_active() == scr_sn) sensors_update();
    prev = port_stats; frames_prev = frames_total; b_mac = rb_mac.total; b_wire = rb_wire.total;
    snprintf(b, sizeof b, "refreshes %lu/s busy %lu%%\nradar %lu frames/s, %lu B/s on %s\nlast frame to screen %lu.%lu ms (max %lu.%lu)\nheap %lu/%lu B, stack free %lu B",
        (unsigned long)n, (unsigned long)busy, (unsigned long)fr, (unsigned long)(dm + dw), src_name,
        (unsigned long)(last_lat_us / 1000), (unsigned long)((last_lat_us % 1000) / 100), (unsigned long)(lat_max_us / 1000), (unsigned long)((lat_max_us % 1000) / 100),
        (unsigned long)(m.total_size - m.free_size), (unsigned long)m.total_size, (unsigned long)port_stack_free());
    lv_label_set_text(stats_lbl, b);
    snprintf(l, sizeof l, "STAT,t=%lu,fps=%lu,busy=%lu,frames=%lu,bytes_mac=%lu,bytes_wire=%lu,drops=%lu,heap_used=%lu,heap_peak=%lu,stack_free=%lu\r\n",
        (unsigned long)(port_ms() / 1000), (unsigned long)n, (unsigned long)busy, (unsigned long)fr, (unsigned long)dm, (unsigned long)dw,
        (unsigned long)(rb_mac.drops + rb_wire.drops), (unsigned long)(m.total_size - m.free_size), (unsigned long)m.max_used, (unsigned long)port_stack_free());
    port_uart_puts(l);
    if (frames_total == 0) { send_status(); }       /* heartbeat while no radar frames arrive */
    else if (fr == 0) { send_status(); }
}

/* secondary button from the guide: Ink outline on Paper, Cream when pressed (the theme's own button look is removed) */
static void btn_style(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(o, C_PAPER, 0);
    lv_obj_set_style_bg_color(o, C_LINE, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(o, 2, 0); lv_obj_set_style_border_color(o, C_INK, 0);
    lv_obj_set_style_radius(o, 8, 0);
}
static lv_obj_t *mk_btn(lv_obj_t *scr, const char *txt, int x, int y, int d)
{
    lv_obj_t *o = lv_button_create(scr);
    btn_style(o); lv_obj_set_size(o, 130, 64); lv_obj_set_pos(o, x, y);   /* style first: removing styles also drops a set size and position */
    lv_obj_add_event_cb(o, btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)d);
    lv_obj_t *l = lv_label_create(o);
    lv_obj_set_style_text_font(l, &frl_barlow_48, 0); lv_obj_set_style_text_color(l, C_INK, 0);
    lv_label_set_text(l, txt); lv_obj_center(l);
    return o;
}

static lv_obj_t *mk_pill(lv_obj_t *scr, int x, int y, int w, int h, lv_obj_t **lbl, const lv_font_t *f)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h); lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(o, C_CREAM, 0);
    lv_obj_set_style_border_color(o, C_LINE, 0);
    lv_obj_set_style_radius(o, h / 2, 0);
    *lbl = lv_label_create(o);
    lv_obj_set_style_text_font(*lbl, f, 0); lv_obj_set_style_text_color(*lbl, C_CREAM, 0);
    lv_obj_center(*lbl);
    return o;
}
static lv_obj_t *mk_cap(lv_obj_t *scr, const char *t, int x, int y)
{
    lv_obj_t *o = lv_label_create(scr);
    lv_obj_set_style_text_color(o, C_STONE, 0); lv_label_set_text(o, t); lv_obj_set_pos(o, x, y);
    return o;
}
static void page_style(lv_obj_t *scr)
{
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(scr, C_PAPER, 0);
    lv_obj_set_style_text_color(scr, C_INK, 0); lv_obj_set_style_text_font(scr, &frl_barlow_14, 0);
    lv_obj_set_style_text_line_space(scr, 3, 0);
}
static lv_obj_t *mk_nav(lv_obj_t *scr, const char *txt, int x, int y, int w, int h, int to_sensors)
{
    lv_obj_t *o = lv_button_create(scr), *l;
    btn_style(o); lv_obj_set_size(o, w, h); lv_obj_set_pos(o, x, y);
    lv_obj_add_event_cb(o, goto_cb, LV_EVENT_CLICKED, (void *)(intptr_t)to_sensors);
    l = lv_label_create(o); lv_obj_set_style_text_font(l, &frl_barlow_14, 0); lv_obj_set_style_text_color(l, C_INK, 0);
    lv_label_set_text(l, txt); lv_obj_center(l);
    return o;
}

void app_create(void)
{
    lv_obj_t *scr = lv_screen_active(), *o;
    scr_th = scr;
    char b[24];
    page_style(scr);
    tlv_init(&par_mac); tlv_init(&par_wire);
    NVIC_EnableIRQ(DBG_UART_INST_INT_IRQN); NVIC_EnableIRQ(LINK_UART_INST_INT_IRQN);

    /* header: logo (Fox Red on Paper, 120 px wide as the guide's minimum), page title, Sensors button */
    o = lv_image_create(scr); lv_image_set_src(o, &frl_logo); lv_obj_set_pos(o, 10, 2);
    lv_obj_set_style_image_recolor(o, C_RED, 0); lv_obj_set_style_image_recolor_opa(o, LV_OPA_COVER, 0);
    o = lv_label_create(scr); lv_obj_set_style_text_font(o, &frl_barlow_20, 0);
    lv_label_set_text(o, "Thermostat"); lv_obj_set_pos(o, 134, 20);
    mk_nav(scr, "Sensors", 246, 9, 66, 44, 1);

    badge = mk_pill(scr, 12, 64, 296, 48, &badge_lbl, &frl_barlow_20);
    lv_obj_set_style_border_width(badge, 0, 0);
    frame_lbl = mk_cap(scr, "", 14, 120);
    chip = mk_pill(scr, 188, 117, 120, 22, &chip_lbl, &frl_barlow_14);

    wbadge = mk_pill(scr, 12, 144, 296, 32, &wbadge_lbl, &frl_barlow_20);

    mk_cap(scr, "Temperature (fixed value, no sensor read)", 14, 184);
    temp_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(temp_lbl, &frl_barlow_48, 0); lv_obj_set_style_text_color(temp_lbl, C_STONE, 0);
    fmt_t(b, sizeof b, temp_now); lv_label_set_text(temp_lbl, b); lv_obj_set_pos(temp_lbl, 14, 198);

    mk_cap(scr, "Setpoint", 14, 248);
    set_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(set_lbl, &frl_barlow_64, 0);
    lv_obj_set_pos(set_lbl, 14, 270);
    heat_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(heat_lbl, &frl_barlow_20, 0);
    lv_obj_set_pos(heat_lbl, 236, 246);

    mk_btn(scr, "-", 20, 338, -5);
    mk_btn(scr, "+", 170, 338, 5);

    radio_lbl = mk_cap(scr, "", 14, 404);
    stats_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(stats_lbl, C_STONE, 0);
    lv_obj_set_pos(stats_lbl, 14, 410); lv_label_set_text(stats_lbl, "measuring...");

    /* second page: what each sensor says right now */
    scr_sn = lv_obj_create(NULL); page_style(scr_sn);
    o = lv_label_create(scr_sn); lv_obj_set_style_text_font(o, &frl_barlow_20, 0);
    lv_label_set_text(o, "Sensors"); lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 10);
    mk_nav(scr_sn, "Back", 6, 4, 80, 40, 0);
    o = lv_label_create(scr_sn); lv_obj_set_style_text_font(o, &frl_barlow_20, 0); lv_obj_set_style_text_color(o, C_RED, 0);
    lv_label_set_text(o, "Radar"); lv_obj_set_pos(o, 8, 52);
    o = lv_label_create(scr_sn); lv_obj_set_style_text_font(o, &frl_barlow_20, 0); lv_obj_set_style_text_color(o, C_INK, 0);
    lv_label_set_text(o, "Wi-Fi"); lv_obj_set_pos(o, 168, 52);
    rad_lbl = lv_label_create(scr_sn); lv_obj_set_pos(rad_lbl, 8, 84); lv_obj_set_width(rad_lbl, 152);
    wif_lbl = lv_label_create(scr_sn); lv_obj_set_pos(wif_lbl, 168, 84); lv_obj_set_width(wif_lbl, 148);
    mk_cap(scr_sn, "Radar, last 30 s: held (red), motion (black)", 8, 270);
    ch_held = mk_strip(scr_sn, 290, 0xB42418, HIST_FR, &se_held);
    ch_motion = mk_strip(scr_sn, 320, 0x161312, HIST_FR, &se_motion);
    mk_cap(scr_sn, "Wi-Fi, last 30 s: present (gray), one per second", 8, 356);
    ch_wifi = mk_strip(scr_sn, 376, 0x6B625B, HIST_W, &se_wifi);
    ui_update_setpoint(); ui_update_presence(); wifi_ui();
    lv_display_add_event_cb(lv_display_get_default(), refr_ready_cb, LV_EVENT_REFR_READY, NULL);
    lv_timer_create(rx_timer_cb, 2, NULL);
    lv_timer_create(stats_cb, 1000, NULL);
}
