/* Demo screen: title, press counter, slider that sets the red LED's brightness, switch for the
 * LED, an arc and a line chart updated every 100 ms, and a live stats block. */
#include "ti_msp_dl_config.h"
#include "lvgl.h"
#include "app.h"
#include "port.h"
#include "board.h"

/* Title and counter use a bigger font only when the build has one (the oversize build; see README). */
#if LV_FONT_MONTSERRAT_20
#define BIG_FONT &lv_font_montserrat_20
#else
#define BIG_FONT &lv_font_montserrat_14
#endif

static lv_obj_t *count_lbl, *level_lbl, *led_lbl, *arc, *arc_lbl, *chart, *stats_lbl;
static lv_chart_series_t *ser;
static lv_obj_t *sw, *slider;
static int presses;

static void led_apply(void)
{
    int on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    port_led(on, lv_slider_get_value(slider) / 10);
    lv_label_set_text(led_lbl, on ? "LED on" : "LED off");
}
static void press_cb(lv_event_t *e)
{
    (void)e;
    lv_label_set_text_fmt(count_lbl, "Count: %d", ++presses);
}
static void slider_cb(lv_event_t *e)
{
    (void)e;
    lv_label_set_text_fmt(level_lbl, "Slider: %d %%  (LED level %d of 10)",
                          (int)lv_slider_get_value(slider), (int)lv_slider_get_value(slider) / 10);
    led_apply();
}
static void switch_cb(lv_event_t *e) { (void)e; led_apply(); }
static void redraw_cb(lv_event_t *e)
{
    (void)e;
    port_stats.full_pending = 1;
    lv_obj_invalidate(lv_screen_active());
}

/* a made-up sensor: triangle wave plus a little noise, 0..100 */
static void sensor_timer_cb(lv_timer_t *t)
{
    static uint32_t phase, rnd = 12345;
    (void)t;
    rnd = rnd * 1103515245u + 12345u;
    phase = (phase + 1) & 63;
    int tri = phase < 32 ? (int)phase * 3 : (63 - (int)phase) * 3;     /* 0..93 */
    int v = 3 + tri + (int)((rnd >> 16) % 5);
    lv_chart_set_next_value(chart, ser, v);
    lv_arc_set_value(arc, v);
    lv_label_set_text_fmt(arc_lbl, "%d", v);
}

/* once a second: refresh the stats block and print the same numbers to UART0 */
static void stats_timer_cb(lv_timer_t *t)
{
    static port_stats_t prev;
    static char line[256];
    lv_mem_monitor_t m;
    (void)t;
    lv_mem_monitor(&m);
    uint32_t n = port_stats.refr_cnt - prev.refr_cnt;
    uint32_t refr_sum = port_stats.refr_us_sum - prev.refr_us_sum;
    uint32_t flush_sum = port_stats.flush_us_sum - prev.flush_us_sum;
    uint32_t px = port_stats.flush_px - prev.flush_px;
    uint32_t busy_pct = refr_sum / 10000;                    /* refresh time per second, in percent */
    uint32_t avg_ms10 = n ? (refr_sum / n) / 100 : 0;        /* average refresh, tenths of ms */
    uint32_t fl_ms10 = n ? (flush_sum / n) / 100 : 0;
    uint32_t used = m.total_size - m.free_size;
    uint32_t full10 = port_stats.full_redraw_us / 100;
    uint32_t stk = port_stack_free();
    prev = port_stats;

    lv_label_set_text_fmt(stats_lbl,
        "FPS: %u refreshes/s, busy %u%%\n"
        "Refresh %u.%u ms (flush %u.%u), %u px/s\n"
        "Full redraw: %u.%u ms\n"
        "Heap: %u/%u B, peak %u, frag %u%%\n"
        "Stack free %u B, touch errors %u",
        (unsigned)n, (unsigned)busy_pct,
        (unsigned)(avg_ms10 / 10), (unsigned)(avg_ms10 % 10), (unsigned)(fl_ms10 / 10), (unsigned)(fl_ms10 % 10),
        (unsigned)px,
        (unsigned)(full10 / 10), (unsigned)(full10 % 10),
        (unsigned)used, (unsigned)m.total_size, (unsigned)m.max_used, (unsigned)m.frag_pct,
        (unsigned)stk, (unsigned)port_stats.touch_errors);

    lv_snprintf(line, sizeof line,
        "t=%us fps=%u busy=%u%% refr_ms=%u.%u flush_ms=%u.%u px=%u full_ms=%u.%u heap_used=%u heap_total=%u heap_peak=%u heap_free=%u frag=%u%% stack_free=%u touch_err=%u presses=%d\r\n",
        (unsigned)(port_ms() / 1000), (unsigned)n, (unsigned)busy_pct,
        (unsigned)(avg_ms10 / 10), (unsigned)(avg_ms10 % 10), (unsigned)(fl_ms10 / 10), (unsigned)(fl_ms10 % 10),
        (unsigned)px, (unsigned)(full10 / 10), (unsigned)(full10 % 10),
        (unsigned)used, (unsigned)m.total_size, (unsigned)m.max_used, (unsigned)m.free_size, (unsigned)m.frag_pct,
        (unsigned)stk, (unsigned)port_stats.touch_errors, presses);
    port_uart_puts(line);
}

