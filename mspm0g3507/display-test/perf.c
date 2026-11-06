/* Display performance on the LP-MSPM0G3507 + Waveshare 3.5in (ST7796S), 32 MHz CPU.
 *
 * For SPI at 8 MHz and 16 MHz, full-screen (320x480, 307,200 bytes) redraw time by method:
 *   cpu8   CPU writes bytes to the SPI FIFO (the first demo's method)
 *   cpu16  CPU writes 16-bit frames (one FIFO write per pixel)
 *   dma8   DMA feeds bytes from a one-row buffer, CPU free
 *   dma16  DMA feeds 16-bit frames, CPU free
 * plus partial-update times, how much CPU time DMA leaves free, start-up time, and a
 * read-back of pixels from the screen's own memory to prove data arrived intact.
 * Results in `res`, read over the debugger; decode with perf.py. */
#include "ti_msp_dl_config.h"

#define W 320
#ifndef READ_SCR
#define READ_SCR 3          /* reads must stay under 6.7 MHz: 32 MHz / 8 = 4 MHz */
#endif
#define H 480

struct method { uint32_t full_us, box100_us, box32_us; };
struct {
    uint32_t magic;
    uint32_t boot_init_us;        /* main() to screen initialized */
    uint32_t boot_first_frame_us; /* main() to first full frame on screen */
    struct method m[2][4];        /* [8 MHz, 16 MHz][cpu8, cpu16, dma8, dma16] */
    uint32_t dma_free_pct[2][2];  /* CPU loop throughput left during dma8, dma16 */
    uint8_t  rb[2][1 + 3 * 64];
    uint32_t loop_cyc_x1000;
    uint8_t  pwr_mode;            /* register 0x0A after init: 0x9C = booster on, awake, normal, display on */      /* cycles per 1000 passes of the DMA wait loop */   /* raw RAMRD bytes after a dma16 frame, per speed */
} res;

static uint16_t rowbuf[W];        /* one row of pixels for DMA */

/* ---------- timing ---------- */
static volatile uint32_t ticks;
void SysTick_Handler(void) { ticks++; }
static uint32_t cyc(void)
{
    uint32_t t1, v;
    do { t1 = ticks; v = SysTick->VAL; } while (t1 != ticks);
    return (t1 << 24) + (0xFFFFFF - v);
}
static uint32_t us_since(uint32_t c0) { return (cyc() - c0) / (CPUCLK_FREQ / 1000000); }
static void delay_ms(uint32_t ms) { while (ms--) delay_cycles(CPUCLK_FREQ / 1000); }

/* ---------- SPI and screen ---------- */
#define SPI LCD_SPI_INST
static void cs_gap(void) { delay_cycles(CPUCLK_FREQ / 100000); }
static void spi_wait(void)
{
    while (!DL_SPI_isTXFIFOEmpty(SPI)) ;
    delay_cycles(32);
    while (DL_SPI_isBusy(SPI)) ;
}
static inline void tx8(uint8_t b) { while (DL_SPI_isTXFIFOFull(SPI)) ; DL_SPI_transmitData8(SPI, b); }
static inline void tx16(uint16_t v) { while (DL_SPI_isTXFIFOFull(SPI)) ; DL_SPI_transmitData16(SPI, v); }
static void cs_low(void) { DL_GPIO_clearPins(PB_PORT, PB_LCD_CS_PIN); }
static void cs_high(void) { DL_GPIO_setPins(PB_PORT, PB_LCD_CS_PIN); cs_gap(); }
static void dc(int data) { if (data) DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN); else DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN); }

static void lcd_cmd(uint8_t c, const uint8_t *d, int n)
{
    cs_low(); dc(0); tx8(c); spi_wait(); dc(1);
    for (int i = 0; i < n; i++) tx8(d[i]);
    spi_wait(); cs_high();
}
#define CMD(c, ...) do { const uint8_t _d[] = {0, ##__VA_ARGS__}; lcd_cmd(c, _d + 1, sizeof(_d) - 1); } while (0)
static void lcd_init(void)
{
#ifdef FAST_BOOT
    /* ST7796S: reset pulse >= 10 us, 5 ms before commands after reset, 5 ms after sleep out
       before the next command (the 120 ms rules only guard sleep in/out cycling). */
    DL_GPIO_clearPins(PB_PORT, PB_LCD_RST_PIN); delay_cycles(CPUCLK_FREQ / 10000);
    DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN);   delay_ms(6);
    CMD(0x11); delay_ms(6);
