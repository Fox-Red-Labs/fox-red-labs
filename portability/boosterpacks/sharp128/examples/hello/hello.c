/* Hello for the BOOSTXL-SHARP128: text, shapes, a counter, then the microSD card.
 *
 * 1. Draws the title, shapes and a counter; times a full-frame update and a one-row update.
 * 2. With a card in the slot: lists the root folder on the screen, then shows the first
 *    lines of HELLO.TXT (or of the first file found).
 * 3. Runs forever: the counter row is redrawn four times a second by sending only its
 *    16 rows, and the VCOM bit is flipped every second.
 *
 * Build flags: SHARP_NO_SD (no card code), SHARP_BAND_ROWS=n (no frame buffer: the scene is
 * drawn again for each band of n rows, for hosts without 2 kB of spare RAM).
 * Results sit in `hello_res`, readable over the debugger (no serial port needed).
 */
#include <stdint.h>
#include <string.h>
#if (defined(SHARP_BAND_ROWS) || !defined(SHARP_WITH_FATFS)) && !defined(SHARP_NO_SD)
#define SHARP_NO_SD           /* no frame buffer, or no full FatFs: no card screens */
#endif
#include "sharp_lcd.h"
#include "sharp_gfx.h"
#include "sharp_port.h"
#ifndef SHARP_NO_SD
#include "sharp_sd.h"
#include "ff.h"
#endif

#ifndef BOARD_NAME
#define BOARD_NAME "MSPM0"
#endif

struct {
    uint32_t magic;          /* 0x53484100 + stage reached */
    uint32_t full_us;        /* one full-frame update */
    uint32_t row_us;         /* one 1-row update */
    uint32_t rows16_us;      /* one 16-row update (the counter) */
    uint32_t frame_bytes;    /* bytes on the wire for the full frame */
    uint32_t spi_hz;
    int32_t  sd_present;     /* card-detect pin: 1 = card in */
    int32_t  sd_init_rc;     /* 0 = card initialized */
    uint32_t sd_sectors;
    int32_t  sd_mount_rc;    /* FatFs result of f_mount */
    int32_t  sd_files;       /* entries listed in the root folder */
    int32_t  sd_file_bytes;  /* bytes read from the file shown */
    uint32_t counter;
    uint32_t loops;
    uint32_t stack_free; /* bytes of stack never used */
    uint32_t raw_1000b_us; /* 1,000 bytes on the bus with nothing selected: the bus rate */
} hello_res;

#ifdef SHARP_BAND_ROWS
static uint8_t band[SHARP_BAND_ROWS * SHARP_ROW_BYTES];
#else
static uint8_t fb[SHARP_FB_BYTES];
#endif

/* ---- the scene ---- */
static const uint8_t checker[8] = {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA};   /* 8 x 8 */

static void scene(sg_surface_t *s, void *ctx)
{
    int16_t i;
    (void)ctx;
    sg_text(s, 2, 2, "FOX RED", SG_BLACK, 2);
    sg_text(s, 92, 2, "LABS", SG_BLACK, 1);
    sg_text(s, 92, 10, "1-bit", SG_BLACK, 1);
    sg_text(s, 2, 20, "Sharp128 on " BOARD_NAME, SG_BLACK, 1);
    sg_hline(s, 0, 30, 128, SG_BLACK);
    sg_rect(s, 4, 36, 40, 28, SG_BLACK);
    sg_fill_rect(s, 10, 42, 12, 16, SG_BLACK);
    sg_line(s, 4, 36, 43, 63, SG_BLACK);
    sg_circle(s, 70, 50, 14, SG_BLACK);
    sg_circle(s, 70, 50, 8, SG_BLACK);
    for (i = 0; i < 4; i++) sg_blit(s, (int16_t)(96 + (i & 1) * 8), (int16_t)(38 + (i >> 1) * 8), checker, 8, 8, SG_BLACK, 0);
    sg_line(s, 90, 62, 124, 36, SG_BLACK);
    sg_hline(s, 0, 70, 128, SG_BLACK);
    sg_text(s, 2, 74, "COUNT", SG_BLACK, 1);
}

static void counter_row(sg_surface_t *s, uint32_t n)
{
    char b[12];
    sg_fill_rect(s, 0, 88, 128, 16, SG_WHITE);
    sg_utoa(n, b);
    sg_text(s, 2, 88, b, SG_BLACK, 2);
}

