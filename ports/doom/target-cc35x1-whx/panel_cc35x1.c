/* Panel driver of the LP-EM-CC35X1 build of rp2040-doom (target cc35x1-whx, PANEL=1). GPL-2.0, same terms as the engine it is linked with.
 * Written fresh for this image from the ST7796S data sheet command list and the Fox Red Labs record of the same Waveshare 3.5 inch panel
 * on the LP-MSPM0G3507 and LP-MSPM0G3519 (mspm0g3507/display-test/README.md); SPI and GPIO through the Wi-Fi SDK's TI Drivers.
 * No vendor example code. Wiring: PANEL-WIRING.md.
 *
 * Screen: 320 x 480 portrait. Doom's 320 pixels fit the width one to one, so there is no scaling: the 3D view (320 x 168) goes to rows
 * PAN_VIEW_Y.., a page (title, intermission: 320 x 200 over both frame buffers) to the same top row. Pixels leave as RGB565, high byte first.
 * One band of PAN_BAND rows is converted while the previous band is on the wire (SPI DMA, callback mode): two band buffers. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"

#ifndef PANEL_HZ
#define PANEL_HZ 16000000u       /* the panel's rated write clock is about 15 MHz; the same wiring ran 16 MHz on the G3519 */
#endif
#define PAN_W 320
#define PAN_H 480
#define PAN_VIEW_Y 140           /* 200 rows (a page) centred in 480; the 168-row view uses the same top row */
#define PAN_BAND 8
#define PAN_VIEW_H 168
#define PAN_PAGE_H 200

int fox_printf(const char *fmt, ...);
uint32_t hal_ms(void);

static SPI_Handle spi;
static volatile int spi_done;
static uint16_t pal[256];                                              /* RGB565, bytes swapped so memory order is high byte first */
static uint16_t band[2][PAN_BAND * PAN_W] __attribute__((aligned(4), section(".bss.fox_arena_d")));
volatile uint32_t pan_frames, pan_last_ms, pan_ok;

static void spi_cb(SPI_Handle h, SPI_Transaction *t) { (void)h; (void)t; spi_done = 1; }
static void spi_start(const void *p, size_t n)
{
    static SPI_Transaction tr;
    memset(&tr, 0, sizeof tr);
    tr.count = n; tr.txBuf = (void *)p; tr.rxBuf = NULL;
    spi_done = 0;
    if (!SPI_transfer(spi, &tr)) spi_done = 1;                         /* a refused transfer is not waited for */
}
static void spi_wait(void) { while (!spi_done) {} }
/* The module's level translator loses a chip select pulse of a few hundred ns between two commands: keep CS high at least 10 us (measured on
 * the G3507). About 3 cycles an iteration at 160 MHz, so 2000 iterations is far above 10 us. */
static void gap(void) { for (volatile int i = 0; i < 2000; i++) {} }
static void cmd(uint8_t c, const uint8_t *d, size_t n)
{
    GPIO_write(CONFIG_GPIO_PANEL_CS, 0);
    GPIO_write(CONFIG_GPIO_PANEL_DC, 0); spi_start(&c, 1); spi_wait();
    if (n) { GPIO_write(CONFIG_GPIO_PANEL_DC, 1); spi_start(d, n); spi_wait(); }
    GPIO_write(CONFIG_GPIO_PANEL_CS, 1);
    gap();
}
static void window(int x0, int y0, int x1, int y1)
{
    uint8_t a[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 };
    uint8_t b[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 };
    cmd(0x2A, a, 4); cmd(0x2B, b, 4);
}
static void stream_begin(int y0, int rows)
{
    window(0, y0, PAN_W - 1, y0 + rows - 1);
    GPIO_write(CONFIG_GPIO_PANEL_CS, 0);
    GPIO_write(CONFIG_GPIO_PANEL_DC, 0);
    { uint8_t c = 0x2C; spi_start(&c, 1); spi_wait(); }
    GPIO_write(CONFIG_GPIO_PANEL_DC, 1);
}
static void stream_end(void) { GPIO_write(CONFIG_GPIO_PANEL_CS, 1); gap(); }

