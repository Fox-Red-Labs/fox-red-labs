/* Halloween show: plays the set list (halloween/ba_setlist.c) forever, shuffled, with effect modes, sound effects
 * and light themes, all driven by the synth's own events. Starts at power-up, needs no serial input, never sleeps.
 * Listener: between songs a clap, knock, whistle or shout at the pack's microphone sets off thunder, a creaking door,
 * a ghost wail or a scream with a colored flash (halloween/ba_listen.h); both buttons held 2 s turn it off and on.
 * Controls: two buttons (halloween/ba_ctl.h) or the console keys n (next song), t (theme), f (effects), v (volume),
 * s (shuffle), p (status); for tests without hands, A, L, B, K, X and Y hold button A for 0.2 s or 1 s, B for 0.2 s
 * or 1 s, or both for 0.5 s or 2.2 s through the same button logic; Z runs the listener's self-test clips; T toggles a 1 kHz test tone, G the show's gain (1.0 or the
 * default), M prints the microphone RMS of the last second. Every real button edge prints a BTN line. Scare mode: between songs, once the gap's own sound has finished, a loud clap or shout at
 * the pack's microphone sets off a scream and a lightning burst.
 * Console (115200): banner, the guard's readings, one line per song and per control change, and a STAT line every
 * 10 s with the DAC frame rate measured against the CPU cycle counter, the frame check, ring frames, sample-tick
 * timing and the settings. In silent mode (a DRV8376EVM found at boot) the DAC lines and amplifier are not driven. */
#include <stdint.h>
#include "board_audio.h"
#include "board_show.h"
#include "ba_dac8311.h"
#include "ba_show.h"
#include "ba_lights.h"
#include "ba_ws2812.h"
#include "ba_ctl.h"
#include "ba_tone.h"
#include "ba_listen.h"
#include "ba_testclips.h"
__attribute__((weak)) int board_has_mic(void) { return 1; }   /* the LP-MSPM33C321A says 0: its pin 5 cannot power the microphone */

#define GAP_MS 6000u       /* 6 s between songs: a scare sound, then a quiet window for the listener */
#define MICQ 1024u         /* microphone samples queued from the sample interrupt to the listener */

static ba_show_t show;
static ba_lights_t lights;
static ba_buttons_t buttons;
static uint8_t ring_buf[BA_WS_FRAME_BYTES(BA_RING_N)];
static volatile uint8_t vol = 16;   /* of 16: the top step; the show's own gain and soft limiter keep it inside the DAC */
static volatile int tone_test;      /* 'T': a 1 kHz test tone in place of the show, for level checks */
static ba_tone_t tone;
static ba_eq_t tone_eq;
static volatile int32_t tone_level = 256;   /* the sweep plays at a quarter (-12 dB) so EQ boosts stay below the limiter */
static volatile uint32_t mic_rms_mv, mic_acc_n;
static volatile uint16_t mic_last;
static volatile uint8_t meas_on;
static volatile uint32_t meas_n;
static volatile int64_t meas_sum, meas_sq;
static int64_t mic_sum, mic_sq;
static volatile int play;
static volatile int ready;

/* Frame monitor: a model of the DAC8311 input stage fed by the same edges the core writes. */
static struct { int sync, sclk, din; uint32_t nclk, sr; } mon = { 1, 0, 0, 0, 0 };
static volatile uint32_t frames_ok, frames_bad, last_word;
static void m_sync(int l)
{
    if (mon.sync && !l) { mon.nclk = 0; mon.sr = 0; }
    if (!mon.sync && l) { if (mon.nclk == 16) { frames_ok++; last_word = mon.sr & 0xFFFFu; } else frames_bad++; }
    mon.sync = l;
    board_sync(l);
}
static void m_sclk(int l)
{
    if (mon.sclk && !l && !mon.sync) { mon.sr = (mon.sr << 1) | (uint32_t)mon.din; mon.nclk++; }
    mon.sclk = l;
    board_sclk(l);
}
static void m_din(int l) { mon.din = l; board_din(l); }
static const ba_hal_t hal = { m_sync, m_sclk, m_din };

static volatile uint32_t late_ticks;
static volatile uint32_t ticks, tick_dt_min = 0xFFFFFFFFu, tick_dt_max, isr_max;
static volatile uint32_t heard[LS_COUNT];
static uint32_t last_tick_cyc;
static uint16_t micq[MICQ];
static volatile uint32_t micq_w;
static volatile uint8_t micq_quiet, micq_reset;
static ba_listen_t listener;

