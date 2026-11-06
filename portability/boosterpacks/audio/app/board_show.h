/* What a board supplies to the Halloween show on top of board_audio.h: the ring's SPI stream, two board LEDs, a
 * cycle counter and the boot interlock's answer. */
#ifndef BOARD_SHOW_H
#define BOARD_SHOW_H
#include <stdint.h>

#define SHOW_MODE_PLAY 1     /* Audio pack found (or override button held): the DAC lines and amplifier are driven */
#define SHOW_MODE_SILENT 0   /* anything else: the four pack lines stay inputs; lights only */

int board_show_mode(void);                          /* decided once at boot, before any pack line is driven */
void board_show_reason(void (*puts_)(const char *)); /* prints what the interlock read and why it chose */
void board_ring_send(const uint8_t *buf, uint32_t n); /* start one ring frame (returns at once; DMA does the rest) */
int board_ring_busy(void);
void board_ring_info(void (*puts_)(const char *));  /* prints the ring SPI set-up as the chip holds it */
void board_leds(int red, int green);                /* 1 = on */
void board_rgb(const uint8_t rgb[3]);               /* one RGB LED, 0..255 a channel (PWM); a no-op without one */
uint32_t board_rgb_duty(void);                      /* the PWM duty now set, red << 16 | green << 8 | blue, of 32 */
uint32_t board_cycles(void);                        /* free-running CPU cycle counter */
uint32_t board_cpu_hz(void);
void board_buttons(int *a, int *b);                 /* 1 = pressed */
uint32_t board_seed(void);                          /* a different number each power-up */

#endif
