/* Opus encode then decode of a generated test signal, frame by frame, for one configuration.
 * Records cycles inside opus_encode and opus_decode, a CRC-32 of the whole packet stream and of all decoded PCM.
 * The same file builds on the Mac and on the board. */
#ifndef OPUSRUN_H
#define OPUSRUN_H
#include <stdint.h>
typedef struct {
    uint32_t magic;                       /* 0x4F505252 when this run finished */
    uint32_t hz, frame_us, kbps, app, complexity, frames, errors;
    uint32_t enc_size, dec_size;          /* bytes of codec state from opus_encoder_get_size and opus_decoder_get_size */
    uint32_t bits_crc, pcm_crc;
    uint32_t enc_lo, enc_hi, dec_lo, dec_hi;
    uint32_t enc_min, enc_max, dec_min, dec_max;
    uint32_t bytes_total, bytes_min, bytes_max;
    uint32_t n_silk, n_hybrid, n_celt;    /* packets by the mode in their table-of-contents byte */
    uint32_t toc0;                        /* first packet's TOC byte */
} oprun_t;
/* enc_crc and dec_crc (100 entries each, or NULL) receive the CRC-32 of every packet and of every decoded frame. */
void oprun(int hz, int frame_us, int kbps, int app_kind, int complexity, oprun_t *r, uint32_t *enc_crc, uint32_t *dec_crc, uint32_t (*now)(void));
#endif
