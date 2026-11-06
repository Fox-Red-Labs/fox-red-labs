/* ST7796S screen over SPI and FT6336U touch over bit-banged I2C, for the Waveshare 3.5 inch
 * capacitive touch LCD wired to the LP-MSPM0G3507. Copied from
 * mspm0g3507/display-test/display_test.c; one change: the software-I2C half-bit delay now scales with the CPU clock (where each workaround is explained), so the digit
 * demo uses the driver that was already tested on this board. Needs W, H, TP_ADDR and delay_ms().
 */
#ifndef LCD_TOUCH_H
#define LCD_TOUCH_H
/* ---------- screen (ST7796S over SPI) ---------- */
/* Wait until the last bit has left: FIFO empty AND shifter idle. BUSY can read idle for a
   moment right after a byte is queued, so check the FIFO first and re-check BUSY. */
static void spi_wait(void)
{
    while (!DL_SPI_isTXFIFOEmpty(LCD_SPI_INST)) ;
    delay_cycles(32);
    while (DL_SPI_isBusy(LCD_SPI_INST)) ;
}
/* Chip select must stay high long enough to reach the screen. The module's level
   translator (TXS0108E) swallowed a high pulse of a few hundred ns between two commands,
   so the second command was lost. 10 us is plenty and costs nothing per command. */
static void cs_gap(void) { delay_cycles(CPUCLK_FREQ / 100000); }
static inline void spi_byte(uint8_t b)
{
    while (DL_SPI_isTXFIFOFull(LCD_SPI_INST)) ;
    DL_SPI_transmitData8(LCD_SPI_INST, b);
}
static void lcd_cmd(uint8_t c, const uint8_t *d, int n)
{
    DL_GPIO_clearPins(PB_PORT, PB_LCD_CS_PIN);
    DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN);   /* DC low: command */
    spi_byte(c); spi_wait();
    DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN);     /* DC high: data */
    for (int i = 0; i < n; i++) spi_byte(d[i]);
    spi_wait();
    DL_GPIO_setPins(PB_PORT, PB_LCD_CS_PIN);
    cs_gap();
}
/* Send a command, then clock in n reply bytes on MISO (PB7). Raw bytes: some replies
   start with one dummy bit, which the decoder strips. */
static void lcd_read(uint8_t c, uint8_t *out, int n)
{
    while (!DL_SPI_isRXFIFOEmpty(LCD_SPI_INST)) (void)DL_SPI_receiveData8(LCD_SPI_INST);
    DL_GPIO_clearPins(PB_PORT, PB_LCD_CS_PIN);
    DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN);
    spi_byte(c); spi_wait();
    while (!DL_SPI_isRXFIFOEmpty(LCD_SPI_INST)) (void)DL_SPI_receiveData8(LCD_SPI_INST);
    DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN);
    for (int i = 0; i < n; i++) {
        spi_byte(0x00);
        while (DL_SPI_isRXFIFOEmpty(LCD_SPI_INST)) ;
        out[i] = DL_SPI_receiveData8(LCD_SPI_INST);
    }
    spi_wait();
    DL_GPIO_setPins(PB_PORT, PB_LCD_CS_PIN);
    cs_gap();
}
#define CMD(c, ...) do { const uint8_t _d[] = {0, ##__VA_ARGS__}; \
                         lcd_cmd(c, _d + 1, sizeof(_d) - 1); } while (0)

static void lcd_init(void)
{
    DL_GPIO_clearPins(PB_PORT, PB_LCD_RST_PIN); delay_ms(10);
    DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN);   delay_ms(120);
    CMD(0x01); delay_ms(120);          /* software reset */
    CMD(0x11); delay_ms(120);          /* sleep out */
    CMD(0x36, 0x48);                   /* portrait, BGR order */
    CMD(0x3A, 0x55);                   /* 16 bits per pixel */
#ifdef FULL_INIT
    /* The longer power, timing and gamma setup that open-source ST7796S drivers send. */
    CMD(0xF0, 0xC3); CMD(0xF0, 0x96);  /* unlock extended commands */
    CMD(0xB4, 0x01);                   /* 1-dot inversion */
    CMD(0xB7, 0xC6);                   /* entry mode */
    CMD(0xC0, 0x80, 0x45);             /* power control 1 */
    CMD(0xC1, 0x13);                   /* power control 2 */
    CMD(0xC2, 0xA7);                   /* power control 3 */
    CMD(0xC5, 0x0A);                   /* VCOM */
    CMD(0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33);
    CMD(0xE0, 0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31);
    CMD(0xE1, 0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32);
    CMD(0xF0, 0x3C); CMD(0xF0, 0x69);  /* lock again */
    delay_ms(120);
