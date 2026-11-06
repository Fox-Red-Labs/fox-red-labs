/* Port layer: SysTick tick, ST7796S flush over SPI1 (16-bit frames, DMA), FT6336U touch over
 * bit-banged I2C, UART0 text out. Traps from display-test/README.md are handled here:
 *   1. chip select needs a 10 us gap after it goes high (cs_gap), or the screen drops commands;
 *   2. touch only answers software I2C, not the chip's I2C block;
 *   3. SPI frame size changes only take effect with the SPI block disabled (frames()). */
#include "ti_msp_dl_config.h"
#include "lvgl.h"
#include <stddef.h>
#include "port.h"
#include "board.h"

#ifndef SPI_SCR
#define SPI_SCR 0              /* SPI serial clock divider: 0 gives 16 MHz at 32 MHz CPU, 40 MHz at 80 MHz CPU */
#endif
#ifndef DRAW_ROWS
#define DRAW_ROWS 10           /* rows in the one partial draw buffer */
#endif
#define TP_ADDR 0x38

port_stats_t port_stats;

/* TI's libc has no strnlen, and LVGL's string layer (LV_STDLIB_CLIB) calls it. */
size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n]) n++;
    return n;
}

/* ---------- time and LED ---------- */
static volatile uint32_t ms_ticks;
static volatile uint8_t  led_level;          /* 0..10 out of 10 SysTick slots */

void SysTick_Handler(void)
{
    static uint8_t phase;
    ms_ticks++;
    if (++phase >= 10) phase = 0;
    if (led_level > phase) DL_GPIO_setPins(PA_PORT, PA_LED1_PIN);
    else                   DL_GPIO_clearPins(PA_PORT, PA_LED1_PIN);
}
uint32_t port_ms(void) { return ms_ticks; }
uint32_t port_us(void)
{
    uint32_t m, v;
    do { m = ms_ticks; v = SysTick->VAL; } while (m != ms_ticks);
    return m * 1000u + (SysTick->LOAD - v) / (CPUCLK_FREQ / 1000000u);
}
void port_led(int on, int level) { led_level = on ? (uint8_t)level : 0; }
static void delay_ms(uint32_t n) { while (n--) delay_cycles(CPUCLK_FREQ / 1000); }

/* ---------- UART0 (XDS110 backchannel, 115200 8N1) ---------- */
void port_uart_puts(const char *s)
{
    while (*s) DL_UART_Main_transmitDataBlocking(DBG_UART_INST, (uint8_t)*s++);
}

/* ---------- stack high-water mark ---------- */
extern uint32_t __STACK_END;   /* linker: top of the stack region (highest address) */
extern uint32_t __STACK_SIZE;
static uint32_t stack_base(void) { return (uint32_t)&__STACK_END - (uint32_t)&__STACK_SIZE; }
static void stack_paint(void)
{
    volatile uint32_t *p = (volatile uint32_t *)stack_base();
    uint32_t sp; __asm volatile("mov %0, sp" : "=r"(sp));
    while ((uint32_t)p < sp - 64) *p++ = 0xA5A5A5A5u;
}
uint32_t port_stack_free(void)
{
    const uint32_t *p = (const uint32_t *)stack_base();
    uint32_t n = 0;
    while (p[n] == 0xA5A5A5A5u && (uint32_t)(p + n) < (uint32_t)&__STACK_END) n++;
    return n * 4;
}

/* ---------- SPI and screen ---------- */
#define SPI LCD_SPI_INST
static void cs_gap(void) { delay_cycles(CPUCLK_FREQ / 100000); }   /* 10 us */
static void spi_wait(void)
{
    while (!DL_SPI_isTXFIFOEmpty(SPI)) { }
    delay_cycles(32);
    while (DL_SPI_isBusy(SPI)) { }
}
static inline void tx8(uint8_t b) { while (DL_SPI_isTXFIFOFull(SPI)) { } DL_SPI_transmitData8(SPI, b); }
static void cs_low(void)  { DL_GPIO_clearPins(PB_PORT, PB_LCD_CS_PIN); }
static void cs_high(void) { DL_GPIO_setPins(PB_PORT, PB_LCD_CS_PIN); cs_gap(); }
static void dc(int data)  { if (data) DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN); else DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN); }

