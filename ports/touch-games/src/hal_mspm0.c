/* hal for the MSPM0G3507 and MSPM0G3519 with the Waveshare 3.5 inch display (ST7796S screen over SPI, FT6336U touch over
 * bit-banged I2C). Written fresh from the ST7796S and FT6336 data sheets and the repository's earlier display runs.
 * Traps from those runs, handled here:
 *   1. chip select needs a 10 us gap after it goes high or the screen drops commands;
 *   2. touch only answers software I2C, not the chip's I2C block;
 *   3. a change of SPI frame size only takes effect with the SPI block disabled. */
#include "ti_msp_dl_config.h"
#include <stddef.h>
#include "hal.h"
#include BOARD_HEADER

#ifndef SPI_SCR
#define SPI_SCR 1              /* SPI clock divider; 0 is the fastest the screen was run at, 1 is half of that */
#endif
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 0
#endif
#ifndef TOUCH_FLIP_X
#define TOUCH_FLIP_X 0
#endif
#ifndef TOUCH_FLIP_Y
#define TOUCH_FLIP_Y 0
#endif
#define TP_ADDR 0x38
#define SPI LCD_SPI_INST

extern int main_app(void);
int main(void) { SYSCFG_DL_init(); main_app(); return 0; }

/* TI's libc has no strnlen and some libs call it; harmless to provide. */
size_t strnlen(const char *s, size_t max) { size_t n = 0; while (n < max && s[n]) n++; return n; }

/* ---------- stack high-water mark: paint the free stack at boot, count the untouched words later ---------- */
extern uint32_t __STACK_END;
extern uint32_t __STACK_SIZE;
static uint32_t stack_base(void) { return (uint32_t)&__STACK_END - (uint32_t)&__STACK_SIZE; }
static void stack_paint(void)
{
    volatile uint32_t *p = (volatile uint32_t *)stack_base();
    uint32_t sp; __asm volatile("mov %0, sp" : "=r"(sp));
    while ((uint32_t)p < sp - 64) *p++ = 0xA5A5A5A5u;
}
uint32_t hal_stack_used(void)
{
    const uint32_t *p = (const uint32_t *)stack_base(); uint32_t n = 0;
    while (p[n] == 0xA5A5A5A5u && (uint32_t)(p + n) < (uint32_t)&__STACK_END) n++;
    return (uint32_t)&__STACK_SIZE - n * 4;
}

/* ---------- time ---------- */
static volatile uint32_t ms_ticks;
void SysTick_Handler(void) { ms_ticks++; }
uint32_t hal_ms(void) { return ms_ticks; }
uint32_t hal_us(void)
{
    uint32_t m, v;
    do { m = ms_ticks; v = SysTick->VAL; } while (m != ms_ticks);
    return m * 1000u + (SysTick->LOAD - v) / (CPUCLK_FREQ / 1000000u);
}
void hal_idle(void) { __WFI(); }
static void delay_ms(uint32_t n) { while (n--) delay_cycles(CPUCLK_FREQ / 1000); }

void hal_log(const char *s) { while (*s) DL_UART_Main_transmitDataBlocking(DBG_UART_INST, (uint8_t)*s++); }

/* ---------- screen ---------- */
static void cs_gap(void) { delay_cycles(CPUCLK_FREQ / 100000); }   /* 10 us */
static void spi_wait(void)
{
    while (!DL_SPI_isTXFIFOEmpty(SPI)) { }
    delay_cycles(32);
    while (DL_SPI_isBusy(SPI)) { }
}
static inline void tx8(uint8_t b)    { while (DL_SPI_isTXFIFOFull(SPI)) { } DL_SPI_transmitData8(SPI, b); }
static inline void tx16(uint16_t v)  { while (DL_SPI_isTXFIFOFull(SPI)) { } DL_SPI_transmitData16(SPI, v); }
static void cs_low(void)  { DL_GPIO_clearPins(LCD_CS_PORT, LCD_CS_PIN); }
static void cs_high(void) { DL_GPIO_setPins(LCD_CS_PORT, LCD_CS_PIN); cs_gap(); }
static void dc(int data)  { if (data) DL_GPIO_setPins(LCD_DC_PORT, LCD_DC_PIN); else DL_GPIO_clearPins(LCD_DC_PORT, LCD_DC_PIN); }

static void lcd_cmd(uint8_t c, const uint8_t *d, int n)
{
    cs_low(); dc(0); tx8(c); spi_wait(); dc(1);
    for (int i = 0; i < n; i++) tx8(d[i]);
    spi_wait(); cs_high();
}
#define CMD(c, ...) do { const uint8_t _d[] = {0, ##__VA_ARGS__}; lcd_cmd(c, _d + 1, sizeof(_d) - 1); } while (0)