#endif
    CMD(0x21);                         /* inversion on: IPS panels need it */
    CMD(0x29); delay_ms(20);           /* display on */
}

static void lcd_window(int x0, int y0, int x1, int y1)
{
    CMD(0x2A, x0 >> 8, x0, x1 >> 8, x1);
    CMD(0x2B, y0 >> 8, y0, y1 >> 8, y1);
}
static void lcd_fill(int x0, int y0, int x1, int y1, uint16_t c)
{
    uint32_t n = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
    uint8_t hi = c >> 8, lo = c;
    lcd_window(x0, y0, x1, y1);
    DL_GPIO_clearPins(PB_PORT, PB_LCD_CS_PIN);
    DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN);
    spi_byte(0x2C); spi_wait();         /* memory write */
    DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN);
    while (n--) { spi_byte(hi); spi_byte(lo); }
    spi_wait();
    DL_GPIO_setPins(PB_PORT, PB_LCD_CS_PIN);
    cs_gap();
}

/* ---------- touch (FT6336U over I2C) ---------- */
static int i2c_wait_idle(void)
{
    for (uint32_t t = 0; t < 200000; t++)
        if (DL_I2C_getControllerStatus(TP_I2C_INST) & DL_I2C_CONTROLLER_STATUS_IDLE)
            return 0;
    return -1;
}
static int tp_read_n(uint8_t addr, uint8_t reg, uint8_t *buf, int n);
static int tp_read(uint8_t reg, uint8_t *buf, int n) { return tp_read_n(TP_ADDR, reg, buf, n); }
static int tp_read_at(uint8_t addr, uint8_t reg, uint8_t *buf) { return tp_read_n(addr, reg, buf, 1); }
/* Software I2C on PB3 (SDA) and PB2 (SCL), open drain: "low" = output on at 0,
 * "high" = output off and the line floats up. Used because the hardware I2C block
 * did not get an answer through this module (see README); the same pins answer here. */
#define SDA_L() DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_3)
#define SDA_H() DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_3)
#define SCL_L() DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_2)
#define HALF()  delay_cycles(CPUCLK_FREQ / 200000)   /* 5 us: 100 kHz at any CPU clock (the original 160 cycles assumed 32 MHz) */
static void SCL_H(void)
{
    DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_2);
    for (int t = 0; t < 2000 && !DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_2); t++) ;  /* clock stretch */
}
static void bb_start(void) { SDA_H(); SCL_H(); HALF(); SDA_L(); HALF(); SCL_L(); HALF(); }
static void bb_stop(void)  { SDA_L(); HALF(); SCL_H(); HALF(); SDA_H(); HALF(); }
static int bb_write(uint8_t v)   /* returns 0 on ACK */
{
    for (int b = 7; b >= 0; b--) {
        if (v & (1 << b)) SDA_H(); else SDA_L();
        HALF(); SCL_H(); HALF(); SCL_L();
    }
    SDA_H(); HALF(); SCL_H(); HALF();
    int nack = DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_3) ? 1 : 0;
    SCL_L(); HALF();
    return nack;
}
static uint8_t bb_read(int ack)
{
    uint8_t v = 0;
    SDA_H();
    for (int b = 7; b >= 0; b--) {
        HALF(); SCL_H(); HALF();
        if (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_3)) v |= 1 << b;
        SCL_L();
    }
    if (ack) SDA_L(); else SDA_H();
    HALF(); SCL_H(); HALF(); SCL_L(); SDA_H(); HALF();
    return v;
}
static int tp_read_n(uint8_t addr, uint8_t reg, uint8_t *buf, int n)
{
    bb_start();
    if (bb_write(addr << 1)) { bb_stop(); return -1; }
    if (bb_write(reg))       { bb_stop(); return -2; }
    bb_start();                                   /* repeated start, then read */
    if (bb_write((addr << 1) | 1)) { bb_stop(); return -3; }
    for (int i = 0; i < n; i++) buf[i] = bb_read(i < n - 1);
    bb_stop();
    return 0;
}

#endif
