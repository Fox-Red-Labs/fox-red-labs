/* Waveshare 3.5inch Capacitive Touch LCD (ST7796S + FT6336U) on the LP-MSPM0G3507.
 *
 * 1. Wakes the screen, fills it red, green, blue, white, black and times each fill.
 * 2. Draws eight color bars.
 * 3. Asks the touch chip for its ID over I2C.
 * 4. Draws a white dot wherever a finger touches, forever.
 *
 * Results sit in `res` in RAM, read over the debugger. No serial port needed.
 * Build: ./build.sh  (SPI_SCR=0 gives a 16 MHz SPI clock, SPI_SCR=1 gives 8 MHz)
 */
#include "ti_msp_dl_config.h"

#ifndef SPI_SCR
#define SPI_SCR 1
#endif

#define W 320
#define H 480
#define TP_ADDR 0x38

struct {
    uint32_t magic;          /* 0xD15B1A00 + stage reached */
    uint32_t spi_hz;
    uint32_t fill_us[5];     /* red, green, blue, white, black */
    uint32_t bars_us;
    int32_t  tp_ack;         /* 1 = touch chip answered at 0x38, -1 = no answer */
    uint8_t  tp_vendor;      /* reg 0xA8, FocalTech = 0x11 */
    uint8_t  tp_chip;        /* reg 0xA3, FT6336U = 0x64 */
    uint8_t  tp_fw;          /* reg 0xA6 */
    uint8_t  tp_int_idle;    /* INT pin level with no touch (expect 1) */
    uint32_t touches;        /* polls that saw a finger */
    uint16_t last_x, last_y;
    uint32_t i2c_errors;
    int32_t  tp_rc;          /* tp_read return code on the first try */
    uint32_t i2c_status;     /* controller status after the first try */
    uint32_t lines_idle;     /* bit0 SDA (PB3), bit1 SCL (PB2) before the first transfer */
    uint8_t  scan[16];       /* bitmap of I2C addresses that answered, 0x08 to 0x77 */
    uint32_t drive_probe;    /* each line driven low as a plain output, read back:
                                bit0 SDA, bit1 SCL (1 = still reads high: tied to a supply) */
    uint32_t bb_ack;         /* bit-banged I2C: 1 = 0x38 acknowledged its address */
    uint32_t line_probe;     /* SDA/SCL as plain inputs: bit0 SDA pulled up, bit1 SCL pulled up,
                                bit2 SDA pulled down, bit3 SCL pulled down (1 = reads high) */
} res;
uint8_t rb[3][6][6];   /* readback snapshots, see READBACK */
uint8_t ex[8][2];      /* EXPERIMENT: [step] = {power mode 0A, pixel format 0C} */

static volatile uint32_t ticks;  /* SysTick overflows, 2^24 cycles each */
void SysTick_Handler(void) { ticks++; }

static uint32_t now_cycles(void)
{
    uint32_t t1, v;
    do { t1 = ticks; v = SysTick->VAL; } while (t1 != ticks);
    return (t1 << 24) + (0xFFFFFF - v);
}
static uint32_t us_since(uint32_t c0) { return (now_cycles() - c0) / (CPUCLK_FREQ / 1000000); }
static void delay_ms(uint32_t ms) { while (ms--) delay_cycles(CPUCLK_FREQ / 1000); }

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
#define HALF()  delay_cycles(160)   /* 5 us: 100 kHz */
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

int main(void)
{
    SYSCFG_DL_init();
    DL_SPI_setBitRateSerialClockDivider(LCD_SPI_INST, SPI_SCR);
    /* The board has no I2C pull-ups fitted (R59, R60): turn on the chip's own. */
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_TP_I2C_IOMUX_SDA, GPIO_TP_I2C_IOMUX_SDA_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_TP_I2C_IOMUX_SCL, GPIO_TP_I2C_IOMUX_SCL_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    /* That call rewrites the whole pin register, which clears open-drain: set it again,
       exactly as SysConfig's own code does. Without this, I2C cannot work. */
    DL_GPIO_enableHiZ(GPIO_TP_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_TP_I2C_IOMUX_SCL);
    SysTick->LOAD = 0xFFFFFF; SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

#ifdef PHOTO
    {   /* Show the photo from photo.py, each stored pixel drawn as a 2x2 block. */
        DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);
        lcd_init();
        uint32_t c0 = now_cycles();
        lcd_window(0, 0, W - 1, H - 1);
        DL_GPIO_clearPins(PB_PORT, PB_LCD_CS_PIN);
        DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN);
        spi_byte(0x2C); spi_wait();
        DL_GPIO_setPins(PB_PORT, PB_LCD_DC_PIN);
