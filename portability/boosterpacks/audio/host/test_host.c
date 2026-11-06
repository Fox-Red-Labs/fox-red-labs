/* Host test for the portable core: renders the synth and the tone to WAV files through the real DAC8311 frame
 * code, and checks the frame logic against a model of the DAC (SYNC, 16 falling edges, cancel on early SYNC rise).
 *   run.sh builds it with the core sources and runs it, writing the WAV files to out/
 * Exit 0 when every check passes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ba_dac8311.h"
#include "ba_chip.h"
#include "ba_tone.h"
#include "ba_mic.h"
#include "fox_run.h"

#define RATE 8000

/* A model of the DAC8311 input stage. */
static struct { int sync, sclk, din; int nclk; uint32_t sr; uint16_t out; int updates, cancelled; } dac;
static void m_sync(int l) {
    if (dac.sync && !l) { dac.nclk = 0; dac.sr = 0; }                 /* SYNC falls: start a frame */
    if (!dac.sync && l) {                                             /* SYNC rises */
        if (dac.nclk == 16) { dac.out = (uint16_t)(dac.sr & 0x3FFF); dac.updates++; }
        else dac.cancelled++;                                         /* early rise cancels the write */
    }
    dac.sync = l;
}
static void m_sclk(int l) {
    if (dac.sclk && !l && !dac.sync) { dac.sr = (dac.sr << 1) | (uint32_t)dac.din; dac.nclk++; } /* falling edge shifts */
    dac.sclk = l;
}
static void m_din(int l) { dac.din = l; }
static const ba_hal_t hal = { m_sync, m_sclk, m_din };

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static void wav(const char *path, const int16_t *x, unsigned n)
{
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(2); }
    uint32_t dlen = n * 2, rate = RATE, br = RATE * 2, fl = 16 + 4 + dlen + 20; uint16_t one = 1, bits = 16, ba = 2;
    uint32_t sixteen = 16, riff = 36 + dlen;
    (void)fl;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sixteen, 4, 1, f);
    fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dlen, 4, 1, f); fwrite(x, 2, n, f); fclose(f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[512];
    dac.sync = 1;

    /* 1. frame bit order and update */
    ba_dac_write16(&hal, ba_dac_word(0x2ABC & 0x3FFF, 0));
    CHECK(dac.updates == 1 && dac.out == (0x2ABC & 0x3FFF), "16-bit frame updates the DAC with the right code");
    ba_dac_write16(&hal, ba_dac_word(1, 0));
    CHECK(dac.out == 1 && dac.updates == 2, "second frame updates again");

    /* 2. the split-8 frame never updates */
    int before = dac.updates, canc = dac.cancelled;
    for (int i = 0; i < 100; i++) ba_dac_write_split8(&hal, ba_dac_word(8192 + i, 0));
    CHECK(dac.updates == before, "two 8-bit frames with SYNC between them: DAC never updates");
    CHECK(dac.cancelled - canc >= 100, "the early SYNC rise cancels each half frame");

    /* 3. tone: 1 kHz at 8 kHz, rendered through the frame code and read back from the DAC model */
    enum { N = 8000 };
    static int16_t pcm[N * 3];
    ba_tone_t t; ba_tone_init(&t, 1000, RATE);
    for (int i = 0; i < N; i++) {
        uint16_t code = ba_dac_code_from_s16(ba_tone_next(&t), 12);
        ba_dac_write16(&hal, ba_dac_word(code, 0));
        pcm[i] = (int16_t)(((int)dac.out - 8192) * 4);
    }
    /* Goertzel at 1 kHz and at 2 kHz and 3 kHz */
    double e1 = 0, tot = 0;
    for (int k = 1; k <= 3; k++) {
        double w = 2 * M_PI * 1000.0 * k / RATE, c = 2 * cos(w), s1 = 0, s2 = 0;
        for (int i = 0; i < N; i++) { double s = pcm[i] + c * s1 - s2; s2 = s1; s1 = s; }
        double p = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (k == 1) e1 = p;
        tot += p;
    }
    CHECK(e1 / tot > 0.999, "tone: 99.9 percent of the energy at 1 kHz");
    snprintf(path, sizeof path, "%s/tone_1khz.wav", dir); wav(path, pcm, N);

    /* 4. song: render one pass through the DAC model */
    ba_song_t song = { (const uint8_t (*)[4])fox_run_rows, sizeof fox_run_rows / 4, (uint32_t)RATE * 30 / FOX_RUN_TEMPO, FOX_RUN_VOL };
    ba_chip_t c; ba_chip_init(&c, &song, RATE);
    unsigned n = song.nrows * song.samples_per_row;
    int16_t *tune = malloc(n * 2);
    int clipped = 0, lo = 99999, hi = -99999;
    for (unsigned i = 0; i < n; i++) {
        int16_t s = ba_chip_next(&c);
        uint16_t code = ba_dac_code_from_s16(s, 12);
        if (code == 0 || code == BA_DAC_MAX) clipped++;
        ba_dac_write16(&hal, ba_dac_word(code, 0));
        tune[i] = (int16_t)(((int)dac.out - 8192) * 4);
        if (tune[i] < lo) lo = tune[i]; if (tune[i] > hi) hi = tune[i];
    }
    printf("song: %u rows, %u samples (%.2f s), DAC code range %d to %d, clipped %d\n", song.nrows, n, n / (double)RATE, lo / 4 + 8192, hi / 4 + 8192, clipped);
    CHECK(clipped == 0, "song stays inside the DAC range");
    CHECK(hi - lo > 8000, "song has real swing");
    snprintf(path, sizeof path, "%s/fox_run.wav", dir); wav(path, tune, n);

    /* 5. note table: A4 = 440 Hz within 0.1 percent; step for 1 kHz */
    double f440 = (double)ba_midi_step24(69, RATE) * RATE / 16777216.0;
    CHECK(fabs(f440 - 440.0) / 440.0 < 0.001, "MIDI 69 is 440 Hz within 0.1 percent");
    double f880 = (double)ba_midi_step24(81, RATE) * RATE / 16777216.0;
    CHECK(fabs(f880 - 880.0) / 880.0 < 0.001, "MIDI 81 is 880 Hz");
    CHECK(ba_tone_step(1000, RATE) == 8192, "1 kHz step at 8 kHz is 8192");

    /* 6. mic stats on a known signal */
    uint16_t m[1000];
    for (int i = 0; i < 1000; i++) m[i] = (uint16_t)(2048 + 400 * sin(2 * M_PI * i / 8.0));
    ba_mic_stats_t st; ba_mic_stats(m, 1000, &st);
    CHECK(fabs(st.rms_x16 / 16.0 - 400 / sqrt(2)) < 3 && abs(st.mean_x16 / 16 - 2048) < 2, "mic stats: RMS and mean of a sine");
    free(tune);
    printf(fails ? "FAILED %d\n" : "all checks pass\n", fails);
    return fails ? 1 : 0;
}