#else
    DL_GPIO_clearPins(PB_PORT, PB_LCD_RST_PIN); delay_ms(10);
    DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN);   delay_ms(120);
    CMD(0x01); delay_ms(120);
    CMD(0x11); delay_ms(120);
#endif
    CMD(0x36, 0x48); CMD(0x3A, 0x55); CMD(0x21);
    CMD(0x29);
}
static void begin(int x0, int y0, int x1, int y1)
{
    CMD(0x2A, x0 >> 8, x0, x1 >> 8, x1);
    CMD(0x2B, y0 >> 8, y0, y1 >> 8, y1);
    cs_low(); dc(0); tx8(0x2C); spi_wait(); dc(1);
}
/* The SPI block ignores a frame-size change while enabled: disable, change, enable. */
static void frames(int bits16)
{
    DL_SPI_disable(SPI);
    DL_SPI_setDataSize(SPI, bits16 ? DL_SPI_DATA_SIZE_16 : DL_SPI_DATA_SIZE_8);
    DL_SPI_enable(SPI);
}
static void end(void) { spi_wait(); frames(0); cs_high(); }

/* ---------- DMA ---------- */
static void dma_setup(int half)
{
    DL_DMA_Config c = {
        .transferMode = DL_DMA_SINGLE_TRANSFER_MODE, .extendedMode = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_UNCHANGED, .srcIncrement = DL_DMA_ADDR_INCREMENT,
        .destWidth = half ? DL_DMA_WIDTH_HALF_WORD : DL_DMA_WIDTH_BYTE,
        .srcWidth = half ? DL_DMA_WIDTH_HALF_WORD : DL_DMA_WIDTH_BYTE,
        .trigger = DMA_SPI1_TX_TRIG, .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL,
    };
    DL_DMA_initChannel(DMA, 0, &c);
    DL_DMA_setDestAddr(DMA, 0, (uint32_t)&SPI->TXDATA);
    DL_SPI_enableDMATransmitEvent(SPI);
}
static volatile uint32_t idle_iters;
static void dma_row(int half)   /* send rowbuf once, counting free loop passes meanwhile */
{
    DL_DMA_setSrcAddr(DMA, 0, (uint32_t)rowbuf);
    DL_DMA_setTransferSize(DMA, 0, half ? W : 2 * W);
    DL_DMA_enableChannel(DMA, 0);
    while (DL_DMA_isChannelEnabled(DMA, 0)) idle_iters++;
}

/* ---------- one method, one region ---------- */
static uint16_t color;
static void fill_row_buffer(int half)
{
    for (int i = 0; i < W; i++) {
        uint16_t c = color ^ (uint16_t)(i * 0x0841);   /* a pattern, so the read-back means something */
        rowbuf[i] = half ? c : (uint16_t)((c >> 8) | (c << 8));   /* bytes in wire order for dma8 */
    }
}
static uint32_t draw(int method, int x, int y, int w, int h)
{
    uint32_t c0 = cyc();
    begin(x, y, x + w - 1, y + h - 1);
    uint32_t n = (uint32_t)w * h;
    switch (method) {
    case 0: while (n--) { tx8(color >> 8); tx8(color); } break;
    case 1: frames(1); while (n--) tx16(color); break;
    case 2: case 3: {
        int half = method == 3;
        if (half) frames(1);
        for (int r = 0; r < h; r++) {
            if (w == W) dma_row(half);
            else {   /* partial rows: DMA the first w pixels */
                DL_DMA_setSrcAddr(DMA, 0, (uint32_t)rowbuf);
                DL_DMA_setTransferSize(DMA, 0, half ? w : 2 * w);
                DL_DMA_enableChannel(DMA, 0);
                while (DL_DMA_isChannelEnabled(DMA, 0)) idle_iters++;
            }
        }
        break;
    }
    }
    end();
    return us_since(c0);
}