#ifdef PHOTO_PAL16
        for (uint32_t i = 0; i < (uint32_t)W * H / 2; i++) {   /* two pixels per byte */
            uint16_t a = photo_pal[photo4[i] >> 4], b = photo_pal[photo4[i] & 15];
            spi_byte(a >> 8); spi_byte(a); spi_byte(b >> 8); spi_byte(b);
        }
#else
        for (int y = 0; y < H; y++) {
            const uint16_t *row = &photo[(y / 2) * PHOTO_W];
            for (int x = 0; x < PHOTO_W; x++) {
                uint16_t c = row[x];
                spi_byte(c >> 8); spi_byte(c); spi_byte(c >> 8); spi_byte(c);
            }
        }
#endif
        spi_wait();
        DL_GPIO_setPins(PB_PORT, PB_LCD_CS_PIN);
        res.bars_us = us_since(c0);
        res.magic = 0xF0700001;
        for (;;) ;
    }
#endif
#ifdef EXPERIMENT
    {
        DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);
        DL_GPIO_clearPins(PB_PORT, PB_LCD_RST_PIN); delay_ms(10);
        DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN);   delay_ms(150);
#define SNAP(k) do { lcd_read(0x0A, &ex[k][0], 1); lcd_read(0x0C, &ex[k][1], 1); } while (0)
        SNAP(0);                                   /* after reset */
        CMD(0x11); delay_ms(150); SNAP(1);         /* sleep out, alone */
        CMD(0x29); delay_ms(20);  SNAP(2);         /* display on, alone */
        CMD(0x3A, 0x55); delay_ms(20); SNAP(3);    /* 16-bit color, alone */
        CMD(0x28); CMD(0x29); delay_ms(20); SNAP(4);   /* off then on, back to back */
        CMD(0x3A, 0x66); CMD(0x3A, 0x55); delay_ms(20); SNAP(5);  /* 18 then 16, back to back */
        lcd_fill(0, 0, W - 1, H - 1, 0xF800); SNAP(6);  /* after a red fill */
        res.magic = 0xE0E00007;
        for (;;) ;
    }
#endif
#ifdef READBACK
    /* Ask the screen about itself: ID (04), status (09), power mode (0A),
       MADCTL (0B), pixel format (0C), ID4 (D3). Three snapshots:
       0 after hardware reset, 1 after init and display on, 2 after a red fill. */
    {
        static const uint8_t regs[6] = {0x04, 0x09, 0x0A, 0x0B, 0x0C, 0xD3};
        for (int snap = 0; snap < 3; snap++) {
            if (snap == 0) {
                DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);
                DL_GPIO_clearPins(PB_PORT, PB_LCD_RST_PIN); delay_ms(10);
                DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN);   delay_ms(150);
            } else if (snap == 1) {
                lcd_init();
            } else {
                lcd_fill(0, 0, W - 1, H - 1, 0xF800);
            }
            for (int r = 0; r < 6; r++) lcd_read(regs[r], rb[snap][r], 6);
        }
        res.magic = 0xDEAD0000 | 0x3;
        for (;;) ;
    }
#endif
#ifdef DIAG
    /* Visible wire check, repeats forever. res.magic low byte = step number.
       1: backlight off 2 s (2nd blue, LCD_BL)
       2: screen held in reset 2 s, should go blank (grey, LCD_RST)
       3: wake and display on with no init: dots come back if commands land
          (1st green SCLK, 1st blue MOSI, purple LCD_CS, brown LCD_DC)
       4: full init, then red, green, blue, 1 s each */
    for (;;) {
        res.magic = 0xD1A60001;
        DL_GPIO_clearPins(PA_PORT, PA_LCD_BL_PIN); delay_ms(2000);
        DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);   delay_ms(1000);
        res.magic = 0xD1A60002;
        DL_GPIO_clearPins(PB_PORT, PB_LCD_RST_PIN); delay_ms(2000);
        DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN);   delay_ms(1000);
        res.magic = 0xD1A60003;
        CMD(0x11); delay_ms(120); CMD(0x29); delay_ms(2000);
        res.magic = 0xD1A60004;
        lcd_init();
        lcd_fill(0, 0, W - 1, H - 1, 0xF800); delay_ms(1000);
        lcd_fill(0, 0, W - 1, H - 1, 0x07E0); delay_ms(1000);
        lcd_fill(0, 0, W - 1, H - 1, 0x001F); delay_ms(1000);
    }
