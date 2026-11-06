/* What a board supplies to the BOOSTXL-AUDIO app: pins as functions, a sample tick, a microphone ADC and a console. */
#ifndef BOARD_AUDIO_H
#define BOARD_AUDIO_H
#include <stdint.h>

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 8000u   /* a board build may raise it (the LP-MSPM33C321A show runs at 16 kHz) */
#endif

void board_init(void);                 /* after the vendor init: power the ADC, start nothing else */
void board_sync(int level);            /* DAC8311 SYNC, BoosterPack pin 12 */
void board_sclk(int level);            /* DAC8311 SCLK, pin 7 */
void board_din(int level);             /* DAC8311 DIN, pin 15 */
void board_amp(int mode);              /* amplifier shutdown line, pin 13: 0 = input (floating), 1 = drive low, 2 = drive high */
void board_mic_power(int on);          /* microphone supply, pin 5 */
void board_adc_start(void);            /* start one microphone conversion, pin 6 */
uint16_t board_adc_read(void);         /* the last finished conversion, 12-bit code */
void board_tick_enable(void);          /* from now on call app_tick() SAMPLE_RATE times a second */
void board_uart_put(uint8_t c);
int board_uart_get(void);              /* -1 when nothing waits */
void board_delay_ms(uint32_t ms);

void app_tick(void);                   /* the sample interrupt body, in audio_app.c */
void app_main(void);                   /* the command loop, in audio_app.c; never returns */

#endif
