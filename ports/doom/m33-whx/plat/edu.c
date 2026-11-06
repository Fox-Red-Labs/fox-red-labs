/* Educational BoosterPack MKII (BOOSTXL-EDUMKII) side of the m33-whx Doom image on the LP-MSPM33C321A. GPL-2.0 with the engine it is linked with.
 * Written fresh for this image on top of the BSD-3 core in portability/boosterpacks/edumkii (panel set-up, windows, text); the pin names come from
 * board/doom_board_edu.syscfg. No vendor example code.
 *
 * Screen: Doom's 320 x 200 picture goes to a 128 x 96 window (4:3, the shape Doom is drawn for) in the middle of the 128 x 128 panel.
 *   - A game frame (VIDEO_TYPE_DOUBLE: the 320 x 168 view, the status bar is not in the frame buffer) fills the top 80 rows of the window; the 16 rows
 *     below hold two lines of text (health, armor, ammo, mode, frames a second) that replace the status bar.
 *   - A page (title, intermission: VIDEO_TYPE_SINGLE and SAVING, the 200 rows spread over both frame buffers) fills all 96 rows.
 *   - No full RGB565 frame exists. Two 256-byte row buffers: the SPI DMA sends one row while the interrupt that its end raises converts the next
 *     (nearest rows, two-pixel average across, palette table) and starts it, so the engine renders the next frame while the previous one is on the wire.
 * Input: joystick X and Y through the HSADC sequencer, buttons S1 and S2 (low = pressed). The joystick press is not usable on this LaunchPad (PA21 is
 *   VREF- and R10 ties it to ground, portability/boosterpacks/edumkii/AGENTS.md).
 * Sound: none; the buzzer pin stays low (quiet hours). */
#include <stdint.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "edumkii.h"

#define WIN_X 0
#define WIN_Y 16            /* 128 x 96 window, centred in 128 rows */
#define WIN_W 128
#define WIN_H 96
#define VIEW_ROWS 80        /* the 3D view: 168 of 200 source rows at 96/200 */
#define SRC_W 320
#define SRC_VIEW_H 168

typedef struct {
    uint32_t magic;               /* 0xED0D0001 */
    uint32_t frames;              /* streams started */
    uint32_t joy_x, joy_y;        /* raw ADC counts of the last poll */
    uint32_t joy_x0, joy_y0;      /* rest point */
    uint32_t buttons;             /* bit 0 S1, bit 1 S2 (1 = pressed) of the last poll */
    uint32_t stream_cyc_last, stream_cyc_max;   /* start of a stream to its last row handed to the SPI */
    uint32_t stall_cyc_total;     /* cycles the engine waited for the previous stream */
    uint32_t conv_cyc_total;      /* cycles in row conversion (interrupt and start-up) */
    uint32_t hud_draws;
    uint32_t fps10;               /* frames a second x 10, real-time builds */
    uint32_t mode;                /* 0 demos, 1 play */
    uint32_t pal_changes;
    uint32_t starts_game;         /* game starts requested */
} edu_res_t;
volatile edu_res_t edu_res;

static edumkii_t dev;
static uint16_t pal[256];                                   /* RGB565 of the current Doom palette */
static uint16_t rowbuf[2][WIN_W] __attribute__((aligned(4)));

static inline uint32_t cyc(void) { return DWT->CYCCNT; }

