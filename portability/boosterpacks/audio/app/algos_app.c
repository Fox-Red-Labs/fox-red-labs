/* BOOSTXL-AUDIO algorithms app (wave 3): tuner, spectrum bars, DTMF decoder, noise gate and echo on the pack's microphone,
 * with the DAC path as the speaker. One image, driven over the backchannel UART (115200, one key per command), same board layer
 * as audio_app.c. Build with the "algos" variant of each board build.sh.
 *   0 silence   1 tone 440 Hz   2 tone 1000 Hz   3 tone 110 Hz   4 tone 82.41 Hz (low E)
 *   t capture 384 samples and print the pitch (YIN): frequency, nearest note, error in cents
 *   s capture 128 samples and print the 16 spectrum bars (0 to 127)
 *   d play the next DTMF digit of "123A456B789C*0#D" and decode it from the microphone
 *   g live microphone -> noise gate -> echo -> DAC (hold 0 to stop); keep the speaker away from the microphone
 *   b benchmark: cycles per call of each algorithm on a synthetic frame (DWT cycle counter; 0 if the board layer has none)
 *   a / A / z amplifier line low (enabled) / high / floating   m / M microphone power on / off   v / V volume   p state */
#include <stdint.h>
#include "board_audio.h"
#include "ba_dac8311.h"
#include "ba_tone.h"
#include "ba_yin.h"
#include "ba_fft.h"
#include "ba_dtmf.h"
#include "ba_gate.h"
#include "ba_amath.h"

__attribute__((weak)) uint32_t board_cycles(void) { return 0; }

#define CAP_MAX 1280u
enum { M_SILENT, M_TONE, M_DTMF, M_LIVE };

static const ba_hal_t hal = { board_sync, board_sclk, board_din };
static volatile int mode = M_SILENT;
static volatile int ready;
static volatile unsigned vol = 12;
static ba_tone_t tone, tone2;
static uint16_t cap[CAP_MAX];
static volatile uint32_t cap_n = CAP_MAX, cap_want;
static int amp_mode, mic_on;
static ba_gate_t gate;
static ba_echo_t echo;
static int16_t echo_line[2000]; /* 250 ms at 8 kHz */
static int16_t dc;              /* running mean of the microphone for the live path, Q0 */
static int32_t dc_acc;
static volatile uint32_t dtmf_left;

void app_tick(void)
{
    if (!ready) return;
    int16_t s = 0;
    uint16_t m = board_adc_read();
    if (mode == M_TONE) s = ba_tone_next(&tone);
    else if (mode == M_DTMF) { if (dtmf_left) { dtmf_left--; s = (int16_t)((ba_tone_next(&tone) >> 1) + (ba_tone_next(&tone2) >> 1)); } }
    else if (mode == M_LIVE) {
        dc_acc += (((int32_t)m << 8) - dc_acc) >> 9; dc = (int16_t)(dc_acc >> 8);
        int32_t x = ((int32_t)m - dc) * 8;
        s = ba_echo_process(&echo, ba_gate_process(&gate, (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x)));
    }
    ba_dac_write16(&hal, ba_dac_word(ba_dac_code_from_s16(s, vol), BA_DAC_PD_NORMAL));
    if (cap_n < cap_want) cap[cap_n++] = m;
    board_adc_start();
}

static void puts_(const char *s) { while (*s) board_uart_put((uint8_t)*s++); }
static void putu(uint32_t v)
{
    char b[11]; int i = 0;
    if (!v) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i) board_uart_put((uint8_t)b[--i]);
}
static void puti(int32_t v) { if (v < 0) { board_uart_put('-'); v = -v; } putu((uint32_t)v); }