void app_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *o;
    lv_obj_set_scrollable(scr, false);   /* the screen must not scroll under a finger */

    o = lv_label_create(scr);
    lv_obj_set_style_text_font(o, BIG_FONT, 0);
    lv_label_set_text(o, "LVGL on " BOARD_NAME);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 8);

    o = lv_label_create(scr);
    lv_obj_set_style_text_color(o, lv_color_hex(0x9AA0A6), 0);
    lv_label_set_text_fmt(o, "Cortex-M0+ %d MHz, 32 KB RAM, LVGL %d.%d.%d",
                          (int)(CPUCLK_FREQ / 1000000), LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 42);

    /* press counter */
    o = lv_button_create(scr);
    lv_obj_set_size(o, 130, 44);
    lv_obj_set_pos(o, 16, 72);
    lv_obj_add_event_cb(o, press_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(o);
    lv_label_set_text(l, "Press me");
    lv_obj_center(l);
    count_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(count_lbl, BIG_FONT, 0);
    lv_label_set_text(count_lbl, "Count: 0");
    lv_obj_set_pos(count_lbl, 164, 82);

    /* slider drives a label and the LED brightness */
    level_lbl = lv_label_create(scr);
    lv_obj_set_pos(level_lbl, 16, 132);
    slider = lv_slider_create(scr);
    lv_obj_set_size(slider, 270, 14);
    lv_obj_set_pos(slider, 25, 164);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* switch: LED on or off; button: time a full-screen redraw */
    sw = lv_switch_create(scr);
    lv_obj_set_pos(sw, 16, 198);
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    led_lbl = lv_label_create(scr);
    lv_obj_set_pos(led_lbl, 80, 208);
    o = lv_button_create(scr);
    lv_obj_set_size(o, 130, 40);
    lv_obj_set_pos(o, 174, 196);
    lv_obj_add_event_cb(o, redraw_cb, LV_EVENT_CLICKED, NULL);
    l = lv_label_create(o);
    lv_label_set_text(l, "Full redraw");
    lv_obj_center(l);

    /* arc and chart, both fed every 100 ms */
    arc = lv_arc_create(scr);
    lv_obj_set_size(arc, 110, 110);
    lv_obj_set_pos(arc, 8, 252);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(arc, false);
    arc_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(arc_lbl, BIG_FONT, 0);
    lv_label_set_text(arc_lbl, "0");
    lv_obj_align_to(arc_lbl, arc, LV_ALIGN_CENTER, 0, 0);

    chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 180, 110);
    lv_obj_set_pos(chart, 128, 252);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 30);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(chart, 3, 0);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    ser = lv_chart_add_series(chart, lv_color_hex(0x4FC3F7), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, ser, 0);

    /* stats */
    stats_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(stats_lbl, lv_color_hex(0xA5D6A7), 0);
    lv_obj_set_pos(stats_lbl, 12, 376);
    lv_label_set_text(stats_lbl, "measuring...");

    slider_cb(NULL);
    lv_timer_create(sensor_timer_cb, 100, NULL);
    lv_timer_create(stats_timer_cb, 1000, NULL);
}