/* ---- port: GPIO, blocking SPI, delays ---- */
typedef struct { GPIO_Regs *port; uint32_t pin; } pinref_t;
static const pinref_t pins[EDUMKII_PIN_COUNT] = {
    [EDUMKII_PIN_LCD_CS]    = { E_LCD_CS_PORT, E_LCD_CS_LCD_CS_PIN },
    [EDUMKII_PIN_LCD_DC]    = { E_LCD_DC_PORT, E_LCD_DC_LCD_DC_PIN },
    [EDUMKII_PIN_LCD_RST]   = { E_LCD_RST_PORT, E_LCD_RST_LCD_RST_PIN },
    [EDUMKII_PIN_BL_RED]    = { E_BL_RED_PORT, E_BL_RED_BL_RED_PIN },
    [EDUMKII_PIN_LED_GREEN] = { E_LED_GREEN_PORT, E_LED_GREEN_LED_GREEN_PIN },
    [EDUMKII_PIN_LED_BLUE]  = { E_LED_BLUE_PORT, E_LED_BLUE_LED_BLUE_PIN },
    [EDUMKII_PIN_BUZZER]    = { E_BUZZER_PORT, E_BUZZER_BUZZER_PIN },
    [EDUMKII_PIN_S1]        = { E_S1_PORT, E_S1_S1_PIN },
    [EDUMKII_PIN_S2]        = { E_S2_PORT, E_S2_S2_PIN },
};
static void p_delay_us(void *c, uint32_t us) { (void)c; uint32_t t0 = cyc(), n = us * (CPUCLK_FREQ / 1000000u); while ((uint32_t)(cyc() - t0) < n) {} }
static void p_delay_ms(void *c, uint32_t ms) { while (ms--) p_delay_us(c, 1000u); }
static void p_gpio_write(void *c, edumkii_pin_t p, int level)
{
    (void)c;
    if (!pins[p].port) return;
    if (level) DL_GPIO_setPins(pins[p].port, pins[p].pin); else DL_GPIO_clearPins(pins[p].port, pins[p].pin);
}
static int p_gpio_read(void *c, edumkii_pin_t p) { (void)c; return pins[p].port ? (DL_GPIO_readPins(pins[p].port, pins[p].pin) ? 1 : 0) : 1; }
static void p_spi_write(void *c, const uint8_t *d, uint32_t n)
{
    (void)c;
    for (uint32_t i = 0; i < n; i++) DL_SPI_transmitDataBlocking8(SPI_LCD_INST, d[i]);
    while (DL_SPI_isBusy(SPI_LCD_INST)) {}
}

/* ---- rows ---- */
static struct {
    const uint8_t *f0, *f1;          /* the frame buffer shown, and the other one (page rows 168 and up come from it) */
    uint32_t nrows, type_single;
    volatile uint32_t sent, done;
    uint32_t t_start;
    volatile uint32_t t_done;
} st;

static const uint8_t *src_row(uint32_t j)
{
    if (!st.type_single) {                                   /* 80 output rows over the 168-row view */
        uint32_t y = (j * 336u + 168u) / 160u;
        return st.f0 + y * SRC_W;
    } else {                                                 /* 96 output rows over 200 rows, 168 from f0 then f1 rows 136 and up */
        uint32_t y = (j * 400u + 200u) / 192u;
        return y < SRC_VIEW_H ? st.f0 + y * SRC_W : st.f1 + (y - 32u) * SRC_W;
    }
}
static void convert_row(uint32_t j, uint16_t *dst)
{
    uint32_t t0 = cyc();
    const uint8_t *s = src_row(j);
    uint32_t x = 0;                                          /* 5 * i / 2: 2.5 source pixels an output pixel */
    for (uint32_t i = 0; i < WIN_W; i++, x += 5u) {
        const uint8_t *p = s + (x >> 1);
        uint32_t a = pal[p[0]], b = pal[p[1]];
        uint32_t m = ((a >> 1) & 0x7BEFu) + ((b >> 1) & 0x7BEFu);
        dst[i] = (uint16_t)__REV16(m);                       /* the panel takes the high byte first */
    }
    edu_res.conv_cyc_total += cyc() - t0;
}
static void start_dma(uint32_t j)
{
    DL_DMA_setSrcAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)rowbuf[j & 1u]);
    DL_DMA_setDestAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)(&SPI_LCD_INST->spi->TXDATA));
    DL_DMA_setTransferSize(DMA0, DMA0_CH1_CHAN_ID, WIN_W * 2u);
    DL_DMA_enableChannel(DMA0, DMA0_CH1_CHAN_ID);
}
void SPI_LCD_INST_IRQHandler(void)
{
    if (DL_SPI_getPendingInterrupt(SPI_LCD_INST) != DL_SPI_IIDX_DMA_DONE_TX) return;
    uint32_t j = ++st.sent;                                  /* row j - 1 is in the SPI FIFO; row j was converted before */
    if (j >= st.nrows) { st.t_done = cyc(); st.done = 1; return; }
    start_dma(j);
    if (j + 1u < st.nrows) convert_row(j + 1u, rowbuf[(j + 1u) & 1u]);
}

