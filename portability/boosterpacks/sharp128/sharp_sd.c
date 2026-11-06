/* microSD over SPI, plus the FatFs disk glue (read only). See sharp_sd.h.
 * Written from the SD Physical Layer simplified specification's SPI-mode chapter. */
#include "sharp_sd.h"

#define CMD0   0
#define CMD8   8
#define CMD9   9
#define CMD12  12
#define CMD16  16
#define CMD17  17
#define CMD18  18
#define CMD55  55
#define CMD58  58
#define ACMD41 (0x80 | 41)

static uint8_t  sdhc, ready;
static uint32_t sectors;
static uint32_t card_hz = SHARP_SD_FAST_HZ;

static uint8_t rx1(void) { uint8_t b; sharp_port_spi_xfer(0, &b, 1); return b; }

static int wait_ready(uint32_t ms)
{
    uint32_t t0 = sharp_port_millis();
    while (rx1() != 0xFF)
        if ((uint32_t)(sharp_port_millis() - t0) > ms) return -1;
    return 0;
}

static void deselect(void) { sharp_port_sd_cs(1); (void)rx1(); }

static int select_card(void)
{
    sharp_port_lcd_cs(0);               /* the screen must not hear the card's traffic */
    sharp_port_sd_cs(0);
    (void)rx1();
    if (wait_ready(500) < 0) { deselect(); return -1; }
    return 0;
}

/* Send a command; returns the R1 byte (0xFF on timeout). The card stays selected. */
static uint8_t cmd(uint8_t c, uint32_t arg)
{
    uint8_t buf[6], r;
    int i;
    if (c & 0x80) {                     /* application command: CMD55 first */
        c &= 0x7F;
        r = cmd(CMD55, 0);
        if (r > 1) return r;
    }
    if (c != 12) { deselect(); if (select_card() < 0) return 0xFF; }
    buf[0] = (uint8_t)(0x40 | c);
    buf[1] = (uint8_t)(arg >> 24); buf[2] = (uint8_t)(arg >> 16);
    buf[3] = (uint8_t)(arg >> 8);  buf[4] = (uint8_t)arg;
    buf[5] = (c == CMD0) ? 0x95 : (c == CMD8) ? 0x87 : 0x01;     /* CRC matters for these two only */
    sharp_port_spi_xfer(buf, 0, 6);
    if (c == 12) (void)rx1();           /* stuff byte after CMD12 */
    for (i = 0; i < 10; i++) { r = rx1(); if (!(r & 0x80)) return r; }
    return 0xFF;
}

int sharp_sd_is_sdhc(void) { return sdhc; }
uint32_t sharp_sd_sectors(void) { return sectors; }
void sharp_sd_for_lcd(void) { sharp_port_spi_set_hz(SHARP_LCD_HZ); }
void sharp_sd_for_card(void) { sharp_port_spi_set_hz(card_hz); }

static int read_block(uint8_t *buf, uint32_t n)
{
    uint32_t t0 = sharp_port_millis();
    uint8_t tok, crc[2];
    do { tok = rx1(); } while (tok == 0xFF && (uint32_t)(sharp_port_millis() - t0) < 200);
    if (tok != 0xFE) return -1;
    sharp_port_spi_xfer(0, buf, n);
    sharp_port_spi_xfer(0, crc, 2);     /* CRC is off in SPI mode; read and ignore */
    return 0;
}

