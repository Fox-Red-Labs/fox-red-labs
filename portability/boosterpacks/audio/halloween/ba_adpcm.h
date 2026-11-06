/* IMA ADPCM (4 bits a sample) playback for recorded tracks, and a fixed-ratio linear resampler from the track's rate
 * to the board's sample rate. A track is a header made by tools/add_track.py: mono, low nibble first. */
#ifndef BA_ADPCM_H
#define BA_ADPCM_H
#include <stdint.h>

typedef struct {
    const char *name;
    const char *credit;        /* performer, recording, licence: printed on the console when it plays */
    const uint8_t *data;
    uint32_t nsamples;
    uint16_t rate;             /* the track's own sample rate */
} ba_track_t;

typedef struct {
    const ba_track_t *t;
    uint32_t pos;              /* next sample to decode */
    int32_t pred; int8_t idx;  /* decoder state */
    int16_t s0, s1;            /* the two samples the output falls between */
    uint32_t frac, step;       /* output position between s0 and s1, Q16; step = track rate / board rate, Q16 */
    uint8_t done;
} ba_adpcm_t;

void ba_adpcm_start(ba_adpcm_t *p, const ba_track_t *t, uint32_t out_rate);
int16_t ba_adpcm_next(ba_adpcm_t *p);          /* one output sample at the board rate; 0 after the end */
int16_t ba_adpcm_decode(int32_t *pred, int8_t *idx, uint8_t nib);   /* one IMA step (shared with the host test) */

#endif
