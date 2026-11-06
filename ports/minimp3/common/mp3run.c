#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "mp3run.h"
#include <string.h>

static mp3dec_t dec;   /* about 6.6 kB */
static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

static uint32_t fnv(uint32_t h, const int16_t *s, int n)
{
    for (int i = 0; i < n; i++) { uint16_t v = (uint16_t)s[i]; h = (h ^ (v & 0xFF)) * 16777619u; h = (h ^ (v >> 8)) * 16777619u; }
    return h;
}

void mp3run(const uint8_t *mp3, int size, mp3run_t *r, mp3detail_t *d, uint32_t (*now)(void))
{
    mp3dec_frame_info_t info; int pos = 0; uint64_t tot = 0; uint32_t h = 2166136261u;
    memset(r, 0, sizeof *r); r->cyc_min = 0xFFFFFFFFu;
    mp3dec_init(&dec);
    while (pos < size) {
#ifdef MP3_STOP_AT
        if (r->frames >= MP3_STOP_AT) break;   /* emulator builds decode only the first frames */
#endif
        uint32_t t0 = now ? now() : 0;
        int n = mp3dec_decode_frame(&dec, mp3 + pos, size - pos, pcm, &info);
        uint32_t dt = now ? now() - t0 : 0;
#ifdef MP3_TIMER_MASK
        dt &= MP3_TIMER_MASK;   /* a counter narrower than 32 bits (SysTick is 24) */
#endif
        if (info.frame_bytes == 0) break;
        pos += info.frame_bytes;
        if (n > 0 && r->frames < MP3_MAXFRAMES) {
            int ns = n * info.channels;
            d->hash[r->frames] = fnv(2166136261u, pcm, ns); d->cyc[r->frames] = dt;
            if (r->frames == 0) memcpy(d->pcm0, pcm, ns * sizeof(int16_t));
            memcpy(d->pcm_last, pcm, ns * sizeof(int16_t));
            h = fnv(h, pcm, ns); tot += dt;
            if (dt < r->cyc_min) r->cyc_min = dt;
            if (dt > r->cyc_max) r->cyc_max = dt;
            r->frames++; r->samples_per_ch += n; r->channels = info.channels; r->hz = info.hz; r->kbps = info.bitrate_kbps;
        }
    }
    r->hash_all = h; r->cycles_lo = (uint32_t)tot; r->cycles_hi = (uint32_t)(tot >> 32); r->bytes_used = pos; r->magic = 0x4D503352u;
}