void app_tick(void)
{
    uint32_t t0 = board_cycles();
    if (ticks) {
        uint32_t dt = t0 - last_tick_cyc;
        if (dt < tick_dt_min) tick_dt_min = dt;
        if (dt > tick_dt_max) tick_dt_max = dt;
        uint32_t per = board_cpu_hz() / SAMPLE_RATE;
        if (dt > per + per / 2u || dt < per / 2u) late_ticks++;   /* a sample interrupt that came late or doubled up */
    }
    last_tick_cyc = t0;
    if (ready) {
        int16_t s = ba_show_next(&show);
        if (tone_test) {   /* the test tone goes through the same speaker EQ when it is on, so the sweep can compare */
            s = (int16_t)(((int32_t)ba_tone_next(&tone) * tone_level) >> 8);
            if (show.eq_on && SAMPLE_RATE == 16000u) s = ba_soft_limit(ba_eq_next(&tone_eq, s), 0);
        }
        ba_dac_write16(&hal, ba_dac_word(ba_dac_code_from_s16(s, vol), BA_DAC_PD_NORMAL));
        if (play) {
            uint16_t m = board_adc_read();
            mic_last = m;
            if (meas_on) { meas_sum += m; meas_sq += (int64_t)m * m; meas_n++; }
            board_adc_start();
            mic_sum += m; mic_sq += (int64_t)m * m;
            if (++mic_acc_n >= SAMPLE_RATE) {     /* microphone RMS over one second, mean removed, in mV (12-bit, 3.3 V) */
                int64_t n = (int64_t)mic_acc_n, var = (mic_sq - mic_sum * mic_sum / n) / n;
                uint32_t r = 0; while ((uint64_t)(r + 1u) * (r + 1u) <= (uint64_t)var) r++;
                mic_rms_mv = r * 3300u / 4096u;
                mic_sum = 0; mic_sq = 0; mic_acc_n = 0;
            }
            /* the listener hears the room only while the show is quiet: between songs, after the gap's own sound */
            int quiet = show.ev.in_gap && show.sfx.type == SFX_NONE && !tone_test && ba_ctl_listening;
            if (quiet && (ticks % (SAMPLE_RATE / 8000u)) == 0u) { micq[micq_w % MICQ] = m; micq_w++; }   /* the listener works at 8 kHz */ else if (micq_quiet) micq_reset = 1;
            micq_quiet = (uint8_t)quiet;
        }
    }
    ticks++;
    uint32_t d = board_cycles() - t0;
    if (d > isr_max) isr_max = d;
}

static void puts_(const char *s) { while (*s) board_uart_put((uint8_t)*s++); }
static void putu(uint32_t v)
{
    char b[11]; int i = 0;
    if (!v) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i) board_uart_put((uint8_t)b[--i]);
}
static void puthex4(uint32_t v) { for (int i = 12; i >= 0; i -= 4) board_uart_put((uint8_t)"0123456789abcdef"[(v >> i) & 15u]); }

static void settings(void)
{
    puts_(" eq="); putu(show.eq_on); puts_(" theme="); puts_(ba_theme_name(lights.theme)); puts_(" fx="); puts_(ba_fx_name(show.fxmode));
    puts_(" vol="); putu(vol); puts_(" shuffle="); putu(show.shuffle);
}

static void act(uint8_t a, const char *src)
{
    const char *name; uint32_t value;
    const char *what = ba_ctl_apply(a, &show, &lights, &vol, &name, &value);
    if (!what) return;
    static const uint16_t blip_hz[] = { 0, 880, 1320, 660, 1000, 1760, 520, 1180 };
    ba_sfx_blip(&show.sfx, blip_hz[a < 8 ? a : 0]);   /* a short beep for each action */
    puts_("CTL "); puts_(src); puts_(" "); puts_(what); if (*name) { puts_(" "); puts_(name); } puts_(" ("); putu(value); puts_(")");
    settings(); puts_("\r\n");
}

/* microphone RMS in tenths of an ADC code over ms milliseconds of whatever is playing */
static uint32_t mic_window(uint32_t ms)
{
    meas_on = 0; meas_sum = 0; meas_sq = 0; meas_n = 0; meas_on = 1;
    board_delay_ms(ms);
    meas_on = 0;
    int64_t n = meas_n ? (int64_t)meas_n : 1, var100 = (meas_sq - meas_sum * meas_sum / n) * 100 / n;
    uint32_t r = 0; while ((uint64_t)(r + 1u) * (r + 1u) <= (uint64_t)var100) r++;
    return r;
}

/* 'W': the speaker's response as the pack's microphone hears it: a 1/4-octave stepped sine from 125 Hz to 5 kHz at
 * the current volume and -12 dB, 250 ms a step, the microphone RMS of each (in tenths of a code). tools/fit_eq.py turns the table
 * into the EQ. */
