/* SSD1306 text driver, TI I2C driver (blocking). Address 0x3C (the ELEGOO module's default). */
#include <string.h>
#include <ti/drivers/I2C.h>
#include "ti_drivers_config.h"
#include "oled.h"
#include "font5x7.h"

static uint8_t OLED_ADDR = 0x3C;      /* 0x3C on the ELEGOO module; 0x3D if its address pad is moved */
uint8_t oled_found_mask[16];           /* bus scan, one bit per 7-bit address, read by the status line */
int oled_i2c_open_ok = -1;
static I2C_Handle i2c;
static bool present;
static char text[OLED_LINES][OLED_COLS + 1];
static uint8_t tx[129];

static bool xfer(const uint8_t *buf, size_t n)
{
    I2C_Transaction t = {0};
    t.targetAddress = OLED_ADDR;
    t.writeBuf = (void *)buf;
    t.writeCount = n;
    return I2C_transfer(i2c, &t);
}

static bool cmd(uint8_t c) { uint8_t b[2] = {0x00, c}; return xfer(b, 2); }

static void drawLine(uint8_t line)
{
    memset(tx, 0, sizeof tx);
    tx[0] = 0x40;
    uint8_t *col = &tx[1];
    for (uint8_t i = 0; i < OLED_COLS; i++)
    {
        char ch = text[line][i];
        if (ch < 32 || ch > 126) ch = '?';
        memcpy(col, font5x7[ch - 32], 5);
        col += 6;                                  /* one blank column between characters */
    }
    cmd(0xB0 | line); cmd(0x00); cmd(0x10);        /* page, column 0 */
    xfer(tx, sizeof tx);
}

bool Oled_present(void) { return present; }

bool Oled_init(void)
{
    static const uint8_t seq[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x02,
                                  0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    I2C_init();
    I2C_Params p;
    I2C_Params_init(&p);
    p.bitRate = I2C_100kHz;
    i2c = I2C_open(CONFIG_I2C_OLED, &p);
    oled_i2c_open_ok = i2c ? 1 : 0;
    if (!i2c) return false;
    for (uint8_t a = 0x08; a < 0x78; a++) {                 /* scan: one-byte read, an ACK means a device */
        uint8_t r; I2C_Transaction t = {0}; t.targetAddress = a; t.readBuf = &r; t.readCount = 1;
        if (I2C_transfer(i2c, &t)) oled_found_mask[a >> 3] |= (uint8_t)(1u << (a & 7));
    }
    if (oled_found_mask[0x3D >> 3] & (1u << (0x3D & 7)) && !(oled_found_mask[0x3C >> 3] & (1u << (0x3C & 7)))) OLED_ADDR = 0x3D;
    present = true;
    for (unsigned i = 0; i < sizeof seq; i++)
        if (!cmd(seq[i])) { present = false; break; }   /* first NAK: no display wired */
    if (present) Oled_clear();
    return present;
}

void Oled_clear(void)
{
    for (uint8_t l = 0; l < OLED_LINES; l++) Oled_setLine(l, "");
}

void Oled_setLine(uint8_t line, const char *s)
{
    if (line >= OLED_LINES) return;
    size_t n = strlen(s);
    if (n > OLED_COLS) n = OLED_COLS;
    memset(text[line], ' ', OLED_COLS);
    memcpy(text[line], s, n);
    text[line][OLED_COLS] = 0;
    if (present) drawLine(line);
}
