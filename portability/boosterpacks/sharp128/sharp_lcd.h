/* Sharp LS013B7DH03 128 x 128 1-bit memory LCD: panel protocol, portable C.
 * BOOSTXL-SHARP128 (user guide SLAU786). Written for this repository from the panel
 * specification and the user guide; Zephyr's drivers/display/ls0xx.c (Apache-2.0) was
 * read as a cross-check of the byte layout.
 *
 * Screen levels: bit 1 = white (background), bit 0 = black (ink).
 * Row layout: 16 bytes a row, bit 0 of byte 0 is the leftmost pixel. This is also the
 * order the panel clocks pixels in, so a row goes out with no pixel shuffling.
 *
 * The panel wants every byte least significant bit first. Hosts differ, so the port
 * gives this code a plain MSB-first SPI (mode 0) and the code reverses each byte
 * itself. One SPI setup then serves the screen and the microSD card.
 */
#ifndef SHARP_LCD_H
#define SHARP_LCD_H
#include <stdint.h>

#define SHARP_W         128
#define SHARP_H         128
#define SHARP_ROW_BYTES 16
#define SHARP_FB_BYTES  (SHARP_ROW_BYTES * SHARP_H)

/* ---- Port interface: one implementation per host (boards/<board>/port.c) ---- */
/* SPI mode 0, most significant bit first. tx may be NULL (sends 0xFF), rx may be NULL
 * (received bytes dropped). Returns when the last bit has left the pin. */
void sharp_port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint32_t n);
void sharp_port_lcd_cs(int level);      /* BoosterPack pin 6, active high */
void sharp_port_lcd_power(int level);   /* pin 2: the panel runs from this pin */
void sharp_port_lcd_disp(int level);    /* pin 5 DISP: low blanks the screen, memory kept */
void sharp_port_delay_us(uint32_t us);

/* ---- Panel ---- */
void sharp_init(void);                  /* power on, DISP on, panel cleared to white */
void sharp_power(int on);               /* off drops the picture: about 0 uA */
void sharp_blank(int on);               /* on hides the picture, memory kept */
void sharp_clear_panel(void);           /* hardware clear-all command: white */
/* Send a whole 2,048-byte frame (2,306 bytes on the wire). */
void sharp_update_full(const uint8_t *fb);
/* Send n rows (16 bytes each, rows[0] is screen row y, 0-based). One chip-select cycle. */
void sharp_update_lines(const uint8_t *rows, uint8_t y, uint8_t n);
/* The panel needs its VCOM bit flipped at least once a second or the glass is damaged
 * over time. Every command flips it. Call this from the main loop with a millisecond
 * clock: once a second it sends a 2-byte VCOM-only command (also when other updates
 * happened in between, which costs 2 bytes a second and keeps the logic stateless). */
void sharp_vcom_tick(uint32_t now_ms);
/* Test hook: byte count and command count since init (cheap, kept in every build). */
uint32_t sharp_stat_bytes(void);

#endif