static void sweep(void)
{
    static const uint16_t f[] = { 125, 149, 177, 210, 250, 297, 354, 420, 500, 595, 707, 841, 1000, 1189, 1414, 1682,
                                  2000, 2378, 2828, 3364, 4000, 4757 };
    int was = tone_test; tone_level = 64;
    puts_("SWEEP vol="); putu(vol); puts_(" rate="); putu(SAMPLE_RATE); puts_(" quiet="); 
    tone_test = 1; ba_tone_init(&tone, 0, SAMPLE_RATE); tone.step = 0; putu(mic_window(300)); puts_("\r\n");
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        ba_tone_init(&tone, f[i], SAMPLE_RATE); tone_test = 1;
        board_delay_ms(120);
        uint32_t r = mic_window(250);
        puts_("SWEEP f="); putu(f[i]); puts_(" rms_x10="); putu(r); puts_("\r\n");
    }
    tone_test = was; tone_level = 256;
    puts_("SWEEP end\r\n");
}

/* what each heard sound sets off: a sound effect and a color flash on the lights */
static void react(uint8_t ev)
{
    heard[ev]++;
    switch (ev) {
    case LS_CLAP:    ba_show_scare(&show, SFX_THUNDER); ba_lights_count(&lights, 8, 64, 64, 64); break;  /* lightning white */
    case LS_SHOUT:   ba_show_scare(&show, SFX_SCREAM);  ba_lights_count(&lights, 8, 64, 0, 0); break;    /* red */
    case LS_WHISTLE: ba_show_scare(&show, SFX_WAIL);    ba_lights_count(&lights, 8, 0, 64, 10); break;   /* green */
    case LS_KNOCK:   ba_show_scare(&show, SFX_DOOR);    ba_lights_count(&lights, 8, 64, 24, 0); break;   /* orange */
    default: return;
    }
    puts_("HEARD "); puts_(ba_listen_name(ev)); puts_(" ms="); putu(listener.last_ms); puts_(" zcr="); putu(listener.last_zcr);
    puts_(" voiced="); putu(listener.last_voiced_pct); puts_("% pitch="); putu(listener.last_pitch_hz); puts_(" peak="); putu(listener.last_peak);
    puts_(" floor="); putu(listener.last_floor); puts_("\r\n");
}

/* 'Z': the synthetic clips of halloween/ba_testclips.h through a fresh listener, the same code and the same clips the
 * Mac test runs (host/test_listen.c, gain 256); prints each clip's class, its features and the worst cycles a sample */
static void listen_selftest(void)
{
    static const char *kn[TC_COUNT] = { "clap", "knock", "whistle", "shout", "quiet", "tick" };
    static ba_listen_t t;
    for (int k = 0; k < TC_COUNT; k++) {
        ba_listen_init(&t); uint32_t rng = 12345u + (uint32_t)k * 77u + 2u, worst = 0; uint8_t got = LS_NONE;
        for (uint32_t i = 0; i < 8000u; i++) ba_listen_sample(&t, ba_testclip(TC_QUIET, i, &rng, 256));
        for (uint32_t i = 0; i < TC_LEN + 8000u; i++) {
            uint16_t v = i < TC_LEN ? ba_testclip((uint8_t)k, i, &rng, 256) : ba_testclip(TC_QUIET, i, &rng, 256);
            uint32_t c0 = board_cycles(); uint8_t c = ba_listen_sample(&t, v); uint32_t dc = board_cycles() - c0;
            if (dc > worst) worst = dc;
            if (c && !got) got = c;
        }
        puts_("SELFTEST "); puts_(kn[k]); puts_(" -> "); puts_(ba_listen_name(got)); puts_(" ms="); putu(t.last_ms);
        puts_(" zcr="); putu(t.last_zcr); puts_(" voiced="); putu(t.last_voiced_pct); puts_("% pitch="); putu(t.last_pitch_hz);
        puts_(" worst_cycles="); putu(worst); puts_("\r\n");
    }
}

