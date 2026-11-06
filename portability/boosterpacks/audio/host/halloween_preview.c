/* Host preview of the Halloween show: the same synth (core/ba_chip.c), set list, show player, effects, sound effects,
 * light engine, controls and recorded tracks (halloween/, tracks/) as the board, at the LP-MSPM33C321A's 16 kHz, through the same DAC code conversion at
 * the board's volume. A scripted run presses the controls at fixed times so one file covers several songs, effect
 * modes and light themes, with one natural song end (the gap's scare sound and the candle).
 * Writes <out>/halloween-preview.wav (each sample is the DAC8311 code the board would send, centred and scaled by 4,
 * as host/test_host.c does) and <out>/halloween-ring.csv (ring colors, board LEDs, song, theme and effect mode, 25
 * times a second). Checks the length, DAC clipping and the WS2812 encoder. */
#include <stdio.h>
#include <stdlib.h>
#include "ba_dac8311.h"
#include "ba_show.h"
#include "ba_lights.h"
#include "ba_ws2812.h"
#include "ba_ctl.h"

#ifndef RATE
#define RATE 16000u   /* the LP-MSPM33C321A's rate */
#endif
#define GAP_MS 6000u
#define SEED 0x2026u

static void wav_header(FILE *f, uint32_t n)
{
    uint32_t dlen = n * 2u, rate = RATE, br = RATE * 2u, sixteen = 16, riff = 36 + dlen; uint16_t one = 1, bits = 16, ba = 2;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sixteen, 4, 1, f);
    fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dlen, 4, 1, f);
}

/* the script: at t seconds, do this. The first song plays to its natural end only if the gap falls inside. */
static const struct { float t; uint8_t act; } script[] = {
    { 0.5f, ACT_NEXT_THEME },   /* pumpkin (the default) -> fire */
    { 22.0f, ACT_NEXT_FX },     /* clean -> haunted */
    { 40.0f, ACT_NEXT_SONG },
    { 41.0f, ACT_NEXT_THEME },  /* -> fire */
    { 62.0f, ACT_NEXT_THEME },  /* -> lightning */
    { 63.0f, ACT_NEXT_FX },     /* -> graveyard */
    { 84.0f, ACT_NEXT_SONG },
    { 85.0f, ACT_NEXT_THEME },  /* -> ghost */
    { 86.0f, ACT_NEXT_FX },     /* -> mad lab */
    { 110.0f, ACT_NEXT_FX },    /* -> clean */
    { 111.0f, ACT_NEXT_THEME }, /* -> heartbeat */
    { 132.0f, ACT_NEXT_THEME }, /* -> witch */
    { 150.0f, ACT_NEXT_THEME }, /* -> music */
};
#define END_S 165.0f

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[1024];
    static ba_show_t show; static ba_lights_t li; volatile uint8_t vol = 16;
    ba_show_init(&show, ba_setlist, ba_setlist_n, RATE, GAP_MS, SEED);
    ba_show_add_tracks(&show, ba_tracks, ba_tracks_n, "In the Hall of the Mountain King");
    ba_lights_init(&li, SEED ^ 0xA5A5A5A5u);
    snprintf(path, sizeof path, "%s/halloween-preview.wav", dir);
    FILE *w = fopen(path, "wb"); if (!w) { perror(path); return 2; }
    snprintf(path, sizeof path, "%s/halloween-ring.csv", dir);
    FILE *c = fopen(path, "w"); if (!c) { perror(path); return 2; }
    fprintf(c, "t_s,song,gap,theme,fx,red,green,led_r,led_g,led_b");
    for (int i = 0; i < BA_RING_N; i++) fprintf(c, ",r%d,g%d,b%d", i, i, i);
    fprintf(c, "\n");
    wav_header(w, 0);
    const uint32_t total = (uint32_t)(END_S * RATE);
    uint32_t clipped = 0, lo = 16383, hi = 0, lf = 0, si = 0, seen = 0xFFFFFFFFu, gaps = 0, in_gap = 0;
    uint8_t enc[BA_WS_FRAME_BYTES(BA_RING_N)];
    for (uint32_t n = 0; n < total; n++) {
        while (si < sizeof script / sizeof script[0] && n >= (uint32_t)(script[si].t * RATE)) {
            const char *name; uint32_t v;
            const char *what = ba_ctl_apply(script[si].act, &show, &li, &vol, &name, &v);
            printf("%6.1f s  %s %s\n", n / (double)RATE, what, name);
            si++;
        }
        int16_t s = ba_show_next(&show);
        if (show.ev.song_seq != seen) {
            seen = show.ev.song_seq;
            printf("%6.1f s  %s %u: %s (key %d, tempo %u%%, swap %u)\n", n / (double)RATE, show.ev.song >= show.n ? "track" : "song", show.ev.song, ba_show_title(&show, show.ev.song), show.key_shift, show.tempo_pct, show.swap);
        }
        if (show.ev.in_gap && !in_gap) { gaps++; if (show.sfx.type) printf("%6.1f s  gap, scare sound: %s\n", n / (double)RATE, ba_sfx_name(show.sfx.type)); }
        in_gap = show.ev.in_gap;
        uint16_t code = ba_dac_code_from_s16(s, vol);
        if (code == 0 || code == BA_DAC_MAX) clipped++;
        if (code < lo) lo = code; if (code > hi) hi = code;
        int16_t pcm = (int16_t)(((int)code - 8192) * 4);
        fwrite(&pcm, 2, 1, w);
        if ((n + 1) % (RATE / BA_LIGHTS_FPS) == 0) {
            ba_lights_frame(&li, &show.ev);
            ba_ws2812_encode((const uint8_t (*)[3])li.rgb, BA_RING_N, enc);
            if (lf++ % 4u == 0u) {
                fprintf(c, "%.3f,%u,%u,%u,%u,%u,%u,%u,%u,%u", (n + 1) / (double)RATE, show.ev.song, show.ev.in_gap, li.theme, show.fxmode, li.red, li.green, li.led[0], li.led[1], li.led[2]);
                for (int i = 0; i < BA_RING_N; i++) fprintf(c, ",%u,%u,%u", li.rgb[i][0], li.rgb[i][1], li.rgb[i][2]);
                fprintf(c, "\n");
            }
        }
    }
    fseek(w, 0, SEEK_SET); wav_header(w, total); fclose(w); fclose(c);
    printf("preview: %u samples (%.2f s), %u gaps; DAC codes %u to %u of 0 to 16383, clipped %u, samples the soft limiter bent %u (%.2f percent)\n",
           total, total / (double)RATE, gaps, lo, hi, clipped, (unsigned)show.limited, 100.0 * show.limited / total);
    uint8_t one[1][3] = { { 0x80, 0x01, 0xFF } }, e[BA_WS_FRAME_BYTES(1)];
    ba_ws2812_encode((const uint8_t (*)[3])one, 1, e);   /* G 92 49 26, R D2 49 24, B DB 6D B6, then 00 00 */
    int ok = e[0] == 0x92 && e[1] == 0x49 && e[2] == 0x26 && e[3] == 0xD2 && e[6] == 0xDB && e[8] == 0xB6 && e[9] == 0 && e[10] == 0;
    printf("ws2812 encoder: %s\n", ok ? "ok" : "WRONG");
    int pass = ok && clipped == 0;
    printf(pass ? "all checks pass\n" : "FAILED\n");
    return pass ? 0 : 1;
}
