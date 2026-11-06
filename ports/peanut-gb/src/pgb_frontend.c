/* Peanut-GB front end (Fox Red Labs, BSD-3-Clause). Peanut-GB itself is MIT and is used unmodified. */
#include "pgb_frontend.h"
#include <frl/abstract.h>
#include <string.h>

/* Build switches (all optional, defaults are the fast path):
 *   PGB_SCALER_AVG      the first scaler: keep the 2-bit picture and average the source pixels each output pixel covers
 *                       (kept as the reference and for 1-bit panels)
 *   PGB_HIGH_LCD_ACCURACY 1 puts Peanut-GB's sprite sorting back (default 0)
 *   PGB_RAMFUNC         put the emulator core in SRAM (".TI.ramfunc", copied by the boot code); default off
 *   PGB_FRAME_SKIP      draw one frame in two (the game still runs every frame) */
#ifndef PGB_HIGH_LCD_ACCURACY
#define PGB_HIGH_LCD_ACCURACY 0
#endif
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#define PEANUT_GB_12_COLOUR 0
#define PEANUT_GB_HIGH_LCD_ACCURACY PGB_HIGH_LCD_ACCURACY
#ifdef PGB_RAMFUNC
#define PGB_HOT __attribute__((section(".TI.ramfunc")))
#pragma clang section text = ".TI.ramfunc"
#else
#define PGB_HOT
#endif
#include "peanut_gb.h"
#ifdef PGB_RAMFUNC
#pragma clang section text = ""
#endif

#define PGB_W 160
#define PGB_H 144

static struct gb_s g_gb;
static const uint8_t *g_rom;
static size_t g_rom_len;
static uint8_t *g_cram;
static size_t g_cram_cap;
static uint8_t g_fb[PGB_W * PGB_H / 4];   /* 2 bits per pixel */
static uint32_t g_frames;
static uint8_t g_lit;

static PGB_HOT uint8_t rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
    (void)gb;
    return addr < g_rom_len ? g_rom[addr] : 0xFF;
}
static PGB_HOT uint8_t cram_read(struct gb_s *gb, const uint_fast32_t addr)
{
    (void)gb;
    return addr < g_cram_cap ? g_cram[addr] : 0xFF;
}
static PGB_HOT void cram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t v)
{
    (void)gb;
    if (addr < g_cram_cap) g_cram[addr] = v;
}
static void on_error(struct gb_s *gb, const enum gb_error_e e, const uint16_t addr)
{
    (void)gb; (void)e; (void)addr;   /* a bad opcode: keep running; the picture will show it */
}

/* ---- fast path: scale each source line as it is drawn and send the output rows at once ---------------------
 * 160 x 144 to 128 x 128. Horizontal: every 5 source pixels make 4 output pixels with weights in eighths
 * (7,1) (5,3) (3,5) (1,7). Vertical: output row y sits at source position 9y/8, blended in eighths between
 * the two rows around it. The sum of the shades times the weights (0 to 192) indexes a table of RGB565 grays.
 * No division, one multiply per output pixel, nothing kept but two 128-byte rows. */
#define PGB_FAST_ROWS 2
static uint8_t g_h[2][FRL_SCREEN_W];          /* horizontally scaled source lines, weight sum 8 per pixel */
static uint16_t g_row[PGB_FAST_ROWS][FRL_SCREEN_W];
static uint16_t g_lut[193];
static const frl_display_t *g_d;
static uint8_t g_fast, g_stream, g_row_sel, g_next_y, g_done, g_drawn_frame;

static void build_lut(const frl_display_t *d)
{
    for (unsigned v = 0; v <= 192; v++) {
        unsigned gray = 255u - (v * 85u + 32u) / 64u;      /* 64 = weight 8 x 8 */
        uint16_t px = (uint16_t)(((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3));
        g_lut[v] = d->stream_be16 ? (uint16_t)((px << 8) | (px >> 8)) : px;
    }
}

static PGB_HOT inline void emit_row(unsigned y, const uint8_t *a, const uint8_t *b)
{
    const frl_display_t *d = g_d;
    uint16_t *o = g_row[g_row_sel];
    unsigned f = (9u * y) & 7u, e = 8u - f;
    for (unsigned x = 0; x < FRL_SCREEN_W; x += 2) {
        o[x] = g_lut[a[x] * e + b[x] * f];
        o[x + 1] = g_lut[a[x + 1] * e + b[x + 1] * f];
    }
    if (g_stream) d->stream_row(d->ctx, o, FRL_SCREEN_W * 2);
    else d->write_lines(d->ctx, (uint16_t)y, 1, o);
    g_row_sel ^= 1;
}

static PGB_HOT void draw_line_fast(struct gb_s *gb, const uint8_t *px, const uint_fast8_t line)
{
    (void)gb;
    const frl_display_t *d = g_d;
    uint8_t *h = g_h[line & 1];
    uint8_t any = 0;
    for (unsigned g = 0; g < 32; g++) {
        const uint8_t *s = px + g * 5;
        unsigned p0 = s[0] & 3, p1 = s[1] & 3, p2 = s[2] & 3, p3 = s[3] & 3, p4 = s[4] & 3;
        uint8_t *o = h + g * 4;
        o[0] = (uint8_t)(7 * p0 + p1);
        o[1] = (uint8_t)(5 * p1 + 3 * p2);
        o[2] = (uint8_t)(3 * p2 + 5 * p3);
        o[3] = (uint8_t)(p3 + 7 * p4);
        any |= (uint8_t)(p0 | p1 | p2 | p3 | p4);
    }
    if (line == 0) { g_lit = 0; g_next_y = 0; if (g_stream) d->stream_begin(d->ctx); g_done = 0; }
    g_lit |= any ? 1 : 0;
    if (line > 0) {
        const uint8_t *a = g_h[(line - 1) & 1], *b = h;
        while (g_next_y < FRL_SCREEN_H && ((9u * g_next_y) >> 3) == (unsigned)(line - 1)) { emit_row(g_next_y, a, b); g_next_y++; }
    }
    if (line == PGB_H - 1) {
        if (g_stream) d->stream_end(d->ctx);
        g_done = 1;
    }
}

static PGB_HOT void draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line)
{
    if (g_fast) { draw_line_fast(gb, pixels, line); return; }
    (void)gb;
    uint8_t *row = g_fb + (size_t)line * (PGB_W / 4);
    uint8_t any = 0;
    for (unsigned x = 0; x < PGB_W; x += 4) {
        uint8_t b = (uint8_t)((pixels[x] & 3) | ((pixels[x + 1] & 3) << 2) | ((pixels[x + 2] & 3) << 4) | ((pixels[x + 3] & 3) << 6));
        row[x >> 2] = b;
        any |= b;
    }
    if (line == 0) g_lit = 0;
    g_lit |= any ? 1 : 0;
}

