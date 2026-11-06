/*
 * Abstract display, input, audio and clock interface (Fox Red Labs, BSD-3-Clause).
 *
 * One small set of structs that every front end in ports/ talks to (the Arduboy2
 * layer, the Peanut-GB front end). A board driver (EDUMKII ST7735S, Sharp
 * LS013B7DH03, or the headless host backend) fills these in and hands them to
 * frl_set_platform(). Nothing here knows about a chip.
 *
 * The screen is 128 x 128. The layer streams it out a few lines at a time, so no
 * driver needs a full frame buffer (a 128 x 128 RGB565 frame is 32 KB, the whole
 * RAM of a MSPM0G3507).
 */
#ifndef FRL_ABSTRACT_H
#define FRL_ABSTRACT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRL_SCREEN_W 128
#define FRL_SCREEN_H 128

typedef enum {
    FRL_FMT_RGB565 = 0, /* uint16_t per pixel, native endian, row after row            */
    FRL_FMT_1BPP   = 1  /* 16 bytes per line, leftmost pixel in bit 0 (Sharp order)    */
} frl_fmt_t;

/* How a smaller picture is placed on the 128 x 128 screen. */
typedef enum {
    FRL_FIT_CENTER = 0, /* 1:1, centred, black bars (Arduboy 128 x 64: rows 32 to 95)  */
    FRL_FIT_2X_V   = 1  /* every source row doubled vertically (Arduboy 128 x 64)      */
} frl_fit_t;

typedef struct frl_display {
    void *ctx;
    frl_fmt_t fmt;
    /* Colours for RGB565: foreground (lit pixel) and background. Ignored for 1BPP. */
    uint16_t fg, bg;
    /* 1BPP only: 1 means invert, so a lit pixel is a 0 bit (Sharp: 1 is white). */
    uint8_t invert;
    /* Called with n consecutive lines starting at line y0 (0 to 127). pix points at
     * n * 128 pixels (RGB565) or n * 16 bytes (1BPP). The pointer is only valid
     * during the call. */
    void (*write_lines)(void *ctx, uint16_t y0, uint16_t n, const void *pix);
    /* Called after the last line of a frame. May be NULL. */
    void (*flush)(void *ctx);
    /* Optional streaming path for RGB565 (a front end that scales as it renders uses it when all three are set):
     * stream_begin opens a 128 x 128 window at 0,0; stream_row queues one row of 128 pixels (256 bytes) and may
     * return before the row is on the wire; the row stays valid until the next call but one (two buffers rotate);
     * stream_end waits for the last row and closes the window. stream_be16 = 1 asks for pixels already byte-swapped
     * for the wire (high byte first in memory) so the driver sends them as they are. */
    void (*stream_begin)(void *ctx);
    void (*stream_row)(void *ctx, const void *row, uint16_t bytes);
    void (*stream_end)(void *ctx);
    uint8_t stream_be16;
} frl_display_t;

/* Raw input: a two-axis joystick (12-bit counts, 0 to 4095) and two push buttons. */
#define FRL_BTN_1 0x01u /* Arduboy A */
#define FRL_BTN_2 0x02u /* Arduboy B */

typedef struct frl_input_raw {
    uint16_t joy_x, joy_y; /* counts; x higher is right, y higher is up (see invert flags) */
    uint8_t buttons;       /* FRL_BTN_1 | FRL_BTN_2, 1 = pressed             */
} frl_input_raw_t;

typedef struct frl_input {
    void *ctx;
    void (*read)(void *ctx, frl_input_raw_t *out);
    /* Stick geometry. centre_x/centre_y of 0 means "learn it from the first
     * samples taken at start-up" (the EDUMKII stick rests off centre). */
    uint16_t centre_x, centre_y;
    uint8_t invert_x, invert_y; /* 1 flips the axis (right must read higher) */
} frl_input_t;

typedef struct frl_audio {
    void *ctx;
    /* Square wave of hz for ms milliseconds, then silence. ms == 0 means "until
     * the next call"; hz == 0 means silence now. May be NULL (no sound). */
    void (*tone)(void *ctx, uint16_t hz, uint16_t ms);
} frl_audio_t;

typedef struct frl_platform {
    frl_display_t display;
    frl_input_t input;
    frl_audio_t audio;
    void *clock_ctx;
    uint32_t (*millis)(void *clock_ctx);
    /* Busy-wait or sleep for ms. On the host this advances the virtual clock. */
    void (*delay_ms)(void *clock_ctx, uint32_t ms);
    /* Any noisy 32-bit value (ADC noise, a timer). May be NULL: millis is used. */
    uint32_t (*entropy)(void *clock_ctx);
} frl_platform_t;

/* Choose how a 128 x 64 picture sits on the screen (default FRL_FIT_CENTER). */
void frl_set_fit(frl_fit_t fit);
frl_fit_t frl_get_fit(void);

/* Register the board's platform. The structs are copied. */
void frl_set_platform(const frl_platform_t *p);
const frl_platform_t *frl_platform(void);

/* Push a packed 1-bit picture to the display.
 *   src      : page format (Arduboy): byte i of page r holds the 8 pixels
 *              x = i, y = 8*r .. 8*r+7, least significant bit on top.
 *   src_w/h  : 128 and a multiple of 8 up to 128 (64 for Arduboy).
 * Streams through a small line buffer and calls display.flush(). */
void frl_present_pages(const uint8_t *src, uint16_t src_w, uint16_t src_h, frl_fit_t fit);

/* Push a picture of 2-bit shades packed four pixels to a byte (first pixel in bits 1 to 0, row
 * after row, no padding), shade 0 lightest, 3 darkest, as the Game Boy front end keeps it
 * (160 x 144 is 5,760 bytes). src_w/h up to 160 x 144 is scaled to 128 x 128 by averaging
 * the source pixels each output pixel covers; 1BPP panels get a 4 x 4 ordered dither. Needs
 * no buffer beyond one output line. Used by Peanut-GB. */
void frl_present_shades2(const uint8_t *src, uint16_t src_w, uint16_t src_h);

/* Joystick plus two buttons to six Arduboy-style button bits (see Arduboy2.h):
 * LEFT 0x20, RIGHT 0x40, UP 0x80, DOWN 0x10, A 0x08, B 0x04. The stick is
 * calibrated on the first call and uses a dead band with hysteresis. */
uint8_t frl_read_buttons(void);

#ifdef __cplusplus
}
#endif
#endif
