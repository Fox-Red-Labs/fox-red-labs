/*
 * ST7735S driver for the Educational BoosterPack MKII's 128 x 128 panel
 * (Crystalfontz CFAF128128B-0145T), written for this project from the ST7735S
 * command set. Register values are the ones this repository's bench run used on the
 * same panel. Zephyr's display_st7735r.c (Apache-2.0) was read for command names only.
 * BSD-3-Clause.
 */
#include "edumkii.h"

#ifndef EDUMKII_NO_LCD

#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_NORON   0x13
#define CMD_INVOFF  0x20
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A


static void select_lcd(edumkii_t *d)   { d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_CS, 0); }
static void deselect_lcd(edumkii_t *d) { d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_CS, 1); }

static void send(edumkii_t *d, const uint8_t *p, uint32_t n)
{
    if (n >= 64u && d->port.spi_write_bulk) d->port.spi_write_bulk(d->port.ctx, p, n);
    else d->port.spi_write(d->port.ctx, p, n);
}

/* One command with its parameters. CS must already be low. */
static void cmd(edumkii_t *d, uint8_t c, const uint8_t *args, uint32_t n)
{
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_DC, 0);
    d->port.spi_write(d->port.ctx, &c, 1);
    if (n) {
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_DC, 1);
        d->port.spi_write(d->port.ctx, args, n);
    }
}

static void cmd1(edumkii_t *d, uint8_t c, uint8_t a) { cmd(d, c, &a, 1); }

static void set_window(edumkii_t *d, int x0, int y0, int x1, int y1)
{
    uint16_t xa = (uint16_t)(x0 + EDUMKII_COL_OFFSET), xb = (uint16_t)(x1 + EDUMKII_COL_OFFSET);
    uint16_t ya = (uint16_t)(y0 + EDUMKII_ROW_OFFSET), yb = (uint16_t)(y1 + EDUMKII_ROW_OFFSET);
    uint8_t c[4] = { (uint8_t)(xa >> 8), (uint8_t)xa, (uint8_t)(xb >> 8), (uint8_t)xb };
    uint8_t r[4] = { (uint8_t)(ya >> 8), (uint8_t)ya, (uint8_t)(yb >> 8), (uint8_t)yb };
    cmd(d, CMD_CASET, c, 4);
    cmd(d, CMD_RASET, r, 4);
    cmd(d, CMD_RAMWR, 0, 0);
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_DC, 1);
}

int edumkii_lcd_init(edumkii_t *d)
{
    static const uint8_t frm[6] = { 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D };
    static const uint8_t pw1[3] = { 0xA2, 0x02, 0x84 };
    static const uint8_t pw3[2] = { 0x0A, 0x00 };
    static const uint8_t gp[16] = { 0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10 };
    static const uint8_t gn[16] = { 0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10 };
    if (!d->port.gpio_write || !d->port.spi_write) return EDUMKII_ERR_PORT;
    select_lcd(d);
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_RST, 0);
    d->port.delay_ms(d->port.ctx, 10);
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_RST, 1);
    d->port.delay_ms(d->port.ctx, 120);
    cmd(d, CMD_SWRESET, 0, 0);
    d->port.delay_ms(d->port.ctx, 150);
    cmd(d, CMD_SLPOUT, 0, 0);
    d->port.delay_ms(d->port.ctx, 120);
    cmd(d, 0xB1, frm, 3);          /* frame rate, normal mode */
    cmd(d, 0xB2, frm, 3);          /* idle mode */
    cmd(d, 0xB3, frm, 6);          /* partial mode */
    cmd1(d, 0xB4, 0x07);           /* inversion control */
    cmd(d, 0xC0, pw1, 3);          /* power control 1 */
    cmd1(d, 0xC1, 0xC5);           /* power control 2 */
    cmd(d, 0xC2, pw3, 2);          /* power control 3 */
    cmd1(d, 0xC5, 0x0E);           /* VCOM */
    cmd(d, CMD_INVOFF, 0, 0);
    cmd1(d, CMD_MADCTL, EDUMKII_MADCTL);
    cmd1(d, CMD_COLMOD, 0x05);     /* 16 bits per pixel */
    cmd(d, 0xE0, gp, 16);
    cmd(d, 0xE1, gn, 16);
    cmd(d, CMD_NORON, 0, 0);
    d->port.delay_ms(d->port.ctx, 10);
    cmd(d, CMD_DISPON, 0, 0);
    d->port.delay_ms(d->port.ctx, 100);
    deselect_lcd(d);
    edumkii_fill(d, EDUMKII_BLACK);
    return EDUMKII_OK;
}

void edumkii_backlight(edumkii_t *d, int on)
{
    if (d->port.gpio_write) d->port.gpio_write(d->port.ctx, EDUMKII_PIN_BL_RED, on ? 1 : 0);
}

static void stream_color(edumkii_t *d, uint32_t npix, uint16_t color)
{
    uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)color;
    uint32_t chunk = EDUMKII_LINE_BYTES / 2u;
    if (chunk > 128u) chunk = 128u;                 /* one panel row */
    for (uint32_t i = 0; i < chunk; i++) { d->line[2 * i] = hi; d->line[2 * i + 1] = lo; }
    while (npix) {
        uint32_t n = npix < chunk ? npix : chunk;
        send(d, d->line, n * 2u);
        npix -= n;
    }
}

