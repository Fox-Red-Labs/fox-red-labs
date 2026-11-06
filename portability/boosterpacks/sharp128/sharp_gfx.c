/* 1-bit drawing for the Sharp panel. See sharp_gfx.h. */
#include "sharp_gfx.h"
#include "sharp_font5x7.h"

void sg_surface_init(sg_surface_t *s, uint8_t *buf, int16_t y0, int16_t h)
{
    s->buf = buf; s->y0 = y0; s->h = h;
}

void sg_fill(sg_surface_t *s, uint8_t color)
{
    uint16_t n = (uint16_t)s->h * SHARP_ROW_BYTES, i;
    uint8_t v = color ? 0xFF : 0x00;
    for (i = 0; i < n; i++) s->buf[i] = v;
}

void sg_pixel(sg_surface_t *s, int16_t x, int16_t y, uint8_t color)
{
    uint8_t *p;
    int16_t r;
    if (x < 0 || x >= SHARP_W) return;
    r = (int16_t)(y - s->y0);
    if (r < 0 || r >= s->h || y >= SHARP_H) return;
    p = &s->buf[(uint16_t)r * SHARP_ROW_BYTES + (x >> 3)];
    if (color) *p |= (uint8_t)(1u << (x & 7));
    else       *p &= (uint8_t)~(1u << (x & 7));
}

int sg_get(const sg_surface_t *s, int16_t x, int16_t y)
{
    int16_t r = (int16_t)(y - s->y0);
    if (x < 0 || x >= SHARP_W || r < 0 || r >= s->h || y >= SHARP_H) return -1;
    return (s->buf[(uint16_t)r * SHARP_ROW_BYTES + (x >> 3)] >> (x & 7)) & 1;
}

void sg_hline(sg_surface_t *s, int16_t x, int16_t y, int16_t len, uint8_t color)
{
    int16_t r = (int16_t)(y - s->y0), x1, xb, xe;
    uint8_t *row;
    if (len <= 0 || r < 0 || r >= s->h || y >= SHARP_H) return;
    x1 = (int16_t)(x + len);              /* one past the end */
    if (x < 0) x = 0;
    if (x1 > SHARP_W) x1 = SHARP_W;
    if (x >= x1) return;
    row = &s->buf[(uint16_t)r * SHARP_ROW_BYTES];
    xb = x; xe = x1;
    while (xb < xe && (xb & 7)) { sg_pixel(s, xb, y, color); xb++; }
    while (xb + 8 <= xe) { row[xb >> 3] = color ? 0xFF : 0x00; xb += 8; }
    while (xb < xe) { sg_pixel(s, xb, y, color); xb++; }
}

void sg_vline(sg_surface_t *s, int16_t x, int16_t y, int16_t len, uint8_t color)
{
    int16_t i;
    for (i = 0; i < len; i++) sg_pixel(s, x, (int16_t)(y + i), color);
}

