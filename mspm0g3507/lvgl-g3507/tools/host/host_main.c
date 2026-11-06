/* Runs the real src/app.c and lv_conf.h on the Mac with a fake 320x480 screen, to check the layout
 * and to measure LVGL heap use. Heap numbers are from a 64-bit build, so they are an upper bound for
 * the 32-bit chip (pointers and alignment are twice as big here). No port.c, no hardware. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lvgl.h"
#include "port.h"
#include "app.h"

port_stats_t port_stats;
static uint32_t fake_ms;
uint32_t port_ms(void) { return fake_ms; }
uint32_t port_us(void) { return fake_ms * 1000u; }
void port_led(int on, int level) { printf("led on=%d level=%d\n", on, level); }
void port_uart_puts(const char *s) { fputs(s, stdout); }
uint32_t port_stack_free(void) { return 0; }

static uint16_t fb[LCD_W * LCD_H];
static uint8_t draw_buf[LCD_W * DRAW_ROWS * 2] __attribute__((aligned(4)));
static uint32_t flushes, max_area_px;
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(&fb[y * LCD_W + a->x1], px + (y - a->y1) * w * 2, w * 2);
    flushes++;
    uint32_t n = (uint32_t)w * (a->y2 - a->y1 + 1); if (n > max_area_px) max_area_px = n;
    lv_display_flush_ready(d);
}
static void dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", LCD_W, LCD_H);
    for (int i = 0; i < LCD_W * LCD_H; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void report(const char *tag)
{
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    printf("[%s] heap total %u used %u free %u max_used %u biggest_free %u frag %u%%\n", tag,
           (unsigned)m.total_size, (unsigned)(m.total_size - m.free_size), (unsigned)m.free_size,
           (unsigned)m.max_used, (unsigned)m.free_biggest_size, (unsigned)m.frag_pct);
}
int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    lv_init();
    lv_tick_set_cb(port_ms);
    lv_display_t *d = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);
    report("after init");
    app_create();
    report("after app_create");
    for (int t = 0; t < 4000; t += 10) { fake_ms = t; lv_timer_handler(); }
    report("after 4 s");
    printf("flushes %u, largest flush %u px\n", flushes, max_area_px);
    dump(argc > 1 ? argv[1] : "host.ppm");
    return 0;
}
