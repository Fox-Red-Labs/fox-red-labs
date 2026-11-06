/* Host test for the drawing and font code. A model of the ST7735S RAM (132 x 162) sits behind
 * the port: it follows CS, D/C, CASET, RASET and RAMWR and stores every pixel written.
 * Build and run: test/run.sh. */
#include "edumkii.h"
#include <stdio.h>
#include <string.h>

#define RW 132
#define RH 162
static uint16_t ram[RH][RW];
static int cs = 1, dc = 1, cur_cmd = -1;
static int xs, xe, ys, ye, px, py, argn;
static uint8_t args[8];
static int bulk_calls, spi_calls, madctl = -1, colmod = -1, reset_seen, dispon, slpout;
static int pending_hi = -1;
static int bad_cs;

static void g_write(void *c, edumkii_pin_t p, int l)
{
    (void)c;
    if (p == EDUMKII_PIN_LCD_CS) cs = l;
    if (p == EDUMKII_PIN_LCD_DC) dc = l;
    if (p == EDUMKII_PIN_LCD_RST && !l) reset_seen = 1;
}
static int g_read(void *c, edumkii_pin_t p) { (void)c; (void)p; return 1; }

static void model_byte(uint8_t b)
{
    if (cs) { bad_cs++; return; }
    if (!dc) {
        cur_cmd = b; argn = 0; pending_hi = -1;
        if (b == 0x2C) { px = xs; py = ys; }
        if (b == 0x11) slpout = 1;
        if (b == 0x29) dispon = 1;
        return;
    }
    switch (cur_cmd) {
    case 0x2A: args[argn++ & 7] = b; if (argn == 4) { xs = (args[0] << 8) | args[1]; xe = (args[2] << 8) | args[3]; } break;
    case 0x2B: args[argn++ & 7] = b; if (argn == 4) { ys = (args[0] << 8) | args[1]; ye = (args[2] << 8) | args[3]; } break;
    case 0x36: madctl = b; break;
    case 0x3A: colmod = b; break;
    case 0x2C:
        if (pending_hi < 0) { pending_hi = b; break; }
        if (py <= ye && py < RH && px < RW) ram[py][px] = (uint16_t)((pending_hi << 8) | b);
        pending_hi = -1;
        if (++px > xe) { px = xs; py++; }
        break;
    default: break;
    }
}
static void g_spi(void *c, const uint8_t *d, uint32_t n) { (void)c; spi_calls++; for (uint32_t i = 0; i < n; i++) model_byte(d[i]); }
static void g_bulk(void *c, const uint8_t *d, uint32_t n) { bulk_calls++; for (uint32_t i = 0; i < n; i++) model_byte(d[i]); (void)c; }
static void g_us(void *c, uint32_t us) { (void)c; (void)us; }
static void g_ms(void *c, uint32_t ms) { (void)c; (void)ms; }

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* panel pixel (x, y) in the 128 x 128 view */
static uint16_t at(int x, int y) { return ram[y + EDUMKII_ROW_OFFSET][x + EDUMKII_COL_OFFSET]; }

static void clear_ram(void) { memset(ram, 0xAA, sizeof ram); }

