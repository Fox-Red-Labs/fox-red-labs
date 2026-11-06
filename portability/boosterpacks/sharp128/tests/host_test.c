/* Host test for the Sharp128 core: panel packing, VCOM, drawing, band rendering.
 * Build and run on the Mac:  ./tests/run.sh   (plain cc, no board needed). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../sharp_lcd.h"
#include "../sharp_gfx.h"

/* ---- mock port: records every byte the core puts on the wire ---- */
static uint8_t wire[1 << 16];
static uint32_t wlen;
static int cs, power, disp;
static int cs_rises, cs_bytes_while_low;

void sharp_port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint32_t n)
{
    uint32_t i;
    if (!cs) cs_bytes_while_low += (int)n;
    for (i = 0; i < n; i++) { if (wlen < sizeof wire) wire[wlen++] = tx ? tx[i] : 0xFF; if (rx) rx[i] = 0xFF; }
}
void sharp_port_lcd_cs(int l) { if (l && !cs) cs_rises++; cs = l; }
void sharp_port_lcd_power(int l) { power = l; }
void sharp_port_lcd_disp(int l) { disp = l; }
void sharp_port_delay_us(uint32_t us) { (void)us; }

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t rev(uint8_t b) { uint8_t r = 0; int i; for (i = 0; i < 8; i++) if (b & (1 << i)) r |= (uint8_t)(0x80 >> i); return r; }
static void wire_reset(void) { wlen = 0; cs_rises = 0; cs_bytes_while_low = 0; }

static uint8_t fb[SHARP_FB_BYTES], fb2[SHARP_FB_BYTES];

static void scene(sg_surface_t *s, void *ctx)
{
    (void)ctx;
    sg_text(s, 4, 4, "FOX RED\nLABS 123", SG_BLACK, 1);
    sg_text(s, 10, 40, "Hi", SG_BLACK, 3);
    sg_rect(s, 2, 2, 124, 124, SG_BLACK);
    sg_line(s, 0, 127, 127, 0, SG_BLACK);
    sg_line(s, 5, 100, 120, 70, SG_BLACK);
    sg_circle(s, 64, 90, 20, SG_BLACK);
    sg_fill_rect(s, 90, 20, 30, 12, SG_BLACK);
    sg_fill_rect(s, 100, 22, 8, 8, SG_WHITE);
}

