/* Drawing on top of the two display calls in hal.h: rectangles and scaled text. */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "hal.h"

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#ifndef GFX_BUF_PX
#define GFX_BUF_PX 1536          /* scratch pixels: the one RAM buffer drawing needs (3 kB) */
#endif
extern uint16_t gfx_buf[GFX_BUF_PX];

const uint8_t *font_glyph(char c);
extern uint32_t gfx_px_sent;                                                 /* pixels sent since boot (for the measurements) */
void gfx_blit(int x, int y, int w, int h, const uint16_t *px);
void gfx_fill(int x, int y, int w, int h, uint16_t c);
void gfx_frame(int x, int y, int w, int h, int t, uint16_t c);              /* outline of thickness t */
int  gfx_text_w(const char *s, int scale);                                  /* width in pixels */
void gfx_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg);
void gfx_text_center(int x, int y, int w, const char *s, int scale, uint16_t fg, uint16_t bg);  /* centered in w, background fills the line */
/* Put one glyph into a w-wide buffer (for pictures built in gfx_buf). */
void gfx_buf_glyph(uint16_t *buf, int bw, int bh, int ox, int oy, char c, int scale, uint16_t fg);
char *gfx_utoa(uint32_t v, char *out);                                      /* decimal, returns out */
#endif
