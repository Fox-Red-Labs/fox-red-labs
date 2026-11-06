/* Sharp LS013B7DH03 panel protocol. See sharp_lcd.h. */
#include "sharp_lcd.h"

/* Command byte, in the panel's least-significant-bit-first terms. */
#define CMD_WRITE 0x01
#define CMD_VCOM  0x02
#define CMD_CLEAR 0x04

#define CS_SETUP_US 6   /* panel: select high at least 6 us before the first clock */
#define CS_HOLD_US  2   /* and low again no sooner than 2 us after the last */

static uint8_t  vcom;           /* VCOM bit for the next command, flips after each one */
static uint32_t last_cmd_ms;
static uint32_t bytes_sent;

static uint8_t rev8(uint8_t b)
{
    b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

static void send(const uint8_t *p, uint32_t n)
{
    bytes_sent += n;
    sharp_port_spi_xfer(p, 0, n);
}

static uint8_t mode_byte(uint8_t cmd)
{
    uint8_t m = (uint8_t)(cmd | (vcom ? CMD_VCOM : 0));
    vcom ^= 1;
    return rev8(m);
}

static void select_panel(void)
{
    sharp_port_lcd_cs(1);
    sharp_port_delay_us(CS_SETUP_US);
}

static void release_panel(void)
{
    sharp_port_delay_us(CS_HOLD_US);
    sharp_port_lcd_cs(0);
}

void sharp_power(int on) { sharp_port_lcd_power(on); }
void sharp_blank(int on) { sharp_port_lcd_disp(!on); }

void sharp_clear_panel(void)
{
    uint8_t c[2];
    c[0] = mode_byte(CMD_CLEAR);
    c[1] = 0;
    select_panel();
    send(c, 2);
    release_panel();
}

void sharp_init(void)
{
    vcom = 0;
    last_cmd_ms = 0;
    bytes_sent = 0;
    sharp_port_lcd_cs(0);
    sharp_port_lcd_disp(0);
    sharp_port_lcd_power(1);
    sharp_port_delay_us(2000);      /* supply settles before the first command */
    sharp_clear_panel();
    sharp_port_lcd_disp(1);
}

void sharp_update_lines(const uint8_t *rows, uint8_t y, uint8_t n)
{
    uint8_t line[2 + SHARP_ROW_BYTES];   /* address, 16 data bytes, 8 trailing zero bits */
    uint8_t m = mode_byte(CMD_WRITE);
    uint8_t zero = 0;
    uint8_t i, k;

    select_panel();
    send(&m, 1);
    for (i = 0; i < n; i++) {
        line[0] = rev8((uint8_t)(y + i + 1));      /* panel rows are numbered 1 to 128 */
        for (k = 0; k < SHARP_ROW_BYTES; k++)
            line[1 + k] = rev8(rows[(uint16_t)i * SHARP_ROW_BYTES + k]);
        line[1 + SHARP_ROW_BYTES] = 0;
        send(line, sizeof line);
    }
    send(&zero, 1);                                  /* final 8 bits of the 16-bit trailer */
    release_panel();
}

void sharp_update_full(const uint8_t *fb)
{
    sharp_update_lines(fb, 0, SHARP_H);
}

void sharp_vcom_tick(uint32_t now_ms)
{
    uint8_t c[2];
    if ((uint32_t)(now_ms - last_cmd_ms) < 1000u)
        return;
    c[0] = mode_byte(0);            /* no write, no clear: only the VCOM bit */
    c[1] = 0;
    select_panel();
    send(c, 2);
    release_panel();
    last_cmd_ms = now_ms;
}

uint32_t sharp_stat_bytes(void) { return bytes_sent; }
