/* Video player for the BOOSTXL-SHARP128: streams 1-bit 128 x 128 frames from the microSD card.
 *
 * The card holds VIDEO.BIN made by player/video_to_frames.py: a 512-byte header, then frames
 * of 2,048 bytes (16 bytes a row, bit 0 = leftmost pixel, 1 = white), the panel's own layout.
 *
 * Two ways to read a frame, chosen at build time:
 *   full frame (default, FatFs):   the 2,048 bytes go into a frame buffer, then one panel update.
 *   SHARP_CHUNK_ROWS=n (no frame buffer): n rows are read from the card, sent to the panel, the
 *       next n rows read, and so on. The card and the panel share the bus, so a frame is
 *       128/n card reads and 128/n panel selections. This is what fits 1 kB of RAM (Petit FatFs).
 * Frames are paced to the header's fps. If the host cannot keep up it plays slower and
 * `late` counts the frames that missed their time.
 * Results sit in `player_res`, readable over the debugger.
 */
#include <stdint.h>
#include <string.h>
#include "sharp_lcd.h"
#include "sharp_gfx.h"
#include "sharp_sd.h"
#include "sharp_port.h"
#ifdef SHARP_WITH_PFF
#include "pff.h"
#else
#include "ff.h"
#endif

#ifndef PLAY_LOOPS
#define PLAY_LOOPS 1          /* passes over the file before stopping on the last frame */
#endif
#ifndef FPS_OVERRIDE
#define FPS_OVERRIDE 0        /* nonzero: pace to this instead of the header's fps */
#endif

struct {
    uint32_t magic;           /* 0x504C4100 + stage reached */
    int32_t  err;             /* 0 = fine; negative = where it stopped */
    uint32_t fps_target;
    uint32_t frames_in_file;
    uint32_t frames_shown;
    uint32_t late;            /* frames that started after their time */
    uint32_t elapsed_ms;      /* first frame start to last frame end */
    uint32_t sd_us;           /* total time in card reads (includes SPI clock changes) */
    uint32_t lcd_us;          /* total time in panel updates */
    uint32_t bytes_read;
    uint32_t fps_x100;        /* achieved: frames_shown * 100000 / elapsed_ms */
    uint32_t sd_kBps_x10;     /* card read rate while reading: bytes * 10000 / sd_us (kB = 1000 B) */
    uint32_t sd_hz;
    uint32_t sd_sectors;
    uint32_t sdhc;
    uint32_t stack_free;      /* bytes of stack never used */
    int32_t  fr;              /* FatFs / Petit FatFs result of the call that failed */
} player_res;

#ifdef SHARP_CHUNK_ROWS
#define CHUNK (SHARP_CHUNK_ROWS * SHARP_ROW_BYTES)
static uint8_t buf[CHUNK];
#else
static uint8_t fb[SHARP_FB_BYTES];
#define buf fb
#endif

/* ---- file access: FatFs or Petit FatFs ---- */
#ifdef SHARP_WITH_PFF
static FATFS fs;
static int vopen(void)
{
    if ((player_res.fr = pf_mount(&fs)) != FR_OK) return -10;
    return (player_res.fr = pf_open("VIDEO.BIN")) == FR_OK ? 0 : -11;
}
static int vread(uint8_t *b, uint32_t n)
{
    UINT br;
    return (pf_read(b, (UINT)n, &br) == FR_OK && br == n) ? 0 : -1;
}
static void vclose(void) { }
#else
static FATFS fs;
static FIL fil;
static int vopen(void)
{
    if ((player_res.fr = f_mount(&fs, "", 1)) != FR_OK) return -10;
    return (player_res.fr = f_open(&fil, "VIDEO.BIN", FA_READ)) == FR_OK ? 0 : -11;
}
static int vread(uint8_t *b, uint32_t n)
{
    UINT br;
    return (f_read(&fil, b, (UINT)n, &br) == FR_OK && br == n) ? 0 : -1;
}
static void vclose(void) { f_close(&fil); }
#endif

static void msg_scene(sg_surface_t *s, void *ctx)
{
    sg_text(s, 4, 56, (const char *)ctx, SG_BLACK, 1);
}

static void show(const char *m)
{
    sharp_sd_for_lcd();
    sg_render_bands(msg_scene, (void *)m, buf, 1);
}

static void hold(void)
{
    for (;;) sharp_vcom_tick(sharp_port_millis());
}

static void fail(int32_t e, const char *m)
{
    player_res.err = e;
    show(m);
    hold();
}

