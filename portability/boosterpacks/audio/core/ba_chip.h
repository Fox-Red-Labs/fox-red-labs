/* Small chiptune synth: four voices at a fixed sample rate.
 *   voice 0  pulse, 25 percent duty (lead)
 *   voice 1  pulse, 50 percent duty (harmony)
 *   voice 2  triangle (bass)
 *   voice 3  noise from a 15-bit LFSR (drums: kick, snare, hat)
 * A song is rows of four codes. One row lasts ba_song_t.samples_per_row samples.
 * Codes: 0 rest, 1 hold the previous note, 12 to 119 a MIDI note number (voices 0 to 2);
 * on voice 3: 0 rest, 1 hold, 2 kick, 3 snare, 4 hat. The text source format is songs/FORMAT in README.md. */
#ifndef BA_CHIP_H
#define BA_CHIP_H
#include <stdint.h>

#define BA_VOICES 4
#define BA_CODE_REST 0
#define BA_CODE_HOLD 1
#define BA_DRUM_KICK 2
#define BA_DRUM_SNARE 3
#define BA_DRUM_HAT 4

typedef struct {
    const uint8_t (*rows)[BA_VOICES];
    uint16_t nrows;
    uint32_t samples_per_row;
    uint8_t vol[BA_VOICES]; /* 0 to 15 per voice */
} ba_song_t;

typedef struct {
    const ba_song_t *song;
    uint32_t sample_in_row;
    uint16_t row;
    uint32_t phase[3];  /* 24-bit accumulators for the three tonal voices */
    uint32_t step[3];   /* 0 = silent */
    uint16_t lfsr;
    uint8_t drum;       /* 0 none, else BA_DRUM_* */
    uint16_t drum_pos;  /* samples since the hit */
    uint8_t noise_div, noise_cnt;
    int8_t noise_out;
    uint32_t rate;
} ba_chip_t;

void ba_chip_init(ba_chip_t *c, const ba_song_t *song, uint32_t rate_hz);
int16_t ba_chip_next(ba_chip_t *c); /* one sample, full scale about +/-32000 */
uint32_t ba_midi_step24(uint8_t note, uint32_t rate_hz); /* 24-bit phase step for a MIDI note at rate_hz */

#endif