size_t pgb_state_size(void) { return sizeof g_gb + sizeof g_fb + sizeof g_row + sizeof g_h + sizeof g_lut; }

#define PGB_ROM_SIZE_CODES 9   /* entries in Peanut-GB's num_rom_banks_mask table */
#define PGB_RAM_SIZE_CODES 6   /* entries in its num_ram_banks table */

int pgb_check_rom(const uint8_t *rom, size_t rom_len)
{
    if (!rom || rom_len < 0x150) return -4;
    if (rom[0x148] >= PGB_ROM_SIZE_CODES) return -5;
    if (rom[0x149] >= PGB_RAM_SIZE_CODES) return -6;
    return 0;
}

const char *pgb_error_text(int code)
{
    switch (code) {
    case 0: return "ok";
    case -1: return "ROM refused: bad header checksum or unsupported cartridge type";
    case -2: return "ROM refused: cartridge RAM size code not supported";
    case -3: return "cartridge RAM buffer too small for this ROM";
    case -4: return "ROM refused: file is shorter than the 0x150-byte header";
    case -5: return "ROM refused: ROM size code in the header (0x148) is not 0 to 8";
    case -6: return "ROM refused: RAM size code in the header (0x149) is not 0 to 5";
    default: return "unknown error";
    }
}

int pgb_init(const uint8_t *rom, size_t rom_len, uint8_t *cart_ram, size_t cart_ram_cap)
{
    int chk = pgb_check_rom(rom, rom_len);
    if (chk) return chk;
    g_rom = rom; g_rom_len = rom_len; g_cram = cart_ram; g_cram_cap = cart_ram_cap; g_frames = 0;
    memset(g_fb, 0, sizeof g_fb);
    if (gb_init(&g_gb, rom_read, cram_read, cram_write, on_error, NULL) != GB_INIT_NO_ERROR) return -1;
    size_t need = 0;
    if (gb_get_save_size_s(&g_gb, &need) != 0) return -2;
    if (need > cart_ram_cap) return -3;
    if (cart_ram && need) memset(cart_ram, 0xFF, need);
    gb_init_lcd(&g_gb, draw_line);
    {
        const frl_platform_t *pf = frl_platform();
#ifndef PGB_SCALER_AVG
        g_fast = pf && pf->display.fmt == FRL_FMT_RGB565 && pf->display.write_lines;
#else
        g_fast = 0;
#endif
        g_stream = g_fast && pf->display.stream_row && pf->display.stream_begin && pf->display.stream_end;
        if (g_fast) { g_d = &pf->display; build_lut(g_d); }
        g_done = 0;
    }
#ifdef PGB_FRAME_SKIP
    g_gb.direct.frame_skip = 1;
#endif
    return 0;
}

static void apply_input(void)
{
    uint8_t b = frl_read_buttons();   /* LEFT 0x20 RIGHT 0x40 UP 0x80 DOWN 0x10 A 0x08 B 0x04 */
    uint8_t j = 0xFF;                 /* active low */
    int a = (b & 0x08) != 0, bb = (b & 0x04) != 0;
    if (a && bb) {
        if (b & 0x10) j &= (uint8_t)~JOYPAD_SELECT; else j &= (uint8_t)~JOYPAD_START;
    } else {
        if (a) j &= (uint8_t)~JOYPAD_A;
        if (bb) j &= (uint8_t)~JOYPAD_B;
    }
    if (b & 0x20) j &= (uint8_t)~JOYPAD_LEFT;
    if (b & 0x40) j &= (uint8_t)~JOYPAD_RIGHT;
    if (b & 0x80) j &= (uint8_t)~JOYPAD_UP;
    if (b & 0x10) j &= (uint8_t)~JOYPAD_DOWN;
    g_gb.direct.joypad = j;
}

void pgb_run_frame(void) { apply_input(); gb_run_frame(&g_gb); g_frames++; }
void pgb_run_frame_no_input(void) { gb_run_frame(&g_gb); g_frames++; }
void pgb_present(void)
{
    if (!g_fast) { frl_present_shades2(g_fb, PGB_W, PGB_H); return; }
    if (g_done) { const frl_display_t *d = g_d; if (d->flush) d->flush(d->ctx); g_done = 0; }
}
void pgb_set_frame_skip(int on) { g_gb.direct.frame_skip = on ? 1 : 0; }
uint32_t pgb_frames(void) { return g_frames; }
int pgb_lcd_on(void) { return g_lit; }
