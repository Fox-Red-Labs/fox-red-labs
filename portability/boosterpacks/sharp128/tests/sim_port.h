/* Mock SPI bus, simulated SD card and panel recorder shared by the host simulations. Define SIM_DONE() first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "../sharp_lcd.h"
#include "../sharp_sd.h"
#include "../sharp_port.h"

static FILE *img;
static uint64_t vt_ns;            /* virtual time */
static uint32_t spi_hz = 1000000;
static jmp_buf done;
static int cs_lcd, cs_sd_low;
static uint8_t lcd[1 << 22]; static uint32_t lcd_n;
static uint32_t sessions, sess_at[1 << 20];

/* ---- simulated card ---- */
static uint8_t resp[2048]; static int rpos, rlen;
static uint8_t cbuf[6]; static int cn;
static int app, inited, acmd_n, streaming; static uint32_t next_lba;
static uint32_t card_cmds, card_blocks;

static void q(const uint8_t *p, int n) { memcpy(resp + rlen, p, n); rlen += n; }
static void q1(uint8_t b) { q(&b, 1); }
static void queue_block(uint32_t lba)
{
    uint8_t blk[512], z[2] = {0, 0};
    fseek(img, (long)lba * 512, SEEK_SET);
    if (fread(blk, 1, 512, img) != 512) memset(blk, 0, 512);
    q1(0xFE); q(blk, 512); q(z, 2); card_blocks++;
}
static void run_cmd(void)
{
    uint8_t c = cbuf[0] & 0x3F;
    uint32_t arg = ((uint32_t)cbuf[1] << 24) | (cbuf[2] << 16) | (cbuf[3] << 8) | cbuf[4];
    card_cmds++;
    if (c != 12) { rpos = rlen = 0; }
    q1(0xFF);
    if (app && c == 41) { app = 0; q1(acmd_n++ ? 0x00 : 0x01); if (acmd_n > 1) inited = 1; return; }
    app = 0;
    switch (c) {
    case 0:  q1(0x01); break;
    case 8:  { uint8_t r[5] = {0x01, 0, 0, 1, 0xAA}; q(r, 5); } break;
    case 55: app = 1; q1(inited ? 0x00 : 0x01); break;
    case 58: { uint8_t r[5] = {0x00, 0x40, 0xFF, 0x80, 0}; q(r, 5); } break;
    case 16: q1(0x00); break;
    case 9:  { uint8_t csd[16] = {0x40, 0, 0x0E, 0, 0x5B, 0x59, 0, 0, 0, 31, 0x7F, 0x80, 0x0A, 0x40, 0, 0x01}; q1(0x00); q1(0xFF); q1(0xFE); q(csd, 16); q1(0); q1(0); } break;
    case 17: q1(0x00); q1(0xFF); queue_block(arg); break;
    case 18: q1(0x00); q1(0xFF); queue_block(arg); streaming = 1; next_lba = arg + 1; break;
    case 12: rpos = rlen = 0; streaming = 0; q1(0xFF); q1(0x00); q1(0); q1(0); q1(0xFF); break;
    default: q1(0x04);
    }
}
static uint8_t card_byte(uint8_t in)
{
    uint8_t out;
    if (rpos < rlen) out = resp[rpos++];
    else if (streaming) { rpos = rlen = 0; queue_block(next_lba++); out = resp[rpos++]; }
    else out = 0xFF;
    if (cn == 0 && (in & 0xC0) == 0x40) cbuf[cn++] = in;
    else if (cn) { cbuf[cn++] = in; if (cn == 6) { cn = 0; run_cmd(); } }
    return out;
}

/* ---- port ---- */
void sharp_port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint32_t n)
{
    uint32_t i;
    vt_ns += (uint64_t)n * 8 * 1000000000ull / spi_hz;
    for (i = 0; i < n; i++) {
        uint8_t in = tx ? tx[i] : 0xFF, out = 0xFF;
        if (cs_sd_low) out = card_byte(in);
        else if (cs_lcd) { if (lcd_n < sizeof lcd) lcd[lcd_n++] = in; }
        if (rx) rx[i] = out;
    }
}
void sharp_port_lcd_cs(int l) { if (l && !cs_lcd) { if (sessions < (1 << 20)) sess_at[sessions] = lcd_n; sessions++; } cs_lcd = l; }
void sharp_port_lcd_power(int l) { (void)l; }
void sharp_port_lcd_disp(int l) { (void)l; }
void sharp_port_delay_us(uint32_t us) { vt_ns += (uint64_t)us * 1000; }
void sharp_port_sd_cs(int l) { cs_sd_low = !l; if (l) { rpos = rlen = 0; cn = 0; streaming = 0; } }
int  sharp_port_sd_present(void) { return 1; }
void sharp_port_spi_set_hz(uint32_t hz) { spi_hz = hz; vt_ns += 2000; }
void sharp_port_init(void) { }
uint32_t sharp_port_spi_hz(void) { return spi_hz; }
void sharp_port_stack_paint(void) { }
uint32_t sharp_port_stack_free(void) { return 0; }
uint32_t sharp_port_micros(void) { vt_ns += 1000; return (uint32_t)(vt_ns / 1000); }
uint32_t sharp_port_millis(void)
{
    vt_ns += 1000;
    if (SIM_DONE()) longjmp(done, 1);      /* the program reached its hold loop */
    return (uint32_t)(vt_ns / 1000000);
}

