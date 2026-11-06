/*
 * Educational BoosterPack MKII (BOOSTXL-EDUMKII) portable core.
 *
 * Plain C99, no vendor includes. The board's parts sit behind the small port
 * interface below: a host fills in the functions it has and leaves the rest NULL.
 * Written from the data sheets and the user guide (SLAU599B); no vendor driver
 * code is used. Panel: Crystalfontz CFAF128128B-0145T (ST7735S, 128 x 128, RGB565).
 *
 * Licence: BSD-3-Clause, as the rest of the repository.
 */
#ifndef EDUMKII_H
#define EDUMKII_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EDUMKII_LCD_W 128
#define EDUMKII_LCD_H 128

/* RGB565 colors (red in the top five bits). */
#define EDUMKII_RGB(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))
#define EDUMKII_BLACK   0x0000u
#define EDUMKII_WHITE   0xFFFFu
#define EDUMKII_RED     0xF800u
#define EDUMKII_GREEN   0x07E0u
#define EDUMKII_BLUE    0x001Fu
#define EDUMKII_YELLOW  0xFFE0u
#define EDUMKII_CYAN    0x07FFu
#define EDUMKII_MAGENTA 0xF81Fu
#define EDUMKII_GREY    0x8410u

/* Panel RAM offset and scan direction of the CFAF128128B-0145T. */
#ifndef EDUMKII_COL_OFFSET
#define EDUMKII_COL_OFFSET 2
#endif
#ifndef EDUMKII_ROW_OFFSET
#define EDUMKII_ROW_OFFSET 3
#endif
#ifndef EDUMKII_MADCTL
#define EDUMKII_MADCTL 0xC8 /* MY | MX | BGR */
#endif

/* Digital pins. BL_RED is BoosterPack pin 39: the screen backlight with jumper J5
 * on 2-3, the red LED with J5 on 1-2 (the shipped position). */
typedef enum {
    EDUMKII_PIN_LCD_CS,   /* pin 13, output, low = selected */
    EDUMKII_PIN_LCD_DC,   /* pin 31, output, low = command  */
    EDUMKII_PIN_LCD_RST,  /* pin 17, output, low = reset    */
    EDUMKII_PIN_BL_RED,   /* pin 39, output                 */
    EDUMKII_PIN_LED_GREEN,/* pin 38, output                 */
    EDUMKII_PIN_LED_BLUE, /* pin 37, output                 */
    EDUMKII_PIN_BUZZER,   /* pin 40, output                 */
    EDUMKII_PIN_S1,       /* pin 33, input, low = pressed   */
    EDUMKII_PIN_S2,       /* pin 32, input, low = pressed   */
    EDUMKII_PIN_JOY_SEL,  /* pin 5,  input, low = pressed   */
    EDUMKII_PIN_COUNT
} edumkii_pin_t;

/* Analog channels, 12-bit results (0..4095, 3.3 V full scale). */
typedef enum {
    EDUMKII_ADC_JOY_X,    /* pin 2  */
    EDUMKII_ADC_JOY_Y,    /* pin 26 */
    EDUMKII_ADC_MIC,      /* pin 6  */
    EDUMKII_ADC_ACC_X,    /* pin 23 */
    EDUMKII_ADC_ACC_Y,    /* pin 24 */
    EDUMKII_ADC_ACC_Z,    /* pin 25 */
    EDUMKII_ADC_COUNT
} edumkii_adc_t;

/* Return codes. */
#define EDUMKII_OK        0
#define EDUMKII_ERR_PORT (-1)  /* the host did not provide the function this needs */
#define EDUMKII_ERR_BUS  (-2)  /* I2C did not acknowledge */

/*
 * The port. All functions take the ctx pointer first. Required: delay_us, delay_ms.
 * Everything else is optional: a missing function makes the matching calls return
 * EDUMKII_ERR_PORT (or do nothing for void calls).
 *
 * spi_write: SPI mode 0, MSB first, 8-bit frames, blocking: return when every byte is
 *            on the wire. The core drives chip select and data/command itself.
 * spi_write_bulk: optional, same contract, called for blocks of 64 bytes or more
 *            (a host puts DMA behind it).
 * i2c_write / i2c_read: 7-bit address, return 0 on success. i2c_write sends a STOP.
 * pwm_set: square wave on the buzzer pin at freq_hz with duty_pct; freq_hz 0 stops.
 */
typedef struct edumkii_port {
    void *ctx;
    void     (*gpio_write)(void *ctx, edumkii_pin_t pin, int level);
    int      (*gpio_read)(void *ctx, edumkii_pin_t pin);
    void     (*spi_write)(void *ctx, const uint8_t *data, uint32_t len);
    void     (*spi_write_bulk)(void *ctx, const uint8_t *data, uint32_t len);
    uint16_t (*adc_read)(void *ctx, edumkii_adc_t ch);
    void     (*pwm_set)(void *ctx, uint32_t freq_hz, uint8_t duty_pct);
    int      (*i2c_write)(void *ctx, uint8_t addr7, const uint8_t *data, uint32_t len);
    int      (*i2c_read)(void *ctx, uint8_t addr7, uint8_t *data, uint32_t len);
    void     (*delay_us)(void *ctx, uint32_t us);
    void     (*delay_ms)(void *ctx, uint32_t ms);
    /* Optional, for streaming a frame while the CPU does other work: spi_write_async starts a DMA send of len bytes
     * and returns at once (it first waits for the previous one); the bytes must stay valid until the next
     * spi_write_async or spi_wait returns. spi_wait returns when the last one is on the wire. */
    void     (*spi_write_async)(void *ctx, const uint8_t *data, uint32_t len);
    void     (*spi_wait)(void *ctx);
} edumkii_port_t;