int main(void)
{
    edumkii_t d;
    edumkii_port_t p = { 0 };
    p.gpio_write = g_write; p.gpio_read = g_read; p.spi_write = g_spi; p.spi_write_bulk = g_bulk;
    p.delay_us = g_us; p.delay_ms = g_ms;
    CHECK(edumkii_init(&d, &p) == EDUMKII_OK, "init");
    CHECK(edumkii_lcd_init(&d) == EDUMKII_OK, "lcd_init");
    CHECK(reset_seen && slpout && dispon, "init sequence: reset %d sleep-out %d display-on %d", reset_seen, slpout, dispon);
    CHECK(madctl == EDUMKII_MADCTL && colmod == 5, "madctl %02x colmod %d", madctl, colmod);
    CHECK(bad_cs == 0, "bytes sent with CS high: %d", bad_cs);
    CHECK(cs == 1, "CS left low");

    /* init leaves the visible area black */
    int nonblack = 0;
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) if (at(x, y) != 0) nonblack++;
    CHECK(nonblack == 0, "after init %d pixels not black", nonblack);

    /* fill */
    clear_ram(); bulk_calls = 0;
    edumkii_fill(&d, EDUMKII_RED);
    int wrong = 0;
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) if (at(x, y) != EDUMKII_RED) wrong++;
    CHECK(wrong == 0, "fill: %d wrong pixels", wrong);
    CHECK(ram[0][0] == 0xAAAA && ram[RH - 1][RW - 1] == 0xAAAA, "fill wrote outside the window");
    CHECK(bulk_calls > 0, "fill did not use the bulk path");

    /* rect, with clipping */
    edumkii_fill(&d, EDUMKII_BLACK);
    edumkii_rect(&d, 10, 20, 30, 5, EDUMKII_GREEN);
    int inside = 0, outside = 0;
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
        int in = x >= 10 && x < 40 && y >= 20 && y < 25;
        if (at(x, y) == EDUMKII_GREEN) inside++;
        if (in != (at(x, y) == EDUMKII_GREEN)) outside++;
    }
    CHECK(inside == 150 && outside == 0, "rect: %d green, %d wrong", inside, outside);
    edumkii_rect(&d, 120, 120, 50, 50, EDUMKII_BLUE);
    CHECK(at(127, 127) == EDUMKII_BLUE && at(120, 120) == EDUMKII_BLUE && at(119, 120) != EDUMKII_BLUE, "rect clip at the corner");
    edumkii_rect(&d, -5, -5, 10, 10, EDUMKII_YELLOW);
    CHECK(at(0, 0) == EDUMKII_YELLOW && at(4, 4) == EDUMKII_YELLOW && at(5, 5) != EDUMKII_YELLOW, "rect clip at the origin");

    /* frame and pixel */
    edumkii_fill(&d, EDUMKII_BLACK);
    edumkii_frame(&d, 4, 4, 10, 6, EDUMKII_WHITE);
    int w = 0;
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) w += at(x, y) == EDUMKII_WHITE;
    CHECK(w == 2 * 10 + 2 * 4, "frame: %d white pixels, want 28", w);
    edumkii_pixel(&d, 77, 99, EDUMKII_CYAN);
    CHECK(at(77, 99) == EDUMKII_CYAN && at(78, 99) != EDUMKII_CYAN, "pixel");
    edumkii_pixel(&d, 128, 0, EDUMKII_CYAN); edumkii_pixel(&d, -1, 0, EDUMKII_CYAN);   /* ignored */

    /* line: diagonal hits both ends and 128 pixels */
    edumkii_fill(&d, EDUMKII_BLACK);
    edumkii_line(&d, 0, 0, 127, 127, EDUMKII_MAGENTA);
    int diag = 0;
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) diag += at(x, y) == EDUMKII_MAGENTA;
    CHECK(diag == 128 && at(0, 0) == EDUMKII_MAGENTA && at(127, 127) == EDUMKII_MAGENTA && at(64, 64) == EDUMKII_MAGENTA, "diagonal: %d", diag);

    /* blit (host order) and blit_raw */
    static uint16_t img[12 * 9];
    for (int i = 0; i < 12 * 9; i++) img[i] = (uint16_t)(0x1234 + i * 7);
    edumkii_blit(&d, 50, 60, 12, 9, img);
    int bad = 0;
    for (int y = 0; y < 9; y++) for (int x = 0; x < 12; x++) bad += at(50 + x, 60 + y) != img[y * 12 + x];
    CHECK(bad == 0, "blit: %d wrong", bad);
    static uint8_t raw[128 * 128 * 2];
    for (int i = 0; i < 128 * 128; i++) { raw[2 * i] = (uint8_t)(i >> 8); raw[2 * i + 1] = (uint8_t)i; }
    bulk_calls = 0;
    edumkii_blit_raw(&d, 0, 0, 128, 128, raw);
    bad = 0;
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) bad += at(x, y) != (uint16_t)(y * 128 + x);
    CHECK(bad == 0 && bulk_calls == 1, "blit_raw: %d wrong, %d bulk calls", bad, bulk_calls);

    /* font: three glyphs against hand-checked art ('.' off, '#' on), rows top to bottom */
    static const char *want_A[7] = { ".###.", "#...#", "#...#", "#...#", "#####", "#...#", "#...#" };
    static const char *want_1[7] = { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." };
    static const char *want_e[7] = { ".....", ".....", ".###.", "#...#", "#####", "#....", ".###." };
    struct { char c; const char **art; } g[3] = { {'A', want_A}, {'1', want_1}, {'e', want_e} };
    for (int k = 0; k < 3; k++) {
        const uint8_t *col = edumkii_font5x7(g[k].c);
        int mism = 0;
        for (int row = 0; row < 7; row++) for (int c = 0; c < 5; c++)
            if ((((col[c] >> row) & 1) ? '#' : '.') != g[k].art[row][c]) mism++;
        CHECK(mism == 0, "glyph '%c': %d cells differ from the reference art", g[k].c, mism);
    }
    /* glyph sanity for the whole table: every printable glyph except space has at least 5 lit cells,
       the top bit (row 7) is never used, and no two letters share a shape */
    for (int c = 33; c < 127; c++) {
        const uint8_t *col = edumkii_font5x7((char)c);
        int lit = 0;
        for (int i = 0; i < 5; i++) { lit += __builtin_popcount(col[i]); CHECK((col[i] & 0x80) == 0, "glyph %d uses bit 7", c); }
        CHECK(lit >= 3, "glyph %c has %d lit cells", c, lit);
        for (int e = c + 1; e < 127; e++) CHECK(memcmp(col, edumkii_font5x7((char)e), 5) != 0, "glyphs %c and %c are identical", c, e);
    }
    CHECK(edumkii_font5x7(0x07) == edumkii_font5x7('?'), "out-of-range character maps to ?");

    /* text on the panel at scale 1 and 2 */
    edumkii_fill(&d, EDUMKII_BLACK);
    int xe2 = edumkii_text(&d, 8, 8, "Hi", EDUMKII_WHITE, EDUMKII_BLUE, 1);
    CHECK(xe2 == 8 + 12, "text advance %d", xe2);
    const uint8_t *H = edumkii_font5x7('H');
    int mism = 0;
    for (int row = 0; row < 8; row++) for (int c = 0; c < 6; c++) {
        int on = (c < 5 && row < 7) ? (H[c] >> row) & 1 : 0;
        mism += at(8 + c, 8 + row) != (on ? EDUMKII_WHITE : EDUMKII_BLUE);
    }
    CHECK(mism == 0, "scale 1 cell: %d wrong", mism);
    edumkii_text(&d, 40, 40, "A", EDUMKII_WHITE, EDUMKII_BLACK, 2);
    const uint8_t *A = edumkii_font5x7('A');
    mism = 0;
    for (int row = 0; row < 16; row++) for (int c = 0; c < 12; c++) {
        int on = (c / 2 < 5 && row / 2 < 7) ? (A[c / 2] >> (row / 2)) & 1 : 0;
        mism += at(40 + c, 40 + row) != (on ? EDUMKII_WHITE : EDUMKII_BLACK);
    }
    CHECK(mism == 0, "scale 2 cell: %d wrong", mism);
    int xn = edumkii_number(&d, 0, 100, -305, 6, EDUMKII_WHITE, EDUMKII_BLACK, 1);
    CHECK(xn == 36, "number advance %d", xn);
    CHECK(bad_cs == 0, "bytes sent with CS high: %d", bad_cs);

    /* ASCII dump of the rendered text, for the log */
    edumkii_fill(&d, EDUMKII_BLACK);
    edumkii_text(&d, 0, 0, "EDUMKII 0123", EDUMKII_WHITE, EDUMKII_BLACK, 1);
    for (int y = 0; y < 8; y++) { for (int x = 0; x < 72; x++) putchar(at(x, y) ? '#' : '.'); putchar('\n'); }

    /* No-bulk fallback and missing-port behaviour */
    edumkii_port_t q = p; q.spi_write_bulk = NULL;
    edumkii_t e; edumkii_init(&e, &q);
    clear_ram(); edumkii_fill(&e, EDUMKII_GREEN);
    CHECK(at(0, 0) == EDUMKII_GREEN && at(127, 127) == EDUMKII_GREEN, "fill without the bulk function");
    edumkii_port_t none = { 0 }; none.delay_us = g_us; none.delay_ms = g_ms;
    edumkii_init(&e, &none);
    CHECK(edumkii_lcd_init(&e) == EDUMKII_ERR_PORT, "lcd_init without gpio and spi must fail");
    uint32_t lux;
    CHECK(edumkii_lux_read(&e, &lux) == EDUMKII_ERR_PORT, "lux without i2c must fail");
    CHECK(edumkii_adc(&e, EDUMKII_ADC_MIC) == 0 && edumkii_button(&e, EDUMKII_PIN_S1) == 0, "inputs without a port read 0");
    edumkii_port_t bad_port = { 0 };
    CHECK(edumkii_init(&e, &bad_port) == EDUMKII_ERR_PORT, "init without delays must fail");

    printf(fails ? "FAILED: %d\n" : "OK: all checks passed\n", fails);
    return fails ? 1 : 0;
}
