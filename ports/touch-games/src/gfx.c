#include "gfx.h"

uint16_t gfx_buf[GFX_BUF_PX];
uint32_t gfx_px_sent;

void gfx_blit(int x, int y, int w, int h, const uint16_t *px)
{
    gfx_px_sent += (uint32_t)w * h;
    hal_lcd_blit(x, y, w, h, px);
}

void gfx_fill(int x, int y, int w, int h, uint16_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_W) w = LCD_W - x;
    if (y + h > LCD_H) h = LCD_H - y;
    if (w > 0 && h > 0) { gfx_px_sent += (uint32_t)w * h; hal_lcd_fill(x, y, w, h, c); }
}

void gfx_frame(int x, int y, int w, int h, int t, uint16_t c)
{
    gfx_fill(x, y, w, t, c); gfx_fill(x, y + h - t, w, t, c);
    gfx_fill(x, y + t, t, h - 2 * t, c); gfx_fill(x + w - t, y + t, t, h - 2 * t, c);
}

int gfx_text_w(const char *s, int scale)
{
    int n = 0; while (s[n]) n++;
    return n ? n * 6 * scale - scale : 0;
}

void gfx_buf_glyph(uint16_t *buf, int bw, int bh, int ox, int oy, char c, int scale, uint16_t fg)
{
    const uint8_t *g = font_glyph(c);
    for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++) {
            if (!(g[r] & (0x10 >> k))) continue;
            for (int sy = 0; sy < scale; sy++) {
                int py = oy + r * scale + sy; if (py < 0 || py >= bh) continue;
                for (int sx = 0; sx < scale; sx++) {
                    int px = ox + k * scale + sx; if (px < 0 || px >= bw) continue;
                    buf[py * bw + px] = fg;
                }
            }
        }
}

/* Text is built in the scratch buffer one chunk of characters at a time and sent as one block. */
void gfx_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    int h = 7 * scale, adv = 6 * scale;
    int per = GFX_BUF_PX / (adv * h); if (per < 1) per = 1;
    while (*s) {
        int n = 0; while (s[n] && n < per) n++;
        int w = n * adv - (s[n] ? 0 : scale);          /* no gap after the last character of the string */
        for (int i = 0; i < w * h; i++) gfx_buf[i] = bg;
        for (int i = 0; i < n; i++) gfx_buf_glyph(gfx_buf, w, h, i * adv, 0, s[i], scale, fg);
        if (x >= 0 && y >= 0 && x + w <= LCD_W && y + h <= LCD_H) gfx_blit(x, y, w, h, gfx_buf);
        x += n * adv; s += n;
    }
}

void gfx_text_center(int x, int y, int w, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    int tw = gfx_text_w(s, scale), th = 7 * scale;
    int lx = x + (w - tw) / 2;
    if (lx > x) gfx_fill(x, y, lx - x, th, bg);
    gfx_text(lx, y, s, scale, fg, bg);
    int rx = lx + tw;
    if (rx < x + w) gfx_fill(rx, y, x + w - rx, th, bg);
}

char *gfx_utoa(uint32_t v, char *out)
{
    char t[11]; int n = 0, i = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) out[i++] = t[--n];
    out[i] = 0;
    return out;
}