#ifndef EDUMKII_NO_LCD
#define EDUMKII_LINE_BYTES 512
#else
#define EDUMKII_LINE_BYTES 0
#endif

typedef struct edumkii {
    edumkii_port_t port;
    uint16_t joy_x0, joy_y0;                 /* rest values from edumkii_joy_calibrate */
#if EDUMKII_LINE_BYTES
    uint8_t line[EDUMKII_LINE_BYTES];        /* scratch for fills and glyphs */
#endif
} edumkii_t;

/* Copy the port in and set the output pins to their idle levels. */
int edumkii_init(edumkii_t *d, const edumkii_port_t *port);

/* ---- screen (needs gpio_write, spi_write; build without it using EDUMKII_NO_LCD) ---- */
#ifndef EDUMKII_NO_LCD
int  edumkii_lcd_init(edumkii_t *d);                 /* reset, init sequence, display on, black */
void edumkii_backlight(edumkii_t *d, int on);        /* pin 39: J5 must be on 2-3 */
void edumkii_fill(edumkii_t *d, uint16_t color);
void edumkii_rect(edumkii_t *d, int x, int y, int w, int h, uint16_t color);   /* filled */
void edumkii_frame(edumkii_t *d, int x, int y, int w, int h, uint16_t color);  /* outline */
void edumkii_pixel(edumkii_t *d, int x, int y, uint16_t color);
void edumkii_line(edumkii_t *d, int x0, int y0, int x1, int y1, uint16_t color);
/* Pixels in host order, row by row, w * h entries. Converted to panel byte order in chunks. */
void edumkii_blit(edumkii_t *d, int x, int y, int w, int h, const uint16_t *px);
/* Bytes already in panel order (high byte first), w * h * 2 of them. Uses spi_write_bulk. */
void edumkii_blit_raw(edumkii_t *d, int x, int y, int w, int h, const uint8_t *be);
/* Streaming: open a window (chip select low, panel in write mode), push rows of bytes already in panel order with
 * edumkii_stream_write (asynchronous when the port has spi_write_async, so the caller can fill the next row while
 * this one is on the wire), and close with edumkii_stream_end, which waits for the last row. Nothing else may use
 * the panel between begin and end. */
void edumkii_stream_begin(edumkii_t *d, int x, int y, int w, int h);
void edumkii_stream_write(edumkii_t *d, const uint8_t *be, uint32_t len);
void edumkii_stream_end(edumkii_t *d);
/* Text, 5 x 7 font in a 6 x 8 cell, scale 1 or 2. Returns the x after the last cell. */
int  edumkii_char(edumkii_t *d, int x, int y, char c, uint16_t fg, uint16_t bg, int scale);
int  edumkii_text(edumkii_t *d, int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
/* Decimal text of a signed value, no printf. */
int  edumkii_number(edumkii_t *d, int x, int y, int32_t v, int min_width,
                    uint16_t fg, uint16_t bg, int scale);
#endif

/* 5 x 7 font: five column bytes per glyph, bit 0 is the top row. ASCII 32..126. */
const uint8_t *edumkii_font5x7(char c);

/* ---- inputs ---- */
uint16_t edumkii_adc(edumkii_t *d, edumkii_adc_t ch);          /* 0 if no adc_read */
int  edumkii_button(edumkii_t *d, edumkii_pin_t pin);          /* 1 = pressed (S1, S2, JOY_SEL) */
void edumkii_joy_calibrate(edumkii_t *d);                      /* average 16 reads as the rest point */
int  edumkii_joy_read(edumkii_t *d, int *dx, int *dy);         /* signed counts from rest */
#define EDUMKII_DIR_LEFT  1
#define EDUMKII_DIR_RIGHT 2
#define EDUMKII_DIR_UP    4
#define EDUMKII_DIR_DOWN  8
/* Direction bits for a stick pushed beyond +/- thresh counts. Right is the high X reading
 * and down the high Y reading until a bench run says otherwise (README, "Open"). */
int  edumkii_joy_dir(edumkii_t *d, int thresh);
void edumkii_accel_read(edumkii_t *d, uint16_t *x, uint16_t *y, uint16_t *z);
/* Peak-to-peak counts over n samples, spaced 100 us: a microphone level. */
uint16_t edumkii_mic_level(edumkii_t *d, int n);

/* ---- outputs ---- */
void edumkii_led(edumkii_t *d, int r, int g, int b);           /* on/off per color; red is pin 39 */
/* Blocking tone on the buzzer: PWM when pwm_set exists, else a software square wave. */
void edumkii_tone(edumkii_t *d, uint32_t freq_hz, uint32_t ms);

/* ---- OPT3001 light sensor (I2C 0x44) ---- */
#define EDUMKII_OPT3001_ADDR 0x44
int edumkii_lux_init(edumkii_t *d);                            /* checks IDs 0x5449 / 0x3001, starts continuous */
/* Result in hundredths of a lux (centilux). Returns EDUMKII_OK or an error code. */
int edumkii_lux_read(edumkii_t *d, uint32_t *centilux);

#ifdef __cplusplus
}
#endif
#endif
