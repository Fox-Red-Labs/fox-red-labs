/* Sound-event listener for the Halloween show: clap, knock, whistle and shout from the BOOSTXL-AUDIO microphone,
 * by rules on a few features, not a trained model. Integer only.
 * Frames of 128 samples (16 ms at 8 kHz): energy above a learned noise floor, zero-crossing rate, and a YIN pitch
 * check (algos/ba_yin.c) every third frame of an event. An event starts when the energy jumps 8 times over the floor
 * and above an absolute minimum, and ends after 3 quiet frames or 2 s. Then:
 *   one frame only (a click)                          nothing
 *   up to 160 ms and noisy (many zero crossings)      clap
 *   up to 160 ms and dull (few zero crossings)        knock
 *   longer, mostly pitched, pitch 600 to 3,500 Hz     whistle
 *   longer, loud, anything else                       shout
 * Self-trigger guard: the caller feeds samples only when the show is quiet (between songs, after the gap's own
 * sound), and a 3 s cooldown follows every event. */
#ifndef BA_LISTEN_H
#define BA_LISTEN_H
#include <stdint.h>

enum { LS_NONE, LS_CLAP, LS_KNOCK, LS_WHISTLE, LS_SHOUT, LS_COUNT };
#define BA_LISTEN_FRAME 128u
#define BA_LISTEN_HIST 384u

typedef struct {
    int16_t hist[BA_LISTEN_HIST];   /* the last 384 samples, DC removed, for the pitch check */
    uint16_t hn;
    int32_t dc_q8;                  /* slow DC estimate, Q8 */
    uint32_t e_acc; uint16_t zc, fn; int16_t last;
    uint32_t floor;                 /* noise floor of frame energy */
    uint8_t in_event, quiet;
    uint16_t ev_frames, ev_zc_sum, ev_pitch_n, ev_voiced, ev_pitch_hi;
    uint32_t ev_peak;
    uint16_t cooldown;              /* frames */
    uint16_t warm;                  /* frames left that only learn the floor (after power-up or a pause) */
    uint8_t seeded;                 /* the DC estimate starts from the first sample heard */
    uint32_t frames;
    /* the last event, for the console */
    uint8_t last_class; uint16_t last_ms, last_zcr, last_voiced_pct, last_pitch_hz; uint32_t last_peak, last_floor;
    uint32_t cycles_max;
} ba_listen_t;

void ba_listen_init(ba_listen_t *l);
/* one microphone sample (12-bit ADC code); returns LS_* when an event has just been classified, else LS_NONE */
uint8_t ba_listen_sample(ba_listen_t *l, uint16_t code);
void ba_listen_reset_event(ba_listen_t *l);   /* drop a half-heard event (the show started to play again) */
const char *ba_listen_name(uint8_t c);

#endif