static void capture(uint32_t n) { cap_want = n; cap_n = 0; while (cap_n < n) { } cap_want = 0; cap_n = CAP_MAX; }
static void to_signed(int16_t *x, uint32_t n)
{
    int32_t sum = 0; for (uint32_t i = 0; i < n; i++) sum += cap[i];
    int32_t mean = sum / (int32_t)n;
    for (uint32_t i = 0; i < n; i++) { int32_t v = ((int32_t)cap[i] - mean) * 8; x[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
}

static void set_tone(unsigned hz_x100)
{
    ba_tone_init(&tone, 0, SAMPLE_RATE); tone.step = (uint16_t)(((uint64_t)hz_x100 << 16) / (SAMPLE_RATE * 100u));
    mode = M_TONE;
}

static void do_tuner(void)
{
    static int16_t x[BA_YIN_FRAME];
    capture(BA_YIN_FRAME); to_signed(x, BA_YIN_FRAME);
    ba_yin_result_t r; uint32_t c0 = board_cycles(); ba_yin(x, SAMPLE_RATE, 4915, &r); uint32_t c1 = board_cycles();
    int midi = 0, c10 = 0; ba_pitch_note(r.freq_x256, &midi, &c10);
    puts_("PITCH voiced="); putu(r.voiced); puts_(" hz_x100="); putu((uint32_t)(((uint64_t)r.freq_x256 * 100u) >> 8));
    puts_(" midi="); puti(midi); puts_(" cents_x10="); puti(c10); puts_(" cmnd_q15="); putu(r.cmnd_q15); puts_(" cycles="); putu(c1 - c0); puts_("\r\n");
}

static void do_spectrum(void)
{
    static int16_t x[128]; static uint32_t mag[64]; uint8_t bar[BA_BARS];
    capture(128); to_signed(x, 128);
    uint32_t c0 = board_cycles(); ba_spectrum_mag(x, mag); ba_spectrum_bars(mag, bar); uint32_t c1 = board_cycles();
    puts_("BARS");
    for (int i = 0; i < BA_BARS; i++) { board_uart_put(' '); putu(bar[i]); }
    puts_(" cycles="); putu(c1 - c0); puts_("\r\n");
}

static void do_dtmf(void)
{
    static const char seq[] = "123A456B789C*0#D"; static unsigned idx;
    static const uint16_t rowhz[4] = { 697, 770, 852, 941 }, colhz[4] = { 1209, 1336, 1477, 1633 };
    char want = seq[idx & 15]; unsigned r = (idx & 15) / 4, c = (idx & 15) % 4; idx++;
    ba_tone_init(&tone, rowhz[r], SAMPLE_RATE); ba_tone_init(&tone2, colhz[c], SAMPLE_RATE);
    dtmf_left = 205u * 6u; mode = M_DTMF;
    static int16_t x[205 * 6]; /* 2.4 KB */
    capture(205u * 6u); to_signed(x, 205u * 6u);
    ba_dtmf_t d; ba_dtmf_init(&d); char got = 0; uint32_t cyc = 0, blocks = 0;
    for (uint32_t i = 0; i < 205u * 6u; i++) {
        uint32_t c0 = board_cycles();
        if (ba_dtmf_push(&d, x[i])) { cyc = board_cycles() - c0; blocks++; char z = ba_dtmf_stream(&d); if (z) got = z; }
    }
    mode = M_SILENT;
    puts_("DTMF sent="); board_uart_put((uint8_t)want); puts_(" decoded="); board_uart_put((uint8_t)(got ? got : '-')); puts_(" blocks="); putu(blocks);
    puts_(" block_cycles="); putu(cyc); puts_("\r\n");
}

static void do_bench(void)
{
    static int16_t x[BA_YIN_FRAME]; static uint32_t mag[64]; uint8_t bar[BA_BARS];
    for (int i = 0; i < BA_YIN_FRAME; i++) x[i] = (int16_t)(8000 * ((i * 8192u >> 8) & 0x100 ? -1 : 1)); /* a square wave, 16 samples a period */
    uint32_t c0, c1; ba_yin_result_t r;
    c0 = board_cycles(); ba_yin(x, SAMPLE_RATE, 4915, &r); c1 = board_cycles(); puts_("BENCH yin384 cycles="); putu(c1 - c0); puts_("\r\n");
    c0 = board_cycles(); ba_spectrum_mag(x, mag); ba_spectrum_bars(mag, bar); c1 = board_cycles(); puts_("BENCH spectrum128 cycles="); putu(c1 - c0); puts_("\r\n");
    ba_dtmf_t d; ba_dtmf_init(&d);
    c0 = board_cycles(); for (int i = 0; i < 205; i++) ba_dtmf_push(&d, x[i]); c1 = board_cycles(); puts_("BENCH dtmf205 cycles="); putu(c1 - c0); puts_("\r\n");
    ba_gate_t g; ba_gate_init(&g, 800, 400, 400, 2, 30, SAMPLE_RATE); ba_echo_t e; ba_echo_init(&e, echo_line, 2000, 12000, 14000);
    c0 = board_cycles(); for (int i = 0; i < 256; i++) (void)ba_echo_process(&e, ba_gate_process(&g, x[i])); c1 = board_cycles();
    puts_("BENCH gate_echo256 cycles="); putu(c1 - c0); puts_("\r\n");
    c0 = board_cycles(); for (int i = 0; i < 256; i++) { ba_dac_write16(&hal, ba_dac_word(8192u + (unsigned)(i & 15), 0)); } c1 = board_cycles();
    puts_("BENCH dac_frame256 cycles="); putu(c1 - c0); puts_("\r\n");
}

static void status(void)
{
    static const char *mn[] = { "silent", "tone", "dtmf", "live" };
    puts_("STATE mode="); puts_(mn[mode]); puts_(" amp="); puts_(amp_mode == 0 ? "float" : amp_mode == 1 ? "low" : "high");
    puts_(" mic="); puts_(mic_on ? "on" : "off"); puts_(" vol="); putu(vol); puts_("\r\n");
}

void app_main(void)
{
    board_init();
    ba_dac_write16(&hal, ba_dac_word(BA_DAC_MIDSCALE, BA_DAC_PD_NORMAL));
    board_amp(0); board_mic_power(0);
    ba_gate_init(&gate, 800, 400, 400, 2, 30, SAMPLE_RATE);
    ba_echo_init(&echo, echo_line, 2000, 12000, 14000);
    ready = 1;
    board_tick_enable();
    puts_("\r\nBOOSTXL-AUDIO algorithms app, 8 kHz\r\n");
    status();
    for (;;) {
        int c = board_uart_get();
        if (c < 0) continue;
        switch (c) {
        case '0': mode = M_SILENT; break;
        case '1': set_tone(44000); break;
        case '2': set_tone(100000); break;
        case '3': set_tone(11000); break;
        case '4': set_tone(8241); break;
        case 't': do_tuner(); continue;
        case 's': do_spectrum(); continue;
        case 'd': do_dtmf(); continue;
        case 'g': ba_gate_init(&gate, 800, 400, 400, 2, 30, SAMPLE_RATE); ba_echo_init(&echo, echo_line, 2000, 12000, 14000); mode = M_LIVE; break;
        case 'b': do_bench(); continue;
        case 'a': amp_mode = 1; board_amp(1); break;
        case 'A': amp_mode = 2; board_amp(2); break;
        case 'z': amp_mode = 0; board_amp(0); break;
        case 'm': mic_on = 1; board_mic_power(1); break;
        case 'M': mic_on = 0; board_mic_power(0); break;
        case 'v': if (vol) vol--; break;
        case 'V': if (vol < 16) vol++; break;
        case 'p': break;
        default: continue;
        }
        status();
    }
}
