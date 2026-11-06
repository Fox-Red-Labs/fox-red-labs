/* microSD over SPI for the BOOSTXL-SHARP128 slot, as a FatFs disk driver (read only).
 * The slot shares clock and data lines with the screen. The screen is selected by pin 6
 * (high) and the card by pin 8 (low); only one is ever selected at a time.
 * FatFs itself is ChaN's, unmodified, in third-party/fatfs/. */
#ifndef SHARP_SD_H
#define SHARP_SD_H
#include <stdint.h>
#include "sharp_lcd.h"

/* ---- Port interface, in addition to the screen's ---- */
void     sharp_port_sd_cs(int level);        /* BoosterPack pin 8: 0 selects the card */
int      sharp_port_sd_present(void);        /* pin 17 card detect: 1 = a card is in (1 if the host has no detect wire) */
void     sharp_port_spi_set_hz(uint32_t hz); /* change the SPI clock (0.4 MHz to 20 MHz) */
uint32_t sharp_port_millis(void);            /* free-running milliseconds */

#ifndef SHARP_SD_FAST_HZ
#define SHARP_SD_FAST_HZ 8000000u            /* clock after the card is initialized */
#endif
#define SHARP_LCD_HZ 1000000u                /* the panel's top rate is 1.1 MHz */

/* Bring the card up (400 kHz init, then SHARP_SD_FAST_HZ). 0 on success. Leaves the SPI
 * at the card's clock: call sharp_sd_for_lcd() before the next screen update. */
int  sharp_sd_init(void);
void sharp_sd_for_lcd(void);                 /* SPI back to 1 MHz for the panel */
void sharp_sd_for_card(void);                /* SPI to the card's clock */
/* Card facts after init */
int      sharp_sd_is_sdhc(void);
uint32_t sharp_sd_sectors(void);             /* 0 if the CSD could not be read */

/* Read cnt bytes starting ofs bytes into sector lba (buf NULL discards). 0 on success. */
int  sharp_sd_read_part(uint8_t *buf, uint32_t lba, uint32_t ofs, uint32_t cnt);

/* Read `count` 512-byte sectors starting at `lba`. 0 on success. */
int  sharp_sd_read(uint8_t *buf, uint32_t lba, uint32_t count);

#endif