int main(void)
{
    uint32_t frames, fps, i, t0, start_ms, period_us_x16;
    uint32_t pass;
    uint8_t hdr0[24];

    sharp_port_stack_paint();
    sharp_port_init();
    player_res.magic = 0x504C4100;
    sharp_init();
    player_res.sd_hz = SHARP_SD_FAST_HZ;

    if (!sharp_port_sd_present()) fail(-1, "NO CARD");
    sharp_sd_for_card();
    if (sharp_sd_init()) fail(-2, "CARD NOT READY");
    player_res.sd_sectors = sharp_sd_sectors();
    player_res.sdhc = (uint32_t)sharp_sd_is_sdhc();
    if (vopen()) fail(-3, "NO VIDEO.BIN");

    /* 512-byte header, read in pieces that fit the buffer; the first 24 bytes matter */
    {
        uint32_t got = 0;
        while (got < 512) {
            uint32_t n = 512 - got;
            uint8_t *dst = (got == 0) ? hdr0 : buf;
            if (got == 0) { if (vread(hdr0, 24)) fail(-4, "BAD HEADER"); got = 24; continue; }
            if (n > sizeof buf) n = sizeof buf;
            if (vread(dst, n)) fail(-4, "BAD HEADER");
            got += n;
        }
    }
    if (memcmp(hdr0, "SH128V1", 8) != 0 || hdr0[16] != 128 || hdr0[17] != 128) fail(-5, "NOT SH128V1");
    fps = (uint32_t)hdr0[8] | ((uint32_t)hdr0[9] << 8);
    frames = (uint32_t)hdr0[12] | ((uint32_t)hdr0[13] << 8) | ((uint32_t)hdr0[14] << 16) | ((uint32_t)hdr0[15] << 24);
    if (FPS_OVERRIDE) fps = FPS_OVERRIDE;
    if (!fps || !frames) fail(-6, "EMPTY FILE");
    player_res.fps_target = fps;
    player_res.frames_in_file = frames;
    player_res.magic = 0x504C4101;
    period_us_x16 = (1000000u * 16u) / fps;     /* frame period in 1/16 microsecond */

    start_ms = sharp_port_millis();
    t0 = sharp_port_micros();
    (void)t0;
    i = 0;
    for (pass = 0; pass < PLAY_LOOPS; pass++) {
        uint32_t f;
        if (pass) {                              /* second pass: reopen at the first frame */
            vclose();
            if (vopen()) fail(-3, "NO VIDEO.BIN");
            { uint32_t got = 0; while (got < 512) { uint32_t n = 512 - got; if (n > sizeof buf) n = sizeof buf; if (vread(buf, n)) fail(-4, "BAD HEADER"); got += n; } }
        }
        for (f = 0; f < frames; f++, i++) {
            /* pace: frame i is due at start + i * period */
            uint32_t due_us = (uint32_t)(((uint64_t)i * period_us_x16) >> 4);
            uint32_t now_us = (sharp_port_millis() - start_ms) * 1000u;
            uint32_t a, b;
            while ((uint32_t)(sharp_port_millis() - start_ms) * 1000u + 500u < due_us) sharp_vcom_tick(sharp_port_millis());
            (void)now_us;
            if ((uint32_t)(sharp_port_millis() - start_ms) * 1000u > due_us + 1000u * 2u) player_res.late++;
#ifdef SHARP_CHUNK_ROWS
            {
                uint8_t y;
                for (y = 0; y < SHARP_H; y = (uint8_t)(y + SHARP_CHUNK_ROWS)) {
                    a = sharp_port_micros();
                    sharp_sd_for_card();
                    if (vread(buf, CHUNK)) fail(-7, "READ ERROR");
                    sharp_sd_for_lcd();
                    b = sharp_port_micros();
                    sharp_update_lines(buf, y, SHARP_CHUNK_ROWS);
                    player_res.sd_us += b - a;
                    player_res.lcd_us += sharp_port_micros() - b;
                    player_res.bytes_read += CHUNK;
                }
            }
#else
            a = sharp_port_micros();
            sharp_sd_for_card();
            if (vread(fb, SHARP_FB_BYTES)) fail(-7, "READ ERROR");
            sharp_sd_for_lcd();
            b = sharp_port_micros();
            sharp_update_full(fb);
            player_res.sd_us += b - a;
            player_res.lcd_us += sharp_port_micros() - b;
            player_res.bytes_read += SHARP_FB_BYTES;
#endif
            player_res.frames_shown++;
            player_res.elapsed_ms = sharp_port_millis() - start_ms;
        }
    }
    if (player_res.elapsed_ms) player_res.fps_x100 = (uint32_t)(((uint64_t)player_res.frames_shown * 100000u) / player_res.elapsed_ms);
    if (player_res.sd_us) player_res.sd_kBps_x10 = (uint32_t)(((uint64_t)player_res.bytes_read * 10000u) / player_res.sd_us);
    player_res.stack_free = sharp_port_stack_free();
    player_res.magic = 0x504C4102;               /* done */
    hold();
    return 0;
}
