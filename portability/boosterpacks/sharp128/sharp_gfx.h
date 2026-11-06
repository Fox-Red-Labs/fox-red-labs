/* 1-bit drawing for the 128 x 128 Sharp panel, portable C.
 *
 * Every call draws into a surface: a band of rows of the screen held in RAM. A full
 * frame is a surface of 128 rows (2 kB). A host with less RAM draws the scene again
 * for each band of 1 to 16 rows and sends the band (sg_render_bands), so one drawing
 * routine serves both. Coordinates are always screen coordinates (0,0 top left);
 * anything outside the band or the screen is clipped.
 *
 * Colors: SG_BLACK is 0 and SG_WHITE is 1, the panel's own levels.
 */
#ifndef SHARP_GFX_H
#define SHARP_GFX_H
#include <stdint.h>
#include "sharp_lcd.h"

#define SG_BLACK 0
#define SG_WHITE 1

typedef struct {
    uint8_t *buf;      /* rows y0 .. y0+h-1, 16 bytes a row, bit 0 = leftmost pixel */
    int16_t  y0;       /* screen row of the first buffered row */
    int16_t  h;        /* buffered rows */
} sg_surface_t;

void sg_surface_init(sg_surface_t *s, uint8_t *buf, int16_t y0, int16_t h);
void sg_fill(sg_surface_t *s, uint8_t color);
void sg_pixel(sg_surface_t *s, int16_t x, int16_t y, uint8_t color);
int  sg_get(const sg_surface_t *s, int16_t x, int16_t y);          /* -1 outside */
void sg_hline(sg_surface_t *s, int16_t x, int16_t y, int16_t len, uint8_t color);
void sg_vline(sg_surface_t *s, int16_t x, int16_t y, int16_t len, uint8_t color);
void sg_line(sg_surface_t *s, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void sg_rect(sg_surface_t *s, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void sg_fill_rect(sg_surface_t *s, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void sg_circle(sg_surface_t *s, int16_t cx, int16_t cy, int16_t r, uint8_t color);

/* Text: 5 x 7 font in a 6 x 8 cell, scaled by an integer 1 to 4. Returns the x after the
 * last character. '\n' moves to the next line at the starting x. */
int16_t sg_text(sg_surface_t *s, int16_t x, int16_t y, const char *str, uint8_t color, uint8_t scale);
#define SG_CELL_W 6
#define SG_CELL_H 8

/* 1-bit bitmap in the screen's row layout (rows padded to whole bytes, bit 0 leftmost,
 * the XBM convention). Set bits are drawn in `color`; clear bits are left alone, unless
 * opaque is nonzero, which draws them in the opposite color. */
void sg_blit(sg_surface_t *s, int16_t x, int16_t y, const uint8_t *bmp, int16_t w, int16_t h,
             uint8_t color, uint8_t opaque);

/* Unsigned decimal into buf (at least 11 bytes). Returns the length. */
int sg_utoa(uint32_t v, char *buf);

/* Draw `fn` once per band of `band_rows` rows and send each band to the panel.
 * `band_buf` holds band_rows * 16 bytes. band_rows = 128 with a 2 kB buffer is one
 * full-frame update. The band is cleared to white before each call. */
typedef void (*sg_draw_fn)(sg_surface_t *s, void *ctx);
void sg_render_bands(sg_draw_fn fn, void *ctx, uint8_t *band_buf, uint8_t band_rows);

#endif