static void lcd_cmd(uint8_t c, const uint8_t *d, int n)
{
    cs_low(); dc(0); tx8(c); spi_wait(); dc(1);
    for (int i = 0; i < n; i++) tx8(d[i]);
    spi_wait(); cs_high();
}
#define CMD(c, ...) do { const uint8_t _d[] = {0, ##__VA_ARGS__}; lcd_cmd(c, _d + 1, sizeof(_d) - 1); } while (0)

static void lcd_init(void)
{
    DL_GPIO_clearPins(LCD_RST_PORT, LCD_RST_PIN_MASK); delay_ms(10);
    DL_GPIO_setPins(LCD_RST_PORT, LCD_RST_PIN_MASK);   delay_ms(120);
    CMD(0x01); delay_ms(120);              /* software reset */
    CMD(0x11); delay_ms(120);              /* sleep out */
    CMD(0x36, 0x48);                       /* portrait, BGR order */
    CMD(0x3A, 0x55);                       /* 16 bits per pixel */
    CMD(0x21);                             /* inversion on (this panel needs it) */
    CMD(0x29); delay_ms(20);               /* display on */
}
/* The SPI block ignores a frame-size change while enabled: disable, change, enable. */
static void frames16(int on)
{
    DL_SPI_disable(SPI);
    DL_SPI_setDataSize(SPI, on ? DL_SPI_DATA_SIZE_16 : DL_SPI_DATA_SIZE_8);
    DL_SPI_enable(SPI);
}
static void dma_setup(void)
{
    DL_DMA_Config c = {
        .transferMode = DL_DMA_SINGLE_TRANSFER_MODE, .extendedMode = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_UNCHANGED, .srcIncrement = DL_DMA_ADDR_INCREMENT,
        .destWidth = DL_DMA_WIDTH_HALF_WORD, .srcWidth = DL_DMA_WIDTH_HALF_WORD,
        .trigger = DMA_SPI1_TX_TRIG, .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL,
    };
    DL_DMA_initChannel(DMA, 0, &c);
    DL_DMA_setDestAddr(DMA, 0, (uint32_t)&SPI->TXDATA);
    DL_SPI_enableDMATransmitEvent(SPI);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *a, uint8_t *px)
{
    uint32_t t0 = port_us();
    int x0 = a->x1, x1 = a->x2, y0 = a->y1, y1 = a->y2;
    uint32_t n = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);       /* pixels; at most 320 * DRAW_ROWS */
    CMD(0x2A, x0 >> 8, x0, x1 >> 8, x1);
    CMD(0x2B, y0 >> 8, y0, y1 >> 8, y1);
    cs_low(); dc(0); tx8(0x2C); spi_wait(); dc(1);
    frames16(1);
    /* LVGL's RGB565 is a native uint16_t; one 16-bit SPI frame sends it high byte first, as the screen wants. */
    DL_DMA_setSrcAddr(DMA, 0, (uint32_t)px);
    DL_DMA_setTransferSize(DMA, 0, n);
    DL_DMA_enableChannel(DMA, 0);
    while (DL_DMA_isChannelEnabled(DMA, 0)) { }
    spi_wait(); frames16(0); cs_high();
    uint32_t dt = port_us() - t0;
    port_stats.flush_us_last = dt;
    port_stats.flush_us_sum += dt;
    port_stats.flush_px += n;
    lv_display_flush_ready(disp);
}

static uint32_t refr_t0;
static void refr_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_REFR_START) { refr_t0 = port_us(); return; }
    uint32_t dt = port_us() - refr_t0;
    port_stats.refr_cnt++;
    port_stats.refr_us_sum += dt;
    port_stats.refr_us_last = dt;
    if (dt > port_stats.refr_us_max) port_stats.refr_us_max = dt;
    if (port_stats.full_pending) { port_stats.full_pending = 0; port_stats.full_redraw_us = dt; }
}

