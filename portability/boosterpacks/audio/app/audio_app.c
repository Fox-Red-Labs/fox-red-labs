/* BOOSTXL-AUDIO test app: one image, driven over the backchannel UART (115200, one key per command).
 *   0 silence (DAC at mid-scale)    1 1 kHz tone, correct 16-bit frames    2 the tune "Fox Run", correct frames, looping
 *   8 1 kHz tone, but two 8-bit frames with SYNC raised between them (the E237 hypothesis)
 *   a amplifier line driven low     A amplifier line driven high           z amplifier line floating (the pack's pull-up decides)
 *   m microphone power on           M microphone power off                 c capture 2048 microphone samples and print them
 *   v / V volume down / up (0 to 16)    p print the state
 * Build with -DAUTOPLAY to start the tune at power-up with the amplifier line driven low. */
#include <stdint.h>
#include "board_audio.h"
#include "ba_dac8311.h"
#include "ba_chip.h"
#include "ba_tone.h"
#include "ba_mic.h"
#include "fox_run.h"

#define CAP_N 2048u

enum { M_SILENT, M_TONE16, M_TUNE16, M_TONE_SPLIT8 };

static const ba_hal_t hal = { board_sync, board_sclk, board_din };
static const ba_song_t song = { (const uint8_t (*)[4])fox_run_rows, sizeof fox_run_rows / 4, (uint32_t)SAMPLE_RATE * 30u / FOX_RUN_TEMPO, FOX_RUN_VOL };

static volatile int mode = M_SILENT;
static volatile int ready;
static volatile unsigned vol = 12;
static ba_tone_t tone;
static ba_chip_t chip;
static uint16_t cap[CAP_N];
static volatile uint32_t cap_n = CAP_N; /* CAP_N means idle */
static volatile uint32_t ticks;
static int amp_mode = 0, mic_on = 0;

void app_tick(void)
{
    if (!ready) return;
    int16_t s = 0;
    int m = mode;
    if (m == M_TONE16 || m == M_TONE_SPLIT8) s = ba_tone_next(&tone);
    else if (m == M_TUNE16) s = ba_chip_next(&chip);
    uint16_t word = ba_dac_word(ba_dac_code_from_s16(s, vol), BA_DAC_PD_NORMAL);
    if (m == M_TONE_SPLIT8) ba_dac_write_split8(&hal, word); else ba_dac_write16(&hal, word);
    if (cap_n < CAP_N) cap[cap_n++] = board_adc_read();
    board_adc_start();
    ticks++;
}

static void puts_(const char *s) { while (*s) board_uart_put((uint8_t)*s++); }
static void putu(uint32_t v)
{
    char b[11]; int i = 0;
    if (!v) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i) board_uart_put((uint8_t)b[--i]);
}
static void puths(int32_t v) { if (v < 0) { board_uart_put('-'); v = -v; } putu((uint32_t)v); }
static void puthex4(uint16_t v) { for (int i = 12; i >= 0; i -= 4) board_uart_put((uint8_t)"0123456789abcdef"[(v >> i) & 15]); }

static void status(void)
{
    static const char *mn[] = { "silent", "tone16", "tune16", "tone-split8" };
    puts_("STATE mode="); puts_(mn[mode]); puts_(" amp="); puts_(amp_mode == 0 ? "float" : amp_mode == 1 ? "low" : "high");
    puts_(" mic="); puts_(mic_on ? "on" : "off"); puts_(" vol="); putu(vol); puts_(" ticks="); putu(ticks); puts_("\r\n");
}

static void set_mode(int m)
{
    ba_tone_init(&tone, 1000, SAMPLE_RATE);
    ba_chip_init(&chip, &song, SAMPLE_RATE);
    mode = m;
}

static void capture(void)
{
    cap_n = 0;
    while (cap_n < CAP_N) { }
    ba_mic_stats_t st; ba_mic_stats(cap, CAP_N, &st);
    puts_("CAP n="); putu(CAP_N); puts_(" rate="); putu(SAMPLE_RATE); puts_("\r\n");
    for (uint32_t i = 0; i < CAP_N; i += 64) {
        puts_("S ");
        for (uint32_t j = 0; j < 64; j++) puthex4(cap[i + j]);
        puts_("\r\n");
    }
    puts_("STATS mean_x16="); puths(st.mean_x16); puts_(" rms_x16="); putu(st.rms_x16);
    puts_(" min="); putu(st.min); puts_(" max="); putu(st.max); puts_("\r\nEND\r\n");
}

void app_main(void)
{
    board_init();
    ba_dac_write16(&hal, ba_dac_word(BA_DAC_MIDSCALE, BA_DAC_PD_NORMAL));
    ba_tone_init(&tone, 1000, SAMPLE_RATE);
    ba_chip_init(&chip, &song, SAMPLE_RATE);
    board_amp(0);
    board_mic_power(0);
#ifdef AUTOPLAY
    board_amp(1); amp_mode = 1; set_mode(M_TUNE16);
#endif
    ready = 1;
    board_tick_enable();
    puts_("\r\nBOOSTXL-AUDIO app, 8 kHz, DAC8311 on SPI pins 7/12/15\r\n");
    status();
    for (;;) {
        int c = board_uart_get();
        if (c < 0) continue;
        switch (c) {
        case '0': mode = M_SILENT; break;
        case '1': set_mode(M_TONE16); break;
        case '2': set_mode(M_TUNE16); break;
        case '8': set_mode(M_TONE_SPLIT8); break;
        case 'a': amp_mode = 1; board_amp(1); break;
        case 'A': amp_mode = 2; board_amp(2); break;
        case 'z': amp_mode = 0; board_amp(0); break;
        case 'm': mic_on = 1; board_mic_power(1); break;
        case 'M': mic_on = 0; board_mic_power(0); break;
        case 'v': if (vol) vol--; break;
        case 'V': if (vol < 16) vol++; break;
        case 'c': capture(); break;
        case 'p': break;
        default: continue;
        }
        status();
    }
}
