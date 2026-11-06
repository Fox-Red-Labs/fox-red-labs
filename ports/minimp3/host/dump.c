#include "minimp3.h"
#include <stdio.h>
int mp3_dump(FILE *o, const uint8_t *mp3, int size)
{
    static mp3dec_t dec; static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]; mp3dec_frame_info_t info; int pos = 0;
    mp3dec_init(&dec);
    while (pos < size) { int n = mp3dec_decode_frame(&dec, mp3 + pos, size - pos, pcm, &info); if (!info.frame_bytes) break; pos += info.frame_bytes; if (n > 0) fwrite(pcm, 2, n * info.channels, o); }
    return 0;
}
