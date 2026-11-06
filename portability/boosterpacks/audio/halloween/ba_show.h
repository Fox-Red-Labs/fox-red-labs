/* Halloween show: plays a set list on the four-voice synth (core/ba_chip.h) forever. Between songs: a short gap with
 * a random scare sound. Each pass of a song gets a small random variation (key 0 to 2 semitones down, tempo within
 * 5 percent, lead and harmony timbres swapped or not). Effect modes: clean, haunted (echo plus vibrato and tremolo on
 * the lead), graveyard (a wind bed under the music), mad lab (bit-crush and zap sweeps). Integer only.
 * ba_show_next() runs one sample, from the sample interrupt. It also reports what the synth played (ba_show_events_t)
 * so lights can follow the music with no microphone. */
#ifndef BA_SHOW_H
#define BA_SHOW_H
#include <stdint.h>
#include "ba_chip.h"
#include "ba_sfx.h"
#include "ba_adpcm.h"
#include "ba_eq.h"

typedef struct {
    const char *name;
    const uint8_t (*rows)[BA_VOICES];
    uint16_t nrows;
    uint16_t tempo;                 /* tempo of row 0 (tune2h: a row lasts 30/tempo s) */
    const uint16_t (*marks)[2];     /* {row, tempo} or NULL */
    uint8_t nmarks;
    uint8_t vol[BA_VOICES];
} ba_show_song_t;

enum { FX_CLEAN, FX_HAUNTED, FX_GRAVEYARD, FX_MADLAB, FX_COUNT };
#define BA_SHOW_MAX_SONGS 24
#ifndef BA_SHOW_FIRST_SONG
#define BA_SHOW_FIRST_SONG 3   /* Danse macabre opens the first pass (index in ba_setlist) */
#endif
#ifndef BA_SHOW_GAIN_Q8
#define BA_SHOW_GAIN_Q8 416   /* 1.625: clean songs peak near 20000 before the gain, so the limiter only bends the loudest notes */
#endif
#define BA_ECHO_N 4000u   /* up to 250 ms at 16 kHz, 8 kB; the echo uses rate / 4 samples of it */

/* Counters only grow, so a reader that sees a counter change knows an event happened. */
typedef struct {
    volatile uint32_t lead_seq, bass_seq, drum_seq, song_seq, sfx_seq;
    volatile uint8_t lead_note, bass_note, drum;  /* MIDI notes; drum is BA_DRUM_KICK, _SNARE or _HAT */
    volatile uint8_t in_gap, song, sfx;           /* song = index in the set list; sfx = the last effect started */
    volatile uint16_t level;                      /* loudness of the output, 0..32767, a fast-attack follower */
    volatile uint32_t row_samples;                /* current row length in samples */
} ba_show_events_t;

typedef struct {
    const ba_show_song_t *list;
    const ba_track_t *tracks;       /* recorded tracks (IMA ADPCM), after the synth songs in the order; may be none */
    uint8_t ntracks;
    uint8_t n, pos, idx;            /* idx >= n means track idx - n */
    uint8_t order[BA_SHOW_MAX_SONGS];
    uint8_t shuffle;
    uint8_t first;                  /* the song that opens the first pass after power-up (0xFF: any) */
    uint16_t gain_q8;               /* master gain before the limiter, 256 = 1.0 */
    volatile uint32_t limited;      /* samples the soft limiter bent */
    uint8_t fxmode;
    uint32_t rate, rng;
    uint32_t gap_samples, gap_left, gap_len;
    volatile uint8_t skip;          /* set by a control: end this song now */
    /* this pass's variation */
    int8_t key_shift;               /* 0, -1 or -2 semitones */
    uint8_t tempo_pct;              /* 95..105 */
    uint8_t swap;                   /* 1: the lead plays on the 50 percent pulse voice */
    uint32_t logical[3], base[3];
    uint32_t lfo_v, lfo_t;          /* vibrato and tremolo phases */
    uint32_t zap_left;
    ba_song_t cur;
    ba_chip_t chip;
    uint16_t rows_played;
    uint8_t mark_i;
    ba_sfx_t sfx;
    ba_wind_t wind;
    int16_t echo[BA_ECHO_N];
    uint16_t echo_i;
    int16_t crush_hold; uint8_t crush_n;
    int32_t env;
    ba_adpcm_t player;
    uint8_t eq_on;                  /* the fitted speaker EQ and bass enhancer (16 kHz only) */
    ba_eq_t eq;
    uint32_t on_acc, on_avg; uint16_t on_n, on_count;   /* onset detector for recorded tracks */
    ba_show_events_t ev;
} ba_show_t;

void ba_show_init(ba_show_t *s, const ba_show_song_t *list, uint8_t n, uint32_t rate_hz, uint32_t gap_ms, uint32_t seed);
void ba_show_add_tracks(ba_show_t *s, const ba_track_t *t, uint8_t nt, const char *first_title);  /* call before playing */
const char *ba_show_title(const ba_show_t *s, uint8_t idx);
int16_t ba_show_next(ba_show_t *s);           /* one sample */
void ba_show_next_song(ba_show_t *s);         /* request: end this song now (a short gap, no scare) */
void ba_show_set_shuffle(ba_show_t *s, int on);
void ba_show_scare(ba_show_t *s, uint8_t sfx);/* play an effect now, over the music */
int16_t ba_soft_limit(int32_t x, volatile uint32_t *count);  /* unity up to 20000, then a soft knee to 32767 */
const char *ba_fx_name(uint8_t m);

extern const ba_show_song_t ba_setlist[];
extern const uint8_t ba_setlist_n;
extern const ba_track_t ba_tracks[];
extern const uint8_t ba_tracks_n;

#endif