/* Wait until the stream in flight is on the wire and release the panel. */
static void wait_stream(void)
{
    if (!st.nrows) return;
    while (!st.done) {}
    while (DL_SPI_isBusy(SPI_LCD_INST)) {}
    p_gpio_write(0, EDUMKII_PIN_LCD_CS, 1);
    uint32_t c = st.t_done - st.t_start;       /* first row started to the last row in the SPI FIFO (the wire time is a few bytes longer) */
    edu_res.stream_cyc_last = c; if (c > edu_res.stream_cyc_max) edu_res.stream_cyc_max = c;
    st.nrows = 0;
}

/* ---- palette: Doom's 768 bytes of RGB to RGB565 ---- */
void edu_set_palette(const uint8_t *rgb)       /* 256 entries of r, g, b, already copied to RAM by the caller */
{
    for (int i = 0; i < 256; i++, rgb += 3) pal[i] = EDUMKII_RGB(rgb[0], rgb[1], rgb[2]);
    edu_res.pal_changes++;
}

/* ---- HUD: two lines of text in the 16 rows under the view ---- */
static char hud_old[2][24];
static int hud_dirty = 1;
static void draw_hud(const char *l1, const char *l2)
{
    const char *l[2] = { l1, l2 };
    for (int k = 0; k < 2; k++) {
        if (!hud_dirty && l[k] && !strncmp(hud_old[k], l[k], sizeof hud_old[k])) continue;
        char pad[23]; int n = 0;
        while (n < 21 && l[k] && l[k][n]) { pad[n] = l[k][n]; n++; }
        while (n < 21) pad[n++] = ' ';
        pad[21] = 0;
        edumkii_text(&dev, WIN_X, WIN_Y + VIEW_ROWS + 8 * k, pad, EDUMKII_WHITE, EDUMKII_BLACK, 1);
        strncpy(hud_old[k], l[k] ? l[k] : "", sizeof hud_old[k] - 1); hud_old[k][sizeof hud_old[k] - 1] = 0;
        edu_res.hud_draws++;
    }
    hud_dirty = 0;
}

/* Frames a second (x 10) over the last second of the CPU's own cycle counter, whatever clock the engine runs on. */
static uint32_t fps_t0, fps_n;
uint32_t edu_fps10(void) { return edu_res.fps10; }
uint32_t edu_script_ms(void) { static uint32_t last, ms_rem, ms; uint32_t c = cyc(), d = c - last; last = c; d += ms_rem; ms += d / (CPUCLK_FREQ / 1000u); ms_rem = d % (CPUCLK_FREQ / 1000u); return ms; }

/* Show a frame. fb0 is the frame buffer holding the view, fb1 the other one. single = a full page (96 rows), else the view (80 rows) and the HUD.
 * Returns when the stream is running (view) or finished (page); the caller may then free the buffer that was shown before. */
void edu_present(const uint8_t *fb0, const uint8_t *fb1, int single, const char *l1, const char *l2)
{
    uint32_t w0 = cyc();
    wait_stream();
    edu_res.stall_cyc_total += cyc() - w0;
    if (!single) draw_hud(l1, l2); else hud_dirty = 1;
    st.f0 = fb0; st.f1 = fb1; st.type_single = (uint32_t)single;
    st.nrows = single ? WIN_H : VIEW_ROWS; st.sent = 0; st.done = 0;
    st.t_start = cyc();
    edumkii_stream_begin(&dev, WIN_X, WIN_Y, WIN_W, (int)st.nrows);
    DL_SPI_flushTXFIFO(SPI_LCD_INST);
    convert_row(0, rowbuf[0]);
    if (st.nrows > 1) convert_row(1, rowbuf[1]);
    start_dma(0);
    edu_res.frames++;
    if (++fps_n >= 8u) {            /* every 8 frames: a stable figure without waiting a second at 1 frame a second */
        uint32_t t = cyc(); uint32_t d = t - fps_t0;
        if (fps_t0 && d) edu_res.fps10 = (uint32_t)(((uint64_t)fps_n * 10u * CPUCLK_FREQ) / d);
        fps_t0 = t; fps_n = 0;
    }
    if (single) wait_stream();
}