void edumkii_rect(edumkii_t *d, int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > EDUMKII_LCD_W) w = EDUMKII_LCD_W - x;
    if (y + h > EDUMKII_LCD_H) h = EDUMKII_LCD_H - y;
    if (w <= 0 || h <= 0) return;
    select_lcd(d);
    set_window(d, x, y, x + w - 1, y + h - 1);
    stream_color(d, (uint32_t)w * (uint32_t)h, color);
    deselect_lcd(d);
}

void edumkii_fill(edumkii_t *d, uint16_t color)
{
    edumkii_rect(d, 0, 0, EDUMKII_LCD_W, EDUMKII_LCD_H, color);
}

void edumkii_frame(edumkii_t *d, int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    edumkii_rect(d, x, y, w, 1, color);
    edumkii_rect(d, x, y + h - 1, w, 1, color);
    edumkii_rect(d, x, y, 1, h, color);
    edumkii_rect(d, x + w - 1, y, 1, h, color);
}

void edumkii_pixel(edumkii_t *d, int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= EDUMKII_LCD_W || y >= EDUMKII_LCD_H) return;
    uint8_t b[2] = { (uint8_t)(color >> 8), (uint8_t)color };
    select_lcd(d);
    set_window(d, x, y, x, y);
    d->port.spi_write(d->port.ctx, b, 2);
    deselect_lcd(d);
}

void edumkii_line(edumkii_t *d, int x0, int y0, int x1, int y1, uint16_t color)
{
    if (y0 == y1) { int a = x0 < x1 ? x0 : x1; edumkii_rect(d, a, y0, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, color); return; }
    if (x0 == x1) { int a = y0 < y1 ? y0 : y1; edumkii_rect(d, x0, a, 1, (y0 < y1 ? y1 - y0 : y0 - y1) + 1, color); return; }
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;   /* dy is negative */
    int err = dx + dy;
    for (;;) {
        edumkii_pixel(d, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void edumkii_blit_raw(edumkii_t *d, int x, int y, int w, int h, const uint8_t *be)
{
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > EDUMKII_LCD_W || y + h > EDUMKII_LCD_H) return;
    select_lcd(d);
    set_window(d, x, y, x + w - 1, y + h - 1);
    send(d, be, (uint32_t)w * (uint32_t)h * 2u);
    deselect_lcd(d);
}

void edumkii_blit(edumkii_t *d, int x, int y, int w, int h, const uint16_t *px)
{
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > EDUMKII_LCD_W || y + h > EDUMKII_LCD_H) return;
    uint32_t total = (uint32_t)w * (uint32_t)h, chunk = EDUMKII_LINE_BYTES / 2u, i = 0;
    select_lcd(d);
    set_window(d, x, y, x + w - 1, y + h - 1);
    while (i < total) {
        uint32_t n = total - i < chunk ? total - i : chunk;
        for (uint32_t k = 0; k < n; k++) {
            d->line[2 * k] = (uint8_t)(px[i + k] >> 8);
            d->line[2 * k + 1] = (uint8_t)px[i + k];
        }
        send(d, d->line, n * 2u);
        i += n;
    }
    deselect_lcd(d);
}

void edumkii_stream_begin(edumkii_t *d, int x, int y, int w, int h)
{
    select_lcd(d);
    set_window(d, x, y, x + w - 1, y + h - 1);
}

void edumkii_stream_write(edumkii_t *d, const uint8_t *be, uint32_t len)
{
    if (d->port.spi_write_async) d->port.spi_write_async(d->port.ctx, be, len);
    else send(d, be, len);
}

void edumkii_stream_end(edumkii_t *d)
{
    if (d->port.spi_wait) d->port.spi_wait(d->port.ctx);
    deselect_lcd(d);
}

int edumkii_char(edumkii_t *d, int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    if (scale < 1) scale = 1;
    if (scale > 2) scale = 2;
    int cw = 6 * scale, ch = 8 * scale;
    if (x < 0 || y < 0 || x + cw > EDUMKII_LCD_W || y + ch > EDUMKII_LCD_H) return x + cw;
    const uint8_t *g = edumkii_font5x7(c);
    uint8_t fh = (uint8_t)(fg >> 8), fl = (uint8_t)fg, bh = (uint8_t)(bg >> 8), bl = (uint8_t)bg;
    uint8_t *o = d->line;
    for (int row = 0; row < ch; row++) {
        int gy = row / scale;
        for (int col = 0; col < cw; col++) {
            int gx = col / scale;
            int on = (gx < 5 && gy < 7) ? ((g[gx] >> gy) & 1) : 0;   /* column 5 and row 7 are spacing */
            *o++ = on ? fh : bh;
            *o++ = on ? fl : bl;
        }
    }
    select_lcd(d);
    set_window(d, x, y, x + cw - 1, y + ch - 1);
    send(d, d->line, (uint32_t)cw * (uint32_t)ch * 2u);
    deselect_lcd(d);
    return x + cw;
}

int edumkii_text(edumkii_t *d, int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    while (*s) x = edumkii_char(d, x, y, *s++, fg, bg, scale);
    return x;
}

int edumkii_number(edumkii_t *d, int x, int y, int32_t v, int min_width, uint16_t fg, uint16_t bg, int scale)
{
    char buf[12];
    int n = 0;
    uint32_t u = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    do { buf[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) buf[n++] = '-';
    for (int i = n; i < min_width; i++) x = edumkii_char(d, x, y, ' ', fg, bg, scale);
    while (n) x = edumkii_char(d, x, y, buf[--n], fg, bg, scale);
    return x;
}

#endif /* EDUMKII_NO_LCD */
