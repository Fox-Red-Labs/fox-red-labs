/* WS2812 frame as SPI bytes: each LED bit becomes three SPI bits at about 2.4 MHz (1 = 110, 0 = 100), so a
 * hardware SPI port with DMA makes the 800 kHz waveform with no CPU time and no counted instructions.
 * At 2.381 MHz (100 MHz / 42, the LP-AM13E230 setting): bit 1.26 us, high 0.42 us for a 0 and 0.84 us for a 1
 * (WS2812B: T0H 0.40, T1H 0.80, each +/-0.15 us; bit 1.25 +/-0.6 us). Color order on the wire is G, R, B. */
#ifndef BA_WS2812_H
#define BA_WS2812_H
#include <stdint.h>

#define BA_WS_BYTES_PER_LED 9u
#define BA_WS_TAIL 2u /* two zero bytes: the line ends low */
#define BA_WS_FRAME_BYTES(n) ((n) * BA_WS_BYTES_PER_LED + BA_WS_TAIL)

/* rgb[i] = {r, g, b}; out holds BA_WS_FRAME_BYTES(n) bytes */
void ba_ws2812_encode(const uint8_t (*rgb)[3], unsigned n, uint8_t *out);

#endif