static void send_scene(void)
{
#ifdef SHARP_BAND_ROWS
    sg_render_bands(scene, 0, band, SHARP_BAND_ROWS);
#else
    sg_surface_t s;
    sg_surface_init(&s, fb, 0, SHARP_H);
    sg_fill(&s, SG_WHITE);
    scene(&s, 0);
    counter_row(&s, 0);
    sharp_update_full(fb);
#endif
}

/* Redraw the counter's 16 rows and send only them. */
static void send_counter(uint32_t n)
{
    sg_surface_t s;
#ifdef SHARP_BAND_ROWS
    uint8_t y;
    for (y = 88; y < 104; y = (uint8_t)(y + SHARP_BAND_ROWS)) {
        sg_surface_init(&s, band, y, SHARP_BAND_ROWS);
        sg_fill(&s, SG_WHITE);
        counter_row(&s, n);
        sharp_update_lines(band, y, SHARP_BAND_ROWS);
    }
#else
    sg_surface_init(&s, fb, 0, SHARP_H);
    counter_row(&s, n);
    sharp_update_lines(&fb[88 * SHARP_ROW_BYTES], 88, 16);
#endif
}

static void status_line(const char *t)
{
    sg_surface_t s;
#ifdef SHARP_BAND_ROWS
    uint8_t y;
    for (y = 108; y < 128; y = (uint8_t)(y + SHARP_BAND_ROWS)) {
        int16_t h = (int16_t)(128 - y < SHARP_BAND_ROWS ? 128 - y : SHARP_BAND_ROWS);
        sg_surface_init(&s, band, y, h);
        sg_fill(&s, SG_WHITE);
        sg_text(&s, 2, 110, t, SG_BLACK, 1);
        sharp_update_lines(band, y, (uint8_t)h);
    }
#else
    sg_surface_init(&s, fb, 0, SHARP_H);
    sg_fill_rect(&s, 0, 106, 128, 22, SG_WHITE);
    sg_text(&s, 2, 110, t, SG_BLACK, 1);
    sharp_update_lines(&fb[106 * SHARP_ROW_BYTES], 106, 22);
#endif
}

static void measure(void)
{
    uint32_t t0;
    hello_res.spi_hz = sharp_port_spi_hz();
    t0 = sharp_port_micros(); sharp_port_spi_xfer(0, 0, 1000); hello_res.raw_1000b_us = sharp_port_micros() - t0;
#ifndef SHARP_BAND_ROWS
    uint32_t b0 = sharp_stat_bytes();
    t0 = sharp_port_micros(); sharp_update_full(fb); hello_res.full_us = sharp_port_micros() - t0;
    hello_res.frame_bytes = sharp_stat_bytes() - b0;
    t0 = sharp_port_micros(); sharp_update_lines(&fb[64 * SHARP_ROW_BYTES], 64, 1); hello_res.row_us = sharp_port_micros() - t0;
    t0 = sharp_port_micros(); sharp_update_lines(&fb[88 * SHARP_ROW_BYTES], 88, 16); hello_res.rows16_us = sharp_port_micros() - t0;
#else
    uint32_t b0 = sharp_stat_bytes();
    t0 = sharp_port_micros(); send_scene(); hello_res.full_us = sharp_port_micros() - t0;   /* 128 / n bands */
    hello_res.frame_bytes = sharp_stat_bytes() - b0;
    t0 = sharp_port_micros(); sharp_update_lines(band, 64, 1); hello_res.row_us = sharp_port_micros() - t0;
    hello_res.rows16_us = 0;
#endif
}

#ifndef SHARP_NO_SD
/* ---- the card ---- */
static FATFS fs;
static FILINFO fno;
static char status[24];

/* Wait, keeping the panel's VCOM bit flipping. */
static void wait_ms(uint32_t ms)
{
    uint32_t t0 = sharp_port_millis();
    while ((uint32_t)(sharp_port_millis() - t0) < ms) sharp_vcom_tick(sharp_port_millis());
}

