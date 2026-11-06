/* Decode one MP3 held in memory with minimp3, frame by frame, and record what a bench needs:
 * cycles spent inside mp3dec_decode_frame only (not the hashing), a 32-bit FNV-1a hash of every frame's PCM,
 * and the PCM of frame 0 so a host can compare sample by sample. The same file builds on the Mac and on the boards. */
#ifndef MP3RUN_H
#define MP3RUN_H
#include <stdint.h>
#define MP3_MAXFRAMES 256
typedef struct {
    uint32_t magic;            /* 0x4D503352 ("MP3R") when this clip's run finished */
    uint32_t frames;           /* frames that returned samples */
    uint32_t channels, hz, kbps;
    uint32_t samples_per_ch;   /* total PCM samples per channel */
    uint32_t hash_all;         /* FNV-1a over all PCM, int16 little-endian */
    uint32_t cycles_lo, cycles_hi;   /* sum of cycles inside the decode calls */
    uint32_t cyc_min, cyc_max;       /* fastest and slowest frame */
    uint32_t bytes_used;       /* bytes of the file consumed */
} mp3run_t;
typedef struct { uint32_t hash[MP3_MAXFRAMES]; uint32_t cyc[MP3_MAXFRAMES]; int16_t pcm0[2304]; int16_t pcm_last[2304]; } mp3detail_t;
/* now() returns a free-running 32-bit cycle count (wraps), or 0 on the Mac when timing is not wanted. */
void mp3run(const uint8_t *mp3, int size, mp3run_t *r, mp3detail_t *d, uint32_t (*now)(void));
#endif