int sharp_sd_init(void)
{
    uint8_t r, ocr[4], csd[16];
    uint32_t t0, i;

    ready = 0; sdhc = 0; sectors = 0;
    sharp_port_lcd_cs(0);
    sharp_port_sd_cs(1);
    sharp_port_spi_set_hz(400000u);
    for (i = 0; i < 10; i++) (void)rx1();             /* 80 clocks with the card deselected */

    if (cmd(CMD0, 0) != 0x01) { deselect(); return -1; }       /* no card, or no answer */
    r = cmd(CMD8, 0x1AA);
    t0 = sharp_port_millis();
    if (r == 0x01) {                                           /* version 2 card */
        sharp_port_spi_xfer(0, ocr, 4);
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) { deselect(); return -2; }
        while (cmd(ACMD41, 1u << 30) != 0x00)
            if ((uint32_t)(sharp_port_millis() - t0) > 1500) { deselect(); return -3; }
        if (cmd(CMD58, 0) != 0x00) { deselect(); return -4; }
        sharp_port_spi_xfer(0, ocr, 4);
        sdhc = (ocr[0] & 0x40) ? 1 : 0;
    } else {                                                   /* version 1 card */
        while (cmd(ACMD41, 0) != 0x00)
            if ((uint32_t)(sharp_port_millis() - t0) > 1500) { deselect(); return -3; }
    }
    if (!sdhc && cmd(CMD16, 512) != 0x00) { deselect(); return -5; }

    if (cmd(CMD9, 0) == 0x00 && read_block(csd, 16) == 0) {
        if ((csd[0] >> 6) == 1) {                               /* CSD version 2 */
            uint32_t cs = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
            sectors = (cs + 1) << 10;
        } else {
            uint32_t cs = ((uint32_t)(csd[6] & 3) << 10) | ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
            uint32_t mult = ((csd[9] & 3) << 1) | (csd[10] >> 7);
            uint32_t bl = csd[5] & 15;
            sectors = (cs + 1) << (mult + 2 + bl - 9);
        }
    }
    deselect();
    ready = 1;
    sharp_port_spi_set_hz(card_hz);
    return 0;
}

int sharp_sd_read(uint8_t *buf, uint32_t lba, uint32_t count)
{
    if (!ready || !count) return -1;
    if (!sdhc) lba <<= 9;
    if (count == 1) {
        if (cmd(CMD17, lba) != 0x00) { deselect(); return -1; }
        if (read_block(buf, 512) < 0) { deselect(); return -1; }
        count = 0;
    } else {
        if (cmd(CMD18, lba) != 0x00) { deselect(); return -1; }
        do {
            if (read_block(buf, 512) < 0) break;
            buf += 512;
        } while (--count);
        (void)cmd(CMD12, 0);
    }
    deselect();
    return count ? -1 : 0;
}

/* Read cnt bytes at byte offset ofs of one sector (buf may be NULL to discard). The whole
 * 512-byte sector is clocked out, so a 1 kB host needs no sector buffer. */
int sharp_sd_read_part(uint8_t *buf, uint32_t lba, uint32_t ofs, uint32_t cnt)
{
    uint32_t t0 = sharp_port_millis();
    uint8_t tok, crc[2];
    if (!ready) return -1;
    if (!sdhc) lba <<= 9;
    if (cmd(CMD17, lba) != 0x00) { deselect(); return -1; }
    do { tok = rx1(); } while (tok == 0xFF && (uint32_t)(sharp_port_millis() - t0) < 200);
    if (tok != 0xFE) { deselect(); return -1; }
    if (ofs) sharp_port_spi_xfer(0, 0, ofs);
    sharp_port_spi_xfer(0, buf, cnt);
    if (512 - ofs - cnt) sharp_port_spi_xfer(0, 0, 512 - ofs - cnt);
    sharp_port_spi_xfer(0, crc, 2);
    deselect();
    return 0;
}

#ifdef SHARP_WITH_PFF
/* ---- Petit FatFs disk driver (read only) ---- */
#include "pff.h"
#include "diskio.h"
DSTATUS disk_initialize(void) { return (ready || sharp_sd_init() == 0) ? 0 : STA_NOINIT; }
DRESULT disk_readp(BYTE *buf, DWORD sector, UINT ofs, UINT cnt)
{
    return sharp_sd_read_part(buf, sector, ofs, cnt) == 0 ? RES_OK : RES_ERROR;
}
DRESULT disk_writep(const BYTE *buf, DWORD sc) { (void)buf; (void)sc; return RES_PARERR; }
#endif

#ifdef SHARP_WITH_FATFS
/* ---- FatFs disk driver (FF_FS_READONLY = 1, FF_FS_NORTC = 1, FF_VOLUMES = 1) ---- */
#include "ff.h"
#include "diskio.h"
DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return ready ? 0 : STA_NOINIT; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; if (!ready && sharp_sd_init() != 0) return STA_NOINIT; return 0; }
DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count)
{
    (void)pdrv;
    return sharp_sd_read(buf, (uint32_t)sector, count) == 0 ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE c, void *buf)
{
    (void)pdrv;
    switch (c) {
    case CTRL_SYNC: return RES_OK;
    case GET_SECTOR_COUNT: *(LBA_t *)buf = sectors; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)buf = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)buf = 1; return RES_OK;
    }
    return RES_PARERR;
}
#endif