static void clear(void)
{
    memset(band[0], 0, sizeof band[0]);
    stream_begin(0, PAN_H);
    for (int r = 0; r < PAN_H; r += PAN_BAND) { spi_start(band[0], sizeof band[0]); spi_wait(); }
    stream_end();
}

void panel_init(void)
{
    SPI_Params p;
    for (int i = 0; i < 256; i++) pal[i] = (uint16_t)(((i >> 3) << 11) | ((i >> 2) << 5) | (i >> 3));   /* grey ramp until the engine sets its palette */
    GPIO_write(CONFIG_GPIO_PANEL_CS, 1);
    SPI_init();
    SPI_Params_init(&p);
    p.mode = SPI_CONTROLLER; p.frameFormat = SPI_POL0_PHA0; p.bitRate = PANEL_HZ; p.dataSize = 8;
    p.transferMode = SPI_MODE_CALLBACK; p.transferCallbackFxn = spi_cb;
    spi = SPI_open(CONFIG_SPI_PANEL, &p);
    if (!spi) { fox_printf("PANEL SPI_open failed\n"); return; }
    GPIO_write(CONFIG_GPIO_PANEL_RST, 0); vTaskDelay(pdMS_TO_TICKS(10));
    GPIO_write(CONFIG_GPIO_PANEL_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));
    cmd(0x01, 0, 0); vTaskDelay(pdMS_TO_TICKS(120));                   /* software reset */
    cmd(0x11, 0, 0); vTaskDelay(pdMS_TO_TICKS(120));                   /* sleep out */
    { uint8_t d = 0x48; cmd(0x36, &d, 1); }                            /* portrait, BGR order */
    { uint8_t d = 0x55; cmd(0x3A, &d, 1); }                            /* 16 bits a pixel */
    cmd(0x21, 0, 0);                                                   /* inversion on: this IPS panel needs it */
    cmd(0x29, 0, 0); vTaskDelay(pdMS_TO_TICKS(20));                    /* display on */
    clear();
    GPIO_write(CONFIG_GPIO_PANEL_BL, 1);
    pan_ok = 1;
    fox_printf("PANEL up, %lu Hz SPI, 320x480\n", (unsigned long)PANEL_HZ);
}

void panel_palette(const uint8_t *rgb)                                 /* 256 entries of r, g, b */
{
    for (int i = 0; i < 256; i++, rgb += 3) {
        uint32_t v = ((uint32_t)(rgb[0] >> 3) << 11) | ((uint32_t)(rgb[1] >> 2) << 5) | (rgb[2] >> 3);
        pal[i] = (uint16_t)((v >> 8) | (v << 8));
    }
}

static void convert(const uint8_t *s, uint16_t *d, int n) { while (n--) *d++ = pal[*s++]; }
/* One source row: rows 0..167 are in f0, rows 168..199 of a page are rows 136.. of f1 (the page spreads over both buffers). */
static const uint8_t *src_row(const uint8_t *f0, const uint8_t *f1, int single, int y)
{
    return (!single || y < PAN_VIEW_H) ? f0 + (size_t)y * PAN_W : f1 + (size_t)(y - 32) * PAN_W;
}
void panel_present(const uint8_t *f0, const uint8_t *f1, int single)
{
    if (!pan_ok) return;
    int rows = single ? PAN_PAGE_H : PAN_VIEW_H, nb = rows / PAN_BAND;
    uint32_t t0 = hal_ms();
    stream_begin(PAN_VIEW_Y, rows);
    for (int r = 0; r < PAN_BAND; r++) convert(src_row(f0, f1, single, r), band[0] + r * PAN_W, PAN_W);
    for (int b = 0; b < nb; b++) {
        spi_start(band[b & 1], sizeof band[0]);                        /* band b on the wire (DMA) ... */
        if (b + 1 < nb)                                                /* ... while band b + 1 is converted */
            for (int r = 0; r < PAN_BAND; r++) convert(src_row(f0, f1, single, (b + 1) * PAN_BAND + r), band[(b + 1) & 1] + r * PAN_W, PAN_W);
        spi_wait();
    }
    stream_end();
    pan_frames++; pan_last_ms = hal_ms() - t0;
}
