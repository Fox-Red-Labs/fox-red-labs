/* Host build of the hal: a framebuffer in memory, scripted touch, stdout log. Used by the tests and the pictures. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "../src/hal.h"

uint16_t host_fb[LCD_W * LCD_H];
long host_px_sent;
int host_tx, host_ty, host_tdown;
static uint32_t fake_us;

void hal_init(void) { memset(host_fb, 0xFF, sizeof host_fb); }
void hal_lcd_fill(int x, int y, int w, int h, uint16_t c)
{
    for (int r = 0; r < h; r++) for (int k = 0; k < w; k++) host_fb[(y + r) * LCD_W + x + k] = c;
    host_px_sent += (long)w * h; fake_us += (uint32_t)(w * h) / 20;
}
void hal_lcd_blit(int x, int y, int w, int h, const uint16_t *px)
{
    for (int r = 0; r < h; r++) memcpy(&host_fb[(y + r) * LCD_W + x], px + r * w, (size_t)w * 2);
    host_px_sent += (long)w * h; fake_us += (uint32_t)(w * h) / 20;
}
int hal_touch(int *x, int *y) { *x = host_tx; *y = host_ty; return host_tdown; }
uint32_t hal_ms(void) { return fake_us / 1000; }
uint32_t hal_us(void) { return fake_us; }
void hal_log(const char *s) { fputs(s, stdout); }
uint32_t hal_stack_used(void) { return 0; }
void hal_idle(void) { fake_us += 1000; }

void host_save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", LCD_W, LCD_H);
    for (int i = 0; i < LCD_W * LCD_H; i++) {
        uint16_t p = host_fb[i]; unsigned char c[3] = { (unsigned char)((p >> 8) & 0xF8), (unsigned char)((p >> 3) & 0xFC), (unsigned char)((p << 3) & 0xF8) };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
}
