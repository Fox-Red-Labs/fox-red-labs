/* Platform registration, frame streaming and stick-to-button mapping (Fox Red Labs, BSD-3-Clause). */
#include <frl/abstract.h>
#include <string.h>

static frl_platform_t g_pf;
static bool g_have_pf = false;
static frl_fit_t g_fit = FRL_FIT_CENTER;

extern "C" {

void frl_set_platform(const frl_platform_t *p) { g_pf = *p; g_have_pf = true; }
const frl_platform_t *frl_platform(void) { return g_have_pf ? &g_pf : 0; }
void frl_set_fit(frl_fit_t f) { g_fit = f; }
frl_fit_t frl_get_fit(void) { return g_fit; }

/* ---- output line makers ------------------------------------------------ */

#define LINES_RGB 4   /* 4 lines x 128 px x 2 B = 1 KB of RAM */
#define LINES_1BPP 8

static uint16_t src_row_for(uint16_t out_y, uint16_t src_h, frl_fit_t fit, int *valid)
{
    if (fit == FRL_FIT_2X_V && src_h * 2u <= FRL_SCREEN_H) {
        uint16_t pad = (FRL_SCREEN_H - src_h * 2u) / 2u;
        if (out_y < pad || out_y >= pad + src_h * 2u) { *valid = 0; return 0; }
        *valid = 1; return (uint16_t)((out_y - pad) / 2u);
    } else {
        uint16_t pad = (FRL_SCREEN_H - src_h) / 2u;
        if (out_y < pad || out_y >= pad + src_h) { *valid = 0; return 0; }
        *valid = 1; return (uint16_t)(out_y - pad);
    }
}

void frl_present_pages(const uint8_t *src, uint16_t src_w, uint16_t src_h, frl_fit_t fit)
{
    if (!g_have_pf || !g_pf.display.write_lines) return;
    const frl_display_t *d = &g_pf.display;
    if (d->fmt == FRL_FMT_RGB565) {
        static uint16_t buf[LINES_RGB * FRL_SCREEN_W];   /* static: keeps the 1 KB off a small stack */
        for (uint16_t y0 = 0; y0 < FRL_SCREEN_H; y0 += LINES_RGB) {
            for (uint16_t k = 0; k < LINES_RGB; k++) {
                int valid; uint16_t sy = src_row_for(y0 + k, src_h, fit, &valid);
                uint16_t *row = buf + k * FRL_SCREEN_W;
                if (!valid) { for (uint16_t x = 0; x < FRL_SCREEN_W; x++) row[x] = d->bg; continue; }
                const uint8_t *page = src + (sy >> 3) * src_w;
                uint8_t sh = sy & 7;
                for (uint16_t x = 0; x < FRL_SCREEN_W; x++) row[x] = ((page[x] >> sh) & 1) ? d->fg : d->bg;
            }
            d->write_lines(d->ctx, y0, LINES_RGB, buf);
        }
    } else {
        static uint8_t buf[LINES_1BPP * (FRL_SCREEN_W / 8)];
        uint8_t inv = d->invert ? 0xFF : 0x00;
        for (uint16_t y0 = 0; y0 < FRL_SCREEN_H; y0 += LINES_1BPP) {
            for (uint16_t k = 0; k < LINES_1BPP; k++) {
                int valid; uint16_t sy = src_row_for(y0 + k, src_h, fit, &valid);
                uint8_t *row = buf + k * (FRL_SCREEN_W / 8);
                if (!valid) { memset(row, inv, FRL_SCREEN_W / 8); continue; }
                const uint8_t *page = src + (sy >> 3) * src_w;
                uint8_t sh = sy & 7;
                for (uint16_t b = 0; b < FRL_SCREEN_W / 8; b++) {
                    uint8_t v = 0;
                    for (uint8_t i = 0; i < 8; i++) v |= (uint8_t)(((page[b * 8 + i] >> sh) & 1) << i);
                    row[b] = v ^ inv;
                }
            }
            d->write_lines(d->ctx, y0, LINES_1BPP, buf);
        }
    }
    if (d->flush) d->flush(d->ctx);
}

static const uint8_t bayer4[4][4] = { {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5} };

void frl_present_shades2(const uint8_t *src, uint16_t src_w, uint16_t src_h)
{
    if (!g_have_pf || !g_pf.display.write_lines) return;
    const frl_display_t *d = &g_pf.display;
    /* Each output pixel covers columns [x*src_w/128, ceil((x+1)*src_w/128)) and the same for rows. The
     * column ranges are worked out once (an M0+ has no divide instruction), the row range once per line. */
    static uint8_t xs0[FRL_SCREEN_W], xn[FRL_SCREEN_W];
    static uint16_t cached_w = 0;
    if (cached_w != src_w) {
        for (uint16_t x = 0; x < FRL_SCREEN_W; x++) {
            uint16_t a0 = (uint16_t)((uint32_t)x * src_w / FRL_SCREEN_W);
            uint16_t a1 = (uint16_t)(((uint32_t)(x + 1) * src_w + FRL_SCREEN_W - 1) / FRL_SCREEN_W);
            if (a1 <= a0) a1 = a0 + 1;
            if (a1 > src_w) a1 = src_w;
            xs0[x] = (uint8_t)a0; xn[x] = (uint8_t)(a1 - a0);
        }
        cached_w = src_w;
    }
    static uint16_t buf16[FRL_SCREEN_W];
    static uint8_t buf1[FRL_SCREEN_W / 8];
    for (uint16_t y = 0; y < FRL_SCREEN_H; y++) {
        uint16_t sy0 = (uint16_t)((uint32_t)y * src_h / FRL_SCREEN_H);
        uint16_t sy1 = (uint16_t)(((uint32_t)(y + 1) * src_h + FRL_SCREEN_H - 1) / FRL_SCREEN_H);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sy1 > src_h) sy1 = src_h;
        uint16_t ny = (uint16_t)(sy1 - sy0);
        if (d->fmt == FRL_FMT_1BPP) memset(buf1, 0, sizeof buf1);
        for (uint16_t x = 0; x < FRL_SCREEN_W; x++) {
            uint32_t sum = 0;
            for (uint16_t yy = sy0; yy < sy1; yy++) {
                uint32_t i = (uint32_t)yy * src_w + xs0[x];
                for (uint16_t k = 0; k < xn[x]; k++, i++) sum += (src[i >> 2] >> ((i & 3u) * 2u)) & 3u;
            }
            uint32_t n = (uint32_t)xn[x] * ny;
            uint32_t avg85;   /* average shade times 85, so 255 is the darkest */
            if (n == 1) avg85 = sum * 85u;
            else if (n == 2) avg85 = (sum * 85u) >> 1;
            else if (n == 4) avg85 = (sum * 85u) >> 2;
            else avg85 = (sum * 85u) / n;
            uint32_t gray = 255u - avg85;
            if (d->fmt == FRL_FMT_RGB565) {
                uint16_t g = (uint16_t)gray;
                buf16[x] = (uint16_t)(((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3));
            } else {
                uint8_t th = (uint8_t)(bayer4[y & 3][x & 3] * 16 + 8);
                if (gray > th) buf1[x >> 3] |= (uint8_t)(1u << (x & 7));
            }
        }
        if (d->fmt == FRL_FMT_1BPP && d->invert)
            for (uint16_t b = 0; b < sizeof buf1; b++) buf1[b] = (uint8_t)~buf1[b];
        d->write_lines(d->ctx, y, 1, d->fmt == FRL_FMT_RGB565 ? (const void *)buf16 : (const void *)buf1);
    }
    if (d->flush) d->flush(d->ctx);
}

/* ---- stick and buttons ------------------------------------------------- */

#define AB_LEFT 0x20
#define AB_RIGHT 0x40
#define AB_UP 0x80
#define AB_DOWN 0x10
#define AB_A 0x08
#define AB_B 0x04
#define STICK_ON 700   /* counts from centre that press a direction */
#define STICK_OFF 400  /* counts from centre that release it        */

static bool g_cal = false;
static int32_t g_cx = 2048, g_cy = 2048;
static uint8_t g_dir = 0;

uint8_t frl_read_buttons(void)
{
    if (!g_have_pf || !g_pf.input.read) return 0;
    frl_input_raw_t r;
    if (!g_cal) {
        if (g_pf.input.centre_x && g_pf.input.centre_y) {
            g_cx = g_pf.input.centre_x; g_cy = g_pf.input.centre_y;
        } else {
            int32_t sx = 0, sy = 0;
            for (int i = 0; i < 8; i++) { g_pf.input.read(g_pf.input.ctx, &r); sx += r.joy_x; sy += r.joy_y; }
            g_cx = sx / 8; g_cy = sy / 8;
        }
        g_cal = true;
    }
    g_pf.input.read(g_pf.input.ctx, &r);
    int32_t dx = (int32_t)r.joy_x - g_cx, dy = (int32_t)r.joy_y - g_cy;
    if (g_pf.input.invert_x) dx = -dx;
    if (g_pf.input.invert_y) dy = -dy;
    uint8_t d = g_dir;
    #define AXIS(delta, plus, minus) do { \
        if (d & (plus)) { if ((delta) < STICK_OFF) d &= (uint8_t)~(plus); } \
        else if ((delta) > STICK_ON) d |= (plus); \
        if (d & (minus)) { if ((delta) > -STICK_OFF) d &= (uint8_t)~(minus); } \
        else if ((delta) < -STICK_ON) d |= (minus); } while (0)
    AXIS(dx, AB_RIGHT, AB_LEFT);
    AXIS(dy, AB_UP, AB_DOWN);
    g_dir = d;
    uint8_t out = d;
    if (r.buttons & FRL_BTN_1) out |= AB_A;
    if (r.buttons & FRL_BTN_2) out |= AB_B;
    return out;
}

} /* extern "C" */