/* ---------- touch: FT6336U over software I2C ---------- */
#define SDA_L() DL_GPIO_enableOutput(TP_GPIO, TP_SDA_MASK)
#define SDA_H() DL_GPIO_disableOutput(TP_GPIO, TP_SDA_MASK)
#define SCL_L() DL_GPIO_enableOutput(TP_GPIO, TP_SCL_MASK)
#define HALF()  delay_cycles(CPUCLK_FREQ / 200000)     /* 5 us per half bit: about 100 kHz */
static void SCL_H(void)
{
    DL_GPIO_disableOutput(TP_GPIO, TP_SCL_MASK);
    for (int t = 0; t < 2000 && !DL_GPIO_readPins(TP_GPIO, TP_SCL_MASK); t++) { }   /* clock stretch */
}
static void bb_start(void) { SDA_H(); SCL_H(); HALF(); SDA_L(); HALF(); SCL_L(); HALF(); }
static void bb_stop(void)  { SDA_L(); HALF(); SCL_H(); HALF(); SDA_H(); HALF(); }
static int bb_write(uint8_t v)
{
    for (int b = 7; b >= 0; b--) {
        if (v & (1 << b)) SDA_H(); else SDA_L();
        HALF(); SCL_H(); HALF(); SCL_L();
    }
    SDA_H(); HALF(); SCL_H(); HALF();
    int nack = DL_GPIO_readPins(TP_GPIO, TP_SDA_MASK) ? 1 : 0;
    SCL_L(); HALF();
    return nack;
}
static uint8_t bb_read(int ack)
{
    uint8_t v = 0;
    SDA_H();
    for (int b = 7; b >= 0; b--) {
        HALF(); SCL_H(); HALF();
        if (DL_GPIO_readPins(TP_GPIO, TP_SDA_MASK)) v |= 1 << b;
        SCL_L();
    }
    if (ack) SDA_L(); else SDA_H();
    HALF(); SCL_H(); HALF(); SCL_L(); SDA_H(); HALF();
    return v;
}
static int tp_read(uint8_t reg, uint8_t *buf, int n)
{
    bb_start();
    if (bb_write(TP_ADDR << 1) || bb_write(reg)) { bb_stop(); return -1; }
    bb_start();
    if (bb_write((TP_ADDR << 1) | 1)) { bb_stop(); return -1; }
    for (int i = 0; i < n; i++) buf[i] = bb_read(i < n - 1);
    bb_stop();
    return 0;
}
static void touch_init(void)
{
    /* I2C pins as plain open-drain GPIO with the chip's pull-ups (the I2C block is not used) */
    DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(TP_GPIO, TP_SDA_MASK | TP_SCL_MASK);
    DL_GPIO_disableOutput(TP_GPIO, TP_SDA_MASK | TP_SCL_MASK);
    DL_GPIO_clearPins(TP_RST_PORT, TP_RST_PIN_MASK); delay_ms(10);
    DL_GPIO_setPins(TP_RST_PORT, TP_RST_PIN_MASK);   delay_ms(300);
}
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int16_t lx, ly;
    static lv_indev_state_t st = LV_INDEV_STATE_RELEASED;
    uint8_t p[5];
    (void)indev;
    if (tp_read(0x02, p, 5) == 0) {            /* 0x02 touch count, 0x03..0x06 X high/low, Y high/low */
        int n = p[0] & 0x0F;
        if (n > 0 && n < 3) {
            int x = ((p[1] & 0x0F) << 8) | p[2], y = ((p[3] & 0x0F) << 8) | p[4];
            lx = x >= LCD_W ? LCD_W - 1 : x;
            ly = y >= LCD_H ? LCD_H - 1 : y;
            st = LV_INDEV_STATE_PRESSED;
        } else st = LV_INDEV_STATE_RELEASED;
    } else port_stats.touch_errors++;          /* keep the last state on a failed read */
    data->point.x = lx; data->point.y = ly; data->state = st;
}

/* ---------- init ---------- */
static uint8_t draw_buf[LCD_W * DRAW_ROWS * 2] __attribute__((aligned(4)));

void port_init(void)
{
    stack_paint();
    SysTick->LOAD = CPUCLK_FREQ / 1000 - 1; SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    DL_SPI_setBitRateSerialClockDivider(SPI, SPI_SCR);
    DL_GPIO_setPins(LCD_BL_PORT, LCD_BL_PIN_MASK);
    lcd_init();
    dma_setup();
    touch_init();

    lv_init();
    lv_tick_set_cb(port_ms);
    lv_display_t *d = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);
    lv_display_add_event_cb(d, refr_event_cb, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(d, refr_event_cb, LV_EVENT_REFR_READY, NULL);

    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch_read_cb);
    lv_timer_set_period(lv_indev_get_read_timer(in), 20);
}