#endif
    res.magic = 0xD15B1A00;
    res.spi_hz = CPUCLK_FREQ / (2 * (SPI_SCR + 1));
    DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);   /* backlight on */
    lcd_init();
    res.magic = 0xD15B1A01;

    static const uint16_t fills[5] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    for (int i = 0; i < 5; i++) {
        uint32_t c0 = now_cycles();
        lcd_fill(0, 0, W - 1, H - 1, fills[i]);
        res.fill_us[i] = us_since(c0);
        delay_ms(400);
    }
    res.magic = 0xD15B1A02;

    static const uint16_t bars[8] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000};
    uint32_t c0 = now_cycles();
    for (int i = 0; i < 8; i++) lcd_fill(i * 40, 0, i * 40 + 39, H - 1, bars[i]);
    res.bars_us = us_since(c0);
    res.magic = 0xD15B1A03;

    DL_GPIO_clearPins(PB_PORT, PB_TP_RST_PIN); delay_ms(10);
    DL_GPIO_setPins(PB_PORT, PB_TP_RST_PIN);   delay_ms(300);
    res.tp_int_idle = DL_GPIO_readPins(PB_PORT, PB_TP_INT_PIN) ? 1 : 0;
    uint8_t v;
    {   /* is anything driving the lines? read them as plain inputs, pulled each way */
        uint32_t pr = 0;
        for (int k = 0; k < 2; k++) {
            uint32_t pull = k ? DL_GPIO_RESISTOR_PULL_DOWN : DL_GPIO_RESISTOR_PULL_UP;
            DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE, pull,
                DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
            DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE, pull,
                DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
            delay_ms(2);
            uint32_t d = DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_3 | DL_GPIO_PIN_2);
            pr |= (((d & DL_GPIO_PIN_3) ? 1 : 0) | ((d & DL_GPIO_PIN_2) ? 2 : 0)) << (2 * k);
        }
        res.line_probe = pr;
        /* drive each low as a push-pull output and read it back */
        DL_GPIO_initDigitalOutput(GPIO_TP_I2C_IOMUX_SDA);
        DL_GPIO_initDigitalOutput(GPIO_TP_I2C_IOMUX_SCL);
        DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE,
            DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE,
            DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        uint32_t both = DL_GPIO_PIN_3 | DL_GPIO_PIN_2, dp = 0;
        DL_GPIO_clearPins(GPIOB, both);
        DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_3); delay_cycles(320);
        if (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_3)) dp |= 1;
        DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_3);
        DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_2); delay_cycles(320);
        if (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_2)) dp |= 2;
        DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_2);
        res.drive_probe = dp;
        /* bit-banged I2C, open drain: "low" = output enabled at 0, "high" = released */
#define SDA_L() DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_3)
#define SDA_H() DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_3)
#define SCL_L() DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_2)
#define SCL_H() DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_2)
#define HALF()  delay_cycles(160)   /* 5 us: 100 kHz */
        SDA_H(); SCL_H(); HALF();
        SDA_L(); HALF(); SCL_L(); HALF();            /* start */
        uint8_t a = TP_ADDR << 1;                      /* write */
        for (int b = 7; b >= 0; b--) {
            if (a & (1 << b)) SDA_H(); else SDA_L();
            HALF(); SCL_H(); HALF(); SCL_L();
        }
        SDA_H(); HALF(); SCL_H(); HALF();
        res.bb_ack = DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_3) ? 0 : 1;   /* ACK pulls SDA low */
        SCL_L(); HALF(); SDA_L(); HALF(); SCL_H(); HALF(); SDA_H(); HALF();   /* stop */
        delay_ms(2);
    }
    uint32_t din = DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_3 | DL_GPIO_PIN_2);
    res.lines_idle = ((din & DL_GPIO_PIN_3) ? 1 : 0) | ((din & DL_GPIO_PIN_2) ? 2 : 0);
    res.tp_rc = tp_read(0xA8, &v, 1);
    res.i2c_status = DL_I2C_getControllerStatus(TP_I2C_INST);
    res.tp_ack = res.tp_rc == 0 ? 1 : -1;
    if (res.tp_ack == 1) {
        res.tp_vendor = v;
        if (tp_read(0xA3, &v, 1) == 0) res.tp_chip = v;
        if (tp_read(0xA6, &v, 1) == 0) res.tp_fw = v;
    }
    res.magic = 0xD15B1A04;

    for (;;) {
        uint8_t p[5];
        if (res.tp_ack == 1 && tp_read(0x02, p, 5) == 0) {
            if ((p[0] & 0x0F) > 0) {
                int x = ((p[1] & 0x0F) << 8) | p[2], y = ((p[3] & 0x0F) << 8) | p[4];
                res.touches++; res.last_x = x; res.last_y = y;
                if (x >= 3 && x < W - 3 && y >= 3 && y < H - 3)
                    lcd_fill(x - 3, y - 3, x + 3, y + 3, 0xFFFF);
                DL_GPIO_togglePins(PA_PORT, PA_LED1_PIN);
            }
        } else if (res.tp_ack == 1) {
            res.i2c_errors++;
        }
        delay_ms(10);
    }
}