void sg_line(sg_surface_t *s, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    int16_t dx = (int16_t)(x1 > x0 ? x1 - x0 : x0 - x1);
    int16_t dy = (int16_t)(y1 > y0 ? y0 - y1 : y1 - y0);   /* negative */
    int16_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int16_t err = (int16_t)(dx + dy), e2;
    for (;;) {
        sg_pixel(s, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = (int16_t)(2 * err);
        if (e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

void sg_rect(sg_surface_t *s, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    if (w <= 0 || h <= 0) return;
    sg_hline(s, x, y, w, color);
    sg_hline(s, x, (int16_t)(y + h - 1), w, color);
    sg_vline(s, x, y, h, color);
    sg_vline(s, (int16_t)(x + w - 1), y, h, color);
}

void sg_fill_rect(sg_surface_t *s, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    int16_t i, ya = y, yb = (int16_t)(y + h);
    if (ya < s->y0) ya = s->y0;                         /* only the rows this band holds */
    if (yb > (int16_t)(s->y0 + s->h)) yb = (int16_t)(s->y0 + s->h);
    for (i = ya; i < yb; i++) sg_hline(s, x, i, w, color);
}

void sg_circle(sg_surface_t *s, int16_t cx, int16_t cy, int16_t r, uint8_t color)
{
    int16_t x = r, y = 0, err = (int16_t)(1 - r);
    while (x >= y) {
        sg_pixel(s, (int16_t)(cx + x), (int16_t)(cy + y), color);
        sg_pixel(s, (int16_t)(cx + y), (int16_t)(cy + x), color);
        sg_pixel(s, (int16_t)(cx - y), (int16_t)(cy + x), color);
        sg_pixel(s, (int16_t)(cx - x), (int16_t)(cy + y), color);
        sg_pixel(s, (int16_t)(cx - x), (int16_t)(cy - y), color);
        sg_pixel(s, (int16_t)(cx - y), (int16_t)(cy - x), color);
        sg_pixel(s, (int16_t)(cx + y), (int16_t)(cy - x), color);
        sg_pixel(s, (int16_t)(cx + x), (int16_t)(cy - y), color);
        y++;
        if (err < 0) err = (int16_t)(err + 2 * y + 1);
        else { x--; err = (int16_t)(err + 2 * (y - x) + 1); }
    }
}

static void glyph(sg_surface_t *s, int16_t x, int16_t y, char c, uint8_t color, uint8_t k)
{
    const uint8_t *g;
    uint8_t col, row, bits;
    if ((uint8_t)c < SHARP_FONT_FIRST || (uint8_t)c > SHARP_FONT_LAST) c = '?';
    /* Skip glyphs wholly outside this band: this is what keeps a 1-row band cheap. */
    if (y >= (int16_t)(s->y0 + s->h) || (int16_t)(y + 7 * k) < s->y0) return;
    g = sharp_font5x7[(uint8_t)c - SHARP_FONT_FIRST];
    for (col = 0; col < 5; col++) {
        bits = g[col];
        for (row = 0; row < 7; row++)
            if (bits & (1u << row))
                sg_fill_rect(s, (int16_t)(x + col * k), (int16_t)(y + row * k), k, k, color);
    }
}

int16_t sg_text(sg_surface_t *s, int16_t x, int16_t y, const char *str, uint8_t color, uint8_t scale)
{
    int16_t cx = x;
    if (scale == 0) scale = 1;
    for (; *str; str++) {
        if (*str == '\n') { cx = x; y = (int16_t)(y + SG_CELL_H * scale); continue; }
        glyph(s, cx, y, *str, color, scale);
        cx = (int16_t)(cx + SG_CELL_W * scale);
    }
    return cx;
}

void sg_blit(sg_surface_t *s, int16_t x, int16_t y, const uint8_t *bmp, int16_t w, int16_t h,
             uint8_t color, uint8_t opaque)
{
    int16_t stride = (int16_t)((w + 7) >> 3), i, j;
    for (j = 0; j < h; j++) {
        int16_t yy = (int16_t)(y + j);
        if (yy < s->y0 || yy >= (int16_t)(s->y0 + s->h) || yy >= SHARP_H) continue;
        for (i = 0; i < w; i++) {
            if ((bmp[j * stride + (i >> 3)] >> (i & 7)) & 1) sg_pixel(s, (int16_t)(x + i), yy, color);
            else if (opaque) sg_pixel(s, (int16_t)(x + i), yy, (uint8_t)!color);
        }
    }
}

int sg_utoa(uint32_t v, char *buf)
{
    char t[10];
    int n = 0, i;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (i = 0; i < n; i++) buf[i] = t[n - 1 - i];
    buf[n] = 0;
    return n;
}

void sg_render_bands(sg_draw_fn fn, void *ctx, uint8_t *band_buf, uint8_t band_rows)
{
    sg_surface_t s;
    int16_t y;
    for (y = 0; y < SHARP_H; y = (int16_t)(y + band_rows)) {
        int16_t h = (int16_t)(SHARP_H - y < band_rows ? SHARP_H - y : band_rows);
        sg_surface_init(&s, band_buf, y, h);
        sg_fill(&s, SG_WHITE);
        fn(&s, ctx);
        sharp_update_lines(band_buf, (uint8_t)y, (uint8_t)h);
    }
}