static void card_screens(void)
{
    sg_surface_t s;
    DIR dir;
    FIL f;
    char name[24];
    int n = 0, y = 12;
    FRESULT fr;

    hello_res.sd_present = sharp_port_sd_present();
    if (!hello_res.sd_present) { strcpy(status, "SD: no card"); return; }

    sharp_sd_for_card();
    hello_res.sd_init_rc = sharp_sd_init();
    hello_res.sd_sectors = sharp_sd_sectors();
    if (hello_res.sd_init_rc) { sharp_sd_for_lcd(); strcpy(status, "SD: card not ready"); return; }
    fr = f_mount(&fs, "", 1);
    hello_res.sd_mount_rc = (int32_t)fr;
    if (fr != FR_OK) { sharp_sd_for_lcd(); strcpy(status, "SD: not FAT32"); return; }

    /* page 1: folder listing, drawn into the frame buffer after the card work is done */
    sg_surface_init(&s, fb, 0, SHARP_H);
    sg_fill(&s, SG_WHITE);
    sg_text(&s, 2, 2, "SD  /", SG_BLACK, 1);
    sg_hline(&s, 0, 10, 128, SG_BLACK);
    if (f_opendir(&dir, "") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            if (n < 13) {
                strncpy(name, fno.fname, 15); name[15] = 0;
                sg_text(&s, 2, (int16_t)y, name, SG_BLACK, 1);
                if (!(fno.fattrib & AM_DIR)) { char b[12]; sg_utoa((uint32_t)fno.fsize, b); sg_text(&s, (int16_t)(128 - 6 * (int)strlen(b) - 2), (int16_t)y, b, SG_BLACK, 1); }
                else sg_text(&s, 104, (int16_t)y, "DIR", SG_BLACK, 1);
                y += 9;
            }
            n++;
        }
        f_closedir(&dir);
    }
    hello_res.sd_files = n;
    sharp_sd_for_lcd();
    sharp_update_full(fb);
    wait_ms(3000);

    /* page 2: first lines of HELLO.TXT, or of the first file in the folder */
    sharp_sd_for_card();
    fr = f_open(&f, "HELLO.TXT", FA_READ);
    if (fr != FR_OK && f_opendir(&dir, "") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
            if (!(fno.fattrib & AM_DIR)) { fr = f_open(&f, fno.fname, FA_READ); break; }
        f_closedir(&dir);
    }
    sg_fill(&s, SG_WHITE);
    if (fr == FR_OK) {
        char line[22];
        UINT got;
        int col = 0, row = 0, total = 0;
        uint8_t ch;
        sg_text(&s, 2, 2, "FILE", SG_BLACK, 1);
        sg_hline(&s, 0, 10, 128, SG_BLACK);
        while (row < 13 && f_read(&f, &ch, 1, &got) == FR_OK && got == 1) {
            total++;
            if (ch == '\n' || col == 21) {
                line[col] = 0; sg_text(&s, 2, (int16_t)(12 + row * 9), line, SG_BLACK, 1); row++; col = 0;
                if (ch == '\n') continue;
            }
            if (ch >= 32 && ch < 127) line[col++] = (char)ch;
        }
        if (col && row < 13) { line[col] = 0; sg_text(&s, 2, (int16_t)(12 + row * 9), line, SG_BLACK, 1); }
        hello_res.sd_file_bytes = total;
        f_close(&f);
    } else {
        sg_text(&s, 2, 2, "NO FILE TO SHOW", SG_BLACK, 1);
    }
    sharp_sd_for_lcd();
    sharp_update_full(fb);
    wait_ms(4000);
    strcpy(status, "SD ok ");
    { char b[12]; sg_utoa((uint32_t)hello_res.sd_files, b); strcat(status, b); strcat(status, " files"); }
}
#endif

int main(void)
{
    uint32_t last_ms, c = 0;

    sharp_port_stack_paint();
    sharp_port_init();
    hello_res.magic = 0x53484100;
    sharp_init();
    hello_res.magic = 0x53484101;
    send_scene();
    hello_res.magic = 0x53484102;
    measure();
    hello_res.magic = 0x53484103;
#ifndef SHARP_NO_SD
    status[0] = 0;
    card_screens();
    hello_res.magic = 0x53484104;
    {   /* back to the main scene, with the card status line */
        sg_surface_t s;
        sg_surface_init(&s, fb, 0, SHARP_H);
        sg_fill(&s, SG_WHITE);
        scene(&s, 0);
        counter_row(&s, 0);
        sharp_update_full(fb);
    }
    status_line(status);
#else
    status_line("no SD slot use");
#endif

    last_ms = sharp_port_millis();
    for (;;) {
        uint32_t now = sharp_port_millis();
        sharp_vcom_tick(now);
        if ((uint32_t)(now - last_ms) >= 250u) {
            last_ms = now;
            hello_res.counter = ++c;
            send_counter(c);
        }
        hello_res.loops++;
        hello_res.stack_free = sharp_port_stack_free();
    }
}