void app_main(void)
{
    board_init();
    play = board_show_mode();
    uint32_t seed = board_seed();
    ba_show_init(&show, ba_setlist, ba_setlist_n, SAMPLE_RATE, GAP_MS, seed);
    ba_show_add_tracks(&show, ba_tracks, ba_tracks_n, "In the Hall of the Mountain King");   /* the recording opens, if built in */
    ba_lights_init(&lights, seed ^ 0xA5A5A5A5u);
    ba_listen_init(&listener);
    ba_eq_init(&tone_eq);
    if (!board_has_mic()) ba_ctl_listening = 0;
    m_sync(1);
    ba_dac_write16(&hal, ba_dac_word(BA_DAC_MIDSCALE, BA_DAC_PD_NORMAL));
    if (play) { board_amp(1); board_mic_power(1); }   /* amplifier line low: it plays; microphone on for the listener */
    board_adc_start();
    ready = 1;
    board_tick_enable();
    puts_("\r\nHALLOWEEN SHOW, BOOSTXL-AUDIO app, DAC8311 on SPI pins 7/12/15, sample rate "); putu(SAMPLE_RATE); puts_(" Hz, recorded tracks "); putu(ba_tracks_n); puts_("\r\n");
    puts_("MODE "); puts_(play ? "play" : "silent (pack lines left as inputs)"); puts_(" seed=0x"); puthex4(seed >> 16); puthex4(seed); puts_("\r\n");
    board_show_reason(puts_);
    if (!board_has_mic()) puts_("LISTEN off: this board cannot power the pack's microphone\r\n");
    board_ring_info(puts_);
    puts_("KEYS n next song, t theme, f effects, v volume, s shuffle, p status; A, L, B, K, X inject button presses\r\n");

    int btn_a_was = 0, btn_b_was = 0; uint32_t micq_r = 0;
    uint32_t sim_a = 0, sim_b = 0, red_edges = 0, green_edges = 0; int red_was = 0, green_was = 0;
    uint32_t frame_tick = 0, loops = 0, ring_frames = 0, ring_skips = 0, song_seen = 0xFFFFFFFFu, songs = 0;
    uint32_t stat_cyc = board_cycles(), stat_frames = frames_ok, secs = 0, hz = board_cpu_hz(), fps_last = 0;
    for (;;) {
        if ((uint32_t)(ticks - frame_tick) >= SAMPLE_RATE / BA_LIGHTS_FPS) {
            frame_tick += SAMPLE_RATE / BA_LIGHTS_FPS;
            int a, b; board_buttons(&a, &b);
            if (a != btn_a_was || b != btn_b_was) {   /* every edge of the real buttons, for debugging presses */
                puts_("BTN A="); putu((uint32_t)a); puts_(" B="); putu((uint32_t)b); puts_(" ms="); putu(ticks / 8u); puts_("\r\n");
                btn_a_was = a; btn_b_was = b;
            }
            if (sim_a) { sim_a--; a = 1; }
            if (sim_b) { sim_b--; b = 1; }
            act(ba_buttons_frame(&buttons, a, b), "button");
            ba_lights_frame(&lights, &show.ev);
            board_leds(lights.red, lights.green);
            board_rgb(lights.led);
            if (lights.red && !red_was) red_edges++;
            if (lights.green && !green_was) green_edges++;
            red_was = lights.red; green_was = lights.green;
            if (!board_ring_busy()) {
                ba_ws2812_encode((const uint8_t (*)[3])lights.rgb, BA_RING_N, ring_buf);
                board_ring_send(ring_buf, sizeof ring_buf);
                ring_frames++;
            } else ring_skips++;
        }
        int c = board_uart_get();
        if (c >= 0) {
            uint8_t a = c == 'E' ? ACT_EQ : c == 'n' ? ACT_NEXT_SONG : c == 't' ? ACT_NEXT_THEME : c == 'f' ? ACT_NEXT_FX : c == 'v' ? ACT_VOLUME : c == 's' ? ACT_SHUFFLE : ACT_NONE;
            if (a) act(a, "key");
            if (c == 'A') sim_a = 20; else if (c == 'L') sim_a = 100; else if (c == 'B') sim_b = 20; else if (c == 'K') sim_b = 100;
            else if (c == 'X') sim_a = sim_b = 50;
            else if (c == 'Y') sim_a = sim_b = 520;
            else if (c == 'U') sim_a = sim_b = 300;
            else if (c == 'Z') listen_selftest();
            else if (c == 'W') sweep();
            else if (c == 'R') {   /* raw microphone ADC codes, as the sample interrupt last read them */
                puts_("MICRAW");
                for (int i = 0; i < 16; i++) { puts_(" "); putu(mic_last); board_delay_ms(1); }
                puts_("\r\n");
            }
            else if (c == 'T') { ba_tone_init(&tone, 1000, SAMPLE_RATE); tone_test = !tone_test; puts_(tone_test ? "TONE 1 kHz on\r\n" : "TONE off\r\n"); }
            else if (c == 'Q') { tone_test = !tone_test; ba_tone_init(&tone, 0, SAMPLE_RATE); tone.step = 0; puts_(tone_test ? "QUIET on\r\n" : "QUIET off\r\n"); }
            else if (c == 'G') { show.gain_q8 = show.gain_q8 == 256u ? BA_SHOW_GAIN_Q8 : 256u; puts_("GAIN q8="); putu(show.gain_q8); puts_("\r\n"); }
            else if (c == 'M') { puts_("MIC rms_mv="); putu(mic_rms_mv); puts_(" vol="); putu(vol); puts_(" gain_q8="); putu(show.gain_q8); puts_(tone_test ? " tone" : " show"); puts_("\r\n"); }
            if (c == 'p') {
                uint32_t du = board_rgb_duty();
                puts_("STATE song="); putu(show.ev.song); settings();
                puts_(" led="); putu(lights.led[0]); puts_(","); putu(lights.led[1]); puts_(","); putu(lights.led[2]);
                puts_(" rgb_duty="); putu(du >> 16); puts_(","); putu((du >> 8) & 255u); puts_(","); putu(du & 255u);
                puts_(" gap="); putu(show.ev.in_gap); puts_("\r\n");
            }
        }
        /* the listener runs here, not in the interrupt: its pitch check takes too long for one sample period */
        if (micq_reset) { micq_reset = 0; ba_listen_reset_event(&listener); }
        while (micq_r != micq_w) {
            if (micq_w - micq_r > MICQ) micq_r = micq_w - MICQ;   /* fell behind: skip ahead */
            uint32_t c0 = board_cycles();
            uint8_t ev = ba_listen_sample(&listener, micq[micq_r % MICQ]);
            uint32_t dc = board_cycles() - c0; if (dc > listener.cycles_max) listener.cycles_max = dc;
            micq_r++;
            if (ev) react(ev);
        }
        if (show.ev.song_seq != song_seen) {
            song_seen = show.ev.song_seq;
            if (songs && show.pos == 0) loops++;
            songs++;
            puts_(show.ev.song >= show.n ? "TRACK " : "SONG "); putu(show.ev.song); puts_(" "); puts_(ba_show_title(&show, show.ev.song));
            if (show.ev.song >= show.n) { puts_(" ["); puts_(show.tracks[show.ev.song - show.n].credit); puts_("]"); }
            puts_(" key="); puts_(show.key_shift ? (show.key_shift == -1 ? "-1" : "-2") : "0");
            puts_(" tempo="); putu(show.tempo_pct); puts_("% swap="); putu(show.swap);
            puts_(" loop="); putu(loops); puts_(" played="); putu(songs); settings(); puts_("\r\n");
        }
        uint32_t now = board_cycles();
        if (now - stat_cyc >= hz) {
            uint32_t f = frames_ok;
            fps_last = (uint32_t)((uint64_t)(f - stat_frames) * hz / (now - stat_cyc));
            stat_frames = f; stat_cyc = now; secs++;
            if (secs % 10u == 0u) {
                puts_("STAT t="); putu(secs); puts_(" dac_fps="); putu(fps_last);
                puts_(" frames_ok="); putu(frames_ok); puts_(" frames_bad="); putu(frames_bad);
                puts_(" last_word=0x"); puthex4(last_word);
                puts_(" ring_frames="); putu(ring_frames); puts_(" ring_skips="); putu(ring_skips);
                puts_(" tick_min="); putu(tick_dt_min); puts_(" tick_max="); putu(tick_dt_max);
                puts_(" isr_max="); putu(isr_max); puts_(" late="); putu(late_ticks); puts_(" rate="); putu(SAMPLE_RATE); puts_(" song="); putu(show.ev.song); puts_(" loop="); putu(loops);
                uint32_t du = board_rgb_duty();
                puts_(" rgb_duty="); putu(du >> 16); puts_(","); putu((du >> 8) & 255u); puts_(","); putu(du & 255u);
                puts_(" led4_on="); putu(red_edges); puts_(" led5_on="); putu(green_edges);
                puts_(" mic_rms_mv="); putu(mic_rms_mv); puts_(" limited="); putu(show.limited);
                puts_(" heard="); putu(heard[LS_CLAP]); puts_(","); putu(heard[LS_KNOCK]); puts_(","); putu(heard[LS_WHISTLE]); puts_(","); putu(heard[LS_SHOUT]);
                puts_(" listen_floor="); putu(listener.floor); puts_(" listen_max_cyc="); putu(listener.cycles_max); puts_(" listening="); putu(ba_ctl_listening);
                puts_(" mode="); puts_(play ? "play" : "silent"); settings(); puts_("\r\n");
                tick_dt_min = 0xFFFFFFFFu; tick_dt_max = 0; isr_max = 0;
            }
        }
    }
}
