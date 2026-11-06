/* DTMF decoder: eight Goertzel filters (697 770 852 941 and 1209 1336 1477 1633 Hz), blocks of 205 samples at 8 kHz
 * (25.6 ms, the usual block length). A digit is accepted when the strongest row and column tone hold at least share16/16 of the block's energy (half by default),
 * each beats the runner-up in its group by 6 dB, and the two are within 8 dB of each other (twist). Work: 8 x 205 multiplies per block. */
#ifndef BA_DTMF_H
#define BA_DTMF_H
#include <stdint.h>

#define BA_DTMF_N 205

typedef struct { int32_t s1[8], s2[8]; uint32_t n; uint64_t energy; char last, blk; uint8_t armed;
    uint8_t share16; /* minimum share of the block energy in the two tones, in sixteenths (8 = half, the default; 4 = a quarter tolerates more noise) */ } ba_dtmf_t;

void ba_dtmf_init(ba_dtmf_t *d);
/* Feed one sample. Returns 1 when a block just finished; ba_dtmf_block_digit() then holds that block's raw decision. */
int ba_dtmf_push(ba_dtmf_t *d, int16_t x);
char ba_dtmf_block_digit(const ba_dtmf_t *d); /* the digit of the block just finished, or 0 */
/* Debounced stream decoder: call after each finished block. Returns a digit once, when two blocks in a row agree; re-arms on a block with no digit. */
char ba_dtmf_stream(ba_dtmf_t *d);

#endif
