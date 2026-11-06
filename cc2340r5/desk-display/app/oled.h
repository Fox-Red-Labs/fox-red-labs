/* SSD1306 128x64 I2C text driver without a frame buffer: one 8-pixel row (page) per text line.
 * 21 characters of a 5x7 font per line, 8 lines. Needs only 168 bytes of RAM. */
#ifndef OLED_H
#define OLED_H
#include <stdint.h>
#include <stdbool.h>
#define OLED_LINES 8
#define OLED_COLS  21
bool Oled_init(void);                        /* opens I2C and sends the start sequence; false if no display answers */
bool Oled_present(void);
void Oled_setLine(uint8_t line, const char *text);   /* line 0..7, text clipped to 21 chars, blank-padded; draws at once */
void Oled_clear(void);
#endif