/* read 64 pixels of row 240 back from the screen (needs MISO), at 4 MHz */
static void readback(uint8_t *out)
{
    DL_SPI_setBitRateSerialClockDivider(SPI, READ_SCR);
    CMD(0x2A, 0, 0, 0, 63);
    CMD(0x2B, 0, 240, 0, 240);
    while (!DL_SPI_isRXFIFOEmpty(SPI)) (void)DL_SPI_receiveData8(SPI);
    cs_low(); dc(0); tx8(0x2E); spi_wait();
    while (!DL_SPI_isRXFIFOEmpty(SPI)) (void)DL_SPI_receiveData8(SPI);
    dc(1);
    for (int i = 0; i < 1 + 3 * 64; i++) {
        tx8(0);
        while (DL_SPI_isRXFIFOEmpty(SPI)) ;
        out[i] = DL_SPI_receiveData8(SPI);
    }
    spi_wait(); cs_high();
}

int main(void)
{
    SYSCFG_DL_init();
    SysTick->LOAD = 0xFFFFFF; SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    uint32_t t0 = cyc();
    DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);
    DL_SPI_setBitRateSerialClockDivider(SPI, 0);
    lcd_init();
    res.boot_init_us = us_since(t0);
    {   /* read the power-mode register once (1 byte, no dummy) at 4 MHz */
        DL_SPI_setBitRateSerialClockDivider(SPI, READ_SCR);
        while (!DL_SPI_isRXFIFOEmpty(SPI)) (void)DL_SPI_receiveData8(SPI);
        cs_low(); dc(0); tx8(0x0A); spi_wait();
        while (!DL_SPI_isRXFIFOEmpty(SPI)) (void)DL_SPI_receiveData8(SPI);
        dc(1); tx8(0); while (DL_SPI_isRXFIFOEmpty(SPI)) ;
        res.pwr_mode = DL_SPI_receiveData8(SPI);
        spi_wait(); cs_high();
        DL_SPI_setBitRateSerialClockDivider(SPI, 0);
    }
    color = 0x0000;
    draw(1, 0, 0, W, H);
    res.boot_first_frame_us = us_since(t0);

    /* cost of one pass of the DMA wait loop, measured with the same body */
    uint32_t c0 = cyc();
    for (idle_iters = 0; idle_iters < 100000; ) { (void)DL_DMA_isChannelEnabled(DMA, 1); idle_iters++; }
    uint32_t loop_cyc100k = cyc() - c0;
    res.loop_cyc_x1000 = loop_cyc100k / 100;

    static const uint16_t colors[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    for (int s = 0; s < 2; s++) {
        DL_SPI_setBitRateSerialClockDivider(SPI, s == 0 ? 1 : 0);   /* 8 MHz, 16 MHz */
        for (int m = 0; m < 4; m++) {
            color = colors[m];
            if (m >= 2) { dma_setup(m == 3); fill_row_buffer(m == 3); }
            idle_iters = 0;
            res.m[s][m].full_us = draw(m, 0, 0, W, H);
            if (m >= 2)
                res.dma_free_pct[s][m - 2] = (uint32_t)((uint64_t)idle_iters * loop_cyc100k / 100000 * 100
                                             / ((uint64_t)res.m[s][m].full_us * (CPUCLK_FREQ / 1000000)));
            res.m[s][m].box100_us = draw(m, 110, 190, 100, 100);
            res.m[s][m].box32_us = draw(m, 144, 224, 32, 32);
            delay_ms(150);
        }
        /* integrity: a patterned dma16 frame at this speed, then read row 240 back */
        color = 0x1234; dma_setup(1); fill_row_buffer(1);
        draw(3, 0, 0, W, H);
        readback(res.rb[s]);
    }
    res.magic = 0x9E7F0001;
    for (;;) ;
}
