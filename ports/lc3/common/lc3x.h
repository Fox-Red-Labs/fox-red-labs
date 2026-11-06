/* LC3 extras: other frame durations, packet loss concealment and two channels. Same test signal as lc3run.c, one second of audio per run.
 * Records the cycles inside lc3_encode and lc3_decode (summed over the channels of a frame), a CRC-32 of the whole bitstream and of all decoded PCM. */
#ifndef LC3X_H
#define LC3X_H
#include <stdint.h>
typedef struct {
    uint32_t magic;                       /* 0x4C583352 when this run finished */
    uint32_t dt_us, hz, kbps, nch, loss_every, nbytes, frames, errors;
    uint32_t enc_size, dec_size;          /* bytes of codec state per channel */
    uint32_t bits_crc, pcm_crc;
    uint32_t enc_lo, enc_hi, dec_lo, dec_hi;      /* summed cycles over all frames */
    uint32_t enc_min, enc_max, dec_min, dec_max;  /* per frame, channels together */
    uint32_t good_lo, good_hi, good_n, lost_lo, lost_hi, lost_n;   /* decode cycles split by good and concealed frames */
    uint32_t dec_h1, dec_h2;              /* decode cycles summed over the first and the second half of the frames (sweep, speech-like) */
} lc3xres_t;
void lc3xrun(int dt_us, int hz, int kbps, int nch, int loss_every, lc3xres_t *r, uint32_t (*now)(void));
#endif