static void lcd_init(void)
{
    DL_GPIO_clearPins(LCD_RST_PORT, LCD_RST_PIN); delay_ms(10);
    DL_GPIO_setPins(LCD_RST_PORT, LCD_RST_PIN);   delay_ms(120);
    CMD(0x01); delay_ms(120);              /* software reset */
    CMD(0x11); delay_ms(120);              /* sleep out */
    CMD(0x36, 0x48);                       /* portrait, BGR order */
    CMD(0x3A, 0x55);                       /* 16 bits per pixel */
    CMD(0x21);                             /* inversion on (this panel needs it) */
    CMD(0x29); delay_ms(20);               /* display on */
}
static void frames16(int on)
{
    DL_SPI_disable(SPI);
    DL_SPI_setDataSize(SPI, on ? DL_SPI_DATA_SIZE_16 : DL_SPI_DATA_SIZE_8);
    DL_SPI_enable(SPI);
}
static void window(int x, int y, int w, int h)
{
    int x1 = x + w - 1, y1 = y + h - 1;
    CMD(0x2A, x >> 8, x, x1 >> 8, x1);
    CMD(0x2B, y >> 8, y, y1 >> 8, y1);
    cs_low(); dc(0); tx8(0x2C); spi_wait(); dc(1);
    frames16(1);
}
static void window_end(void) { spi_wait(); frames16(0); cs_high(); }

void hal_lcd_fill(int x, int y, int w, int h, uint16_t c)
{
    window(x, y, w, h);
    for (uint32_t n = (uint32_t)w * h; n; n--) tx16(c);
    window_end();
}
void hal_lcd_blit(int x, int y, int w, int h, const uint16_t *px)
{
    window(x, y, w, h);
    for (uint32_t n = (uint32_t)w * h; n; n--) tx16(*px++);
    window_end();
}

/* ---------- touch: FT6336U over software I2C, about 100 kHz ---------- */
#define SDA_L() DL_GPIO_enableOutput(TP_GPIO, TP_SDA_MASK)
#define SDA_H() DL_GPIO_disableOutput(TP_GPIO, TP_SDA_MASK)
#define SCL_L() DL_GPIO_enableOutput(TP_GPIO, TP_SCL_MASK)
#define HALF()  delay_cycles(CPUCLK_FREQ / 200000)
static void SCL_H(void)
{
    DL_GPIO_disableOutput(TP_GPIO, TP_SCL_MASK);
    for (int t = 0; t < 2000 && !DL_GPIO_readPins(TP_GPIO, TP_SCL_MASK); t++) { }   /* clock stretch */
}
static void bb_start(void) { SDA_H(); SCL_H(); HALF(); SDA_L(); HALF(); SCL_L(); HALF(); }
static void bb_stop(void)  { SDA_L(); HALF(); SCL_H(); HALF(); SDA_H(); HALF(); }
static int bb_write(uint8_t v)
{
    for (int b = 7; b >= 0; b--) { if (v & (1 << b)) SDA_H(); else SDA_L(); HALF(); SCL_H(); HALF(); SCL_L(); }
    SDA_H(); HALF(); SCL_H(); HALF();
    int nack = DL_GPIO_readPins(TP_GPIO, TP_SDA_MASK) ? 1 : 0;
    SCL_L(); HALF();
    return nack;
}
static uint8_t bb_read(int ack)
{
    uint8_t v = 0;
    SDA_H();
    for (int b = 7; b >= 0; b--) { HALF(); SCL_H(); HALF(); if (DL_GPIO_readPins(TP_GPIO, TP_SDA_MASK)) v |= 1 << b; SCL_L(); }
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

int hal_touch(int *x, int *y)
{
    uint8_t p[5];
    if (tp_read(0x02, p, 5)) return -1;          /* 0x02 touch count, 0x03..0x06 X high/low, Y high/low */
    int n = p[0] & 0x0F;
    if (n < 1 || n > 2) return 0;
    int tx = ((p[1] & 0x0F) << 8) | p[2], ty = ((p[3] & 0x0F) << 8) | p[4];
    if (TOUCH_SWAP_XY) { int t = tx; tx = ty; ty = t; }
    if (TOUCH_FLIP_X) tx = LCD_W - 1 - tx;
    if (TOUCH_FLIP_Y) ty = LCD_H - 1 - ty;
    *x = tx >= LCD_W ? LCD_W - 1 : tx;
    *y = ty >= LCD_H ? LCD_H - 1 : ty;
    return 1;
}

void hal_init(void)
{
    stack_paint();
    SysTick->LOAD = CPUCLK_FREQ / 1000 - 1; SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    DL_SPI_setBitRateSerialClockDivider(SPI, SPI_SCR);
    DL_GPIO_setPins(LCD_BL_PORT, LCD_BL_PIN);
    lcd_init();
    /* touch pins as plain open-drain GPIO with the chip's pull-ups (the I2C block is not used) */
    DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(TP_GPIO, TP_SDA_MASK | TP_SCL_MASK);
    DL_GPIO_disableOutput(TP_GPIO, TP_SDA_MASK | TP_SCL_MASK);
    DL_GPIO_clearPins(TP_RST_PORT, TP_RST_PIN); delay_ms(10);
    DL_GPIO_setPins(TP_RST_PORT, TP_RST_PIN);   delay_ms(300);
}