/* ---- inputs ---- */
void edu_inputs(int *dx, int *dy, int *s1, int *s2)
{
    DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
    p_delay_us(0, 14);                                       /* seven conversions after the force */
    uint32_t x = DL_HSADC_getResult(HSADC1_RESULT_INST, (DL_HSADC_SOC_NUMBER)3);
    uint32_t y = DL_HSADC_getResult(HSADC1_RESULT_INST, (DL_HSADC_SOC_NUMBER)6);
    edu_res.joy_x = x; edu_res.joy_y = y;
    *dx = (int)x - (int)edu_res.joy_x0; *dy = (int)y - (int)edu_res.joy_y0;
    *s1 = !p_gpio_read(0, EDUMKII_PIN_S1); *s2 = !p_gpio_read(0, EDUMKII_PIN_S2);
    edu_res.buttons = (uint32_t)(*s1 | (*s2 << 1));
}

static const edumkii_port_t port = {
    0, p_gpio_write, p_gpio_read, p_spi_write, 0, 0, 0, 0, 0, p_delay_us, p_delay_ms, 0, 0
};

void edu_text(int x, int y, const char *s) { edumkii_text(&dev, x, y, s, EDUMKII_WHITE, EDUMKII_BLACK, 1); }

#ifdef EDU_TEST_MS
/* Colour check: four bars (red, green, blue, white as the driver's constants name them), held for EDU_TEST_MS. */
static void test_pattern(void)
{
    edumkii_rect(&dev, 0, 0, 32, 128, EDUMKII_RED); edumkii_rect(&dev, 32, 0, 32, 128, EDUMKII_GREEN);
    edumkii_rect(&dev, 64, 0, 32, 128, EDUMKII_BLUE); edumkii_rect(&dev, 96, 0, 32, 64, EDUMKII_WHITE);
    edumkii_text(&dev, 2, 2, "RED", EDUMKII_BLACK, EDUMKII_RED, 1); edumkii_text(&dev, 34, 2, "GRN", EDUMKII_BLACK, EDUMKII_GREEN, 1);
    edumkii_text(&dev, 66, 2, "BLU", EDUMKII_WHITE, EDUMKII_BLUE, 1); edumkii_text(&dev, 98, 2, "WHT", EDUMKII_BLACK, EDUMKII_WHITE, 1);
    p_delay_ms(0, EDU_TEST_MS);
    edumkii_fill(&dev, EDUMKII_BLACK);
}
#endif
void edu_init(void)
{
    edu_res.magic = 0xED0D0001u;
    edumkii_init(&dev, &port);
    p_gpio_write(0, EDUMKII_PIN_BUZZER, 0);
    edumkii_lcd_init(&dev);
    edumkii_backlight(&dev, 1);
#ifdef EDU_TEST_MS
    test_pattern();
#endif
    NVIC_EnableIRQ(SPI_LCD_INST_INT_IRQN);
    uint32_t sx = 0, sy = 0;                                 /* rest point: average of 16 reads */
    edu_res.joy_x0 = 0; edu_res.joy_y0 = 0;
    for (int i = 0; i < 16; i++) { int dx, dy, a, b; edu_inputs(&dx, &dy, &a, &b); sx += edu_res.joy_x; sy += edu_res.joy_y; p_delay_us(0, 200); }
    edu_res.joy_x0 = sx / 16u; edu_res.joy_y0 = sy / 16u;
    for (int i = 0; i < 256; i++) pal[i] = EDUMKII_RGB(i, i, i);   /* grey ramp until the engine sets its palette */
}