int main(void)
{
    uint32_t i, k, y;
    int n;

    /* --- init --- */
    sharp_init();
    CHECK(power == 1 && disp == 1 && cs == 0, "init leaves power on, DISP on, CS low");
    CHECK(wlen == 2, "init sends a 2-byte clear, got %u", wlen);
    CHECK(wire[0] == rev(0x04), "clear command byte %02x", wire[0]);

    /* --- full update packing --- */
    memset(fb, 0xFF, sizeof fb);
    for (y = 0; y < SHARP_H; y++) { fb[y * 16] = (uint8_t)(0x01 + y); fb[y * 16 + 15] = (uint8_t)(0xA0 ^ y); }
    wire_reset();
    sharp_update_full(fb);
    CHECK(wlen == 2306, "full frame is %u bytes on the wire, want 2306", wlen);
    CHECK(cs_rises == 1 && cs_bytes_while_low == 0, "one chip-select cycle, all bytes inside it");
    CHECK(wire[0] == rev(0x01 | 0x02) || wire[0] == rev(0x01), "mode byte is write (+VCOM): %02x", wire[0]);
    for (y = 0; y < SHARP_H; y++) {
        const uint8_t *l = &wire[1 + y * 18];
        CHECK(l[0] == rev((uint8_t)(y + 1)), "row %u address %02x", y, l[0]);
        CHECK(l[1] == rev(fb[y * 16]) && l[16] == rev(fb[y * 16 + 15]), "row %u data", y);
        CHECK(l[17] == 0, "row %u trailer", y);
    }
    CHECK(wire[2305] == 0, "frame trailer");

    /* --- partial update --- */
    wire_reset();
    sharp_update_lines(&fb[16 * 10], 10, 3);
    CHECK(wlen == 1 + 3 * 18 + 1, "3 rows are %u bytes", wlen);
    CHECK(wire[1] == rev(11) && wire[1 + 18] == rev(12) && wire[1 + 36] == rev(13), "row addresses 11 12 13");

    /* --- VCOM flips on every command and the tick sends only after a second --- */
    wire_reset(); sharp_vcom_tick(500);
    CHECK(wlen == 0, "no tick before 1000 ms");
    sharp_vcom_tick(1000);
    CHECK(wlen == 2, "tick sends 2 bytes");
    {
        uint8_t a, b, c;
        wire_reset(); sharp_update_lines(fb, 0, 1); a = wire[0];
        wire_reset(); sharp_update_lines(fb, 0, 1); b = wire[0];
        wire_reset(); sharp_vcom_tick(2000);         c = wire[0];
        CHECK(a != b && b != c, "VCOM bit alternates (%02x %02x %02x)", a, b, c);
        CHECK(((a ^ b) == rev(0x02)) && ((b ^ c) & rev(0x02)) == rev(0x02), "only the VCOM bit differs");
    }

    /* --- drawing: pixel order and the bit layout --- */
    memset(fb, 0xFF, sizeof fb);
    {
        sg_surface_t s; sg_surface_init(&s, fb, 0, 128);
        sg_pixel(&s, 0, 0, SG_BLACK);
        sg_pixel(&s, 9, 3, SG_BLACK);
        sg_pixel(&s, 127, 127, SG_BLACK);
        CHECK(fb[0] == 0xFE, "pixel (0,0) is bit 0 of byte 0: %02x", fb[0]);
        CHECK(fb[3 * 16 + 1] == 0xFD, "pixel (9,3) is bit 1 of byte 1 of row 3: %02x", fb[3 * 16 + 1]);
        CHECK(fb[127 * 16 + 15] == 0x7F, "pixel (127,127): %02x", fb[127 * 16 + 15]);
        CHECK(sg_get(&s, 9, 3) == 0 && sg_get(&s, 10, 3) == 1 && sg_get(&s, 128, 0) == -1, "sg_get");
        sg_pixel(&s, -1, 5, SG_BLACK); sg_pixel(&s, 200, 5, SG_BLACK); sg_pixel(&s, 5, 128, SG_BLACK);
        n = 0; for (i = 0; i < sizeof fb; i++) n += 8 - __builtin_popcount(fb[i]);
        CHECK(n == 3, "clipped pixels draw nothing (%d black)", n);
        sg_fill(&s, SG_WHITE);
        sg_hline(&s, 3, 5, 20, SG_BLACK);
        n = 0; for (i = 0; i < sizeof fb; i++) n += 8 - __builtin_popcount(fb[i]);
        CHECK(n == 20 && sg_get(&s, 3, 5) == 0 && sg_get(&s, 22, 5) == 0 && sg_get(&s, 23, 5) == 1 && sg_get(&s, 2, 5) == 1, "hline 20 px");
        sg_fill(&s, SG_WHITE);
        sg_hline(&s, -10, 0, 300, SG_BLACK);
        n = 0; for (i = 0; i < 16; i++) n += 8 - __builtin_popcount(fb[i]);
        CHECK(n == 128, "hline clipped to the row (%d)", n);
        sg_fill(&s, SG_WHITE);
        sg_rect(&s, 10, 10, 20, 10, SG_BLACK);
        n = 0; for (i = 0; i < sizeof fb; i++) n += 8 - __builtin_popcount(fb[i]);
        CHECK(n == 2 * 20 + 2 * 8, "rect outline 20x10 = 56 px, got %d", n);
        sg_fill(&s, SG_WHITE);
        sg_fill_rect(&s, 10, 10, 20, 10, SG_BLACK);
        n = 0; for (i = 0; i < sizeof fb; i++) n += 8 - __builtin_popcount(fb[i]);
        CHECK(n == 200, "filled rect 200 px, got %d", n);
        sg_fill(&s, SG_WHITE);
        sg_line(&s, 0, 0, 127, 127, SG_BLACK);
        CHECK(sg_get(&s, 64, 64) == 0 && sg_get(&s, 127, 127) == 0 && sg_get(&s, 0, 0) == 0, "diagonal");
        n = 0; for (i = 0; i < sizeof fb; i++) n += 8 - __builtin_popcount(fb[i]);
        CHECK(n == 128, "diagonal is 128 px, got %d", n);
        /* text: 'A' in the classic font, column 1 = 0x11 (rows 0 and 4) */
        sg_fill(&s, SG_WHITE);
        sg_text(&s, 0, 0, "A", SG_BLACK, 1);
        CHECK(sg_get(&s, 0, 0) == 1 && sg_get(&s, 0, 1) == 0 && sg_get(&s, 0, 6) == 0 && sg_get(&s, 0, 7) == 1 && sg_get(&s, 1, 0) == 0 && sg_get(&s, 1, 1) == 1 && sg_get(&s, 1, 4) == 0, "glyph A shape");
        CHECK(sg_text(&s, 0, 0, "ABC", SG_BLACK, 2) == 36, "text advance is 6 px a character times scale");
        { char b[12]; CHECK(sg_utoa(0, b) == 1 && !strcmp(b, "0"), "utoa 0"); CHECK(sg_utoa(4294967295u, b) == 10 && !strcmp(b, "4294967295"), "utoa max"); }
        /* blit: XBM layout, 8x2 bitmap */
        sg_fill(&s, SG_WHITE);
        { static const uint8_t bm[2] = {0x01, 0x80}; sg_blit(&s, 20, 20, bm, 8, 2, SG_BLACK, 0);
          CHECK(sg_get(&s, 20, 20) == 0 && sg_get(&s, 21, 20) == 1 && sg_get(&s, 27, 21) == 0 && sg_get(&s, 26, 21) == 1, "blit places bit 0 leftmost"); }
    }

    /* --- band rendering equals the full-frame render, for every band height --- */
    {
        static const uint8_t heights[] = {1, 2, 8, 16, 128};
        sg_surface_t s;
        int hi;
        sg_surface_init(&s, fb, 0, 128);
        sg_fill(&s, SG_WHITE);
        scene(&s, 0);
        memcpy(fb2, fb, sizeof fb);
        for (hi = 0; hi < 5; hi++) {
            static uint8_t band[SHARP_FB_BYTES];
            uint8_t bh = heights[hi];
            wire_reset();
            sg_render_bands(scene, 0, band, bh);
            /* decode what went on the wire back into a frame */
            {
                uint8_t got[SHARP_FB_BYTES];
                uint32_t p = 0, seen = 0;
                memset(got, 0x5A, sizeof got);
                while (p < wlen) {
                    /* mode byte, then rows, then the trailer */
                    p++;
                    while (p + 18 <= wlen && rev(wire[p]) != 0 && rev(wire[p]) <= 128 && !(p + 1 == wlen)) {
                        uint8_t row = (uint8_t)(rev(wire[p]) - 1);
                        for (k = 0; k < 16; k++) got[row * 16 + k] = rev(wire[p + 1 + k]);
                        seen++; p += 18;
                        if (p + 1 >= wlen) break;
                        if (rev(wire[p]) == 0) break;
                    }
                    p++;   /* trailer */
                }
                CHECK(seen == 128, "band height %u: %u rows seen", bh, (unsigned)seen);
                CHECK(memcmp(got, fb2, sizeof got) == 0, "band height %u: wire frame differs from full render", bh);
            }
        }
    }

    printf(fails ? "FAILED: %d checks\n" : "PASS: all checks\n", fails);
    return fails != 0;
}
