/* LC3 encode then decode of a generated test signal, frame by frame, for one LE Audio configuration.
 * Records what a bench needs: cycles inside lc3_encode and lc3_decode only, a CRC-32 of every encoded frame and of every
 * decoded frame, and CRC-32 of the whole bitstream and of all decoded PCM. The same file builds on the Mac and on the board. */
#ifndef LC3RUN_H
#define LC3RUN_H
#include <stdint.h>
#ifndef LC3_BENCH_FRAMES
#define LC3_BENCH_FRAMES 100              /* 100 frames of 10 ms = 1 s of audio per configuration (the emulator run uses 10) */
#endif
#define LC3_BENCH_MAXBYTES 120
typedef struct {
    uint32_t magic;                       /* 0x4C433352 when this configuration finished */
    uint32_t hz, kbps, nbytes, frames;
    uint32_t enc_size, dec_size;          /* bytes of codec state asked for by lc3_encoder_size / lc3_decoder_size */
    uint32_t bits_crc, pcm_crc;           /* CRC-32 of all encoded bytes, CRC-32 of all decoded int16 samples */
    uint32_t enc_lo, enc_hi, dec_lo, dec_hi;   /* summed cycles */
    uint32_t enc_min, enc_max, dec_min, dec_max;
    uint32_t errors;                      /* nonzero return codes from setup, encode or decode */
} lc3res_t;
typedef struct { uint32_t enc_crc[LC3_BENCH_FRAMES], dec_crc[LC3_BENCH_FRAMES], enc_cyc[LC3_BENCH_FRAMES], dec_cyc[LC3_BENCH_FRAMES]; } lc3detail_t;
/* now() returns a free-running 32-bit cycle count (wraps), or NULL when timing is not wanted. */
void lc3run(int hz, int kbps, lc3res_t *r, lc3detail_t *d, uint32_t (*now)(void));
#endif
