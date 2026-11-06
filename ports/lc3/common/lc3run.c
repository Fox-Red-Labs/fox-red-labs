#include <string.h>
#include "lc3.h"
#include "lc3run.h"

/* ---- Test signal: integer arithmetic only, so the Mac and the board generate identical samples. ----
 * Frames 0 to 49: a sine sweep from 100 Hz to 0.45 of the sample rate (parabolic sine, so it has harmonics).
 * Frames 50 to 99: speech-like: a 120 Hz pitched buzz with a 4 Hz syllable envelope plus low-passed noise. */
typedef struct { uint32_t ph, f, df, bz, lcg, nz; int hz; } gen_t;
static int32_t psin(uint32_t ph)       /* parabolic sine of a 32-bit phase, result in +-16384 */
{
    int32_t x = (int32_t)(ph >> 16) - 32768;          /* -32768..32767 across the cycle */
    int32_t a = x < 0 ? -x : x;                       /* 0..32768, triangle */
    int32_t t = a - 16384;                            /* -16384..16384 */
    int32_t y = 16384 - ((t * t) >> 14) ;             /* 0 at the ends, 16384 at the middle (parabola) */
    return (x < 0) ? -y : y;
}
static void gen_init(gen_t *g, int hz)
{
    memset(g, 0, sizeof *g); g->hz = hz; g->lcg = 12345u;
    uint64_t f0 = ((uint64_t)100u << 32) / (uint32_t)hz, f1 = ((uint64_t)45u << 32) / 100u;
    g->f = (uint32_t)f0; g->df = (uint32_t)((f1 - f0) / ((uint64_t)(LC3_BENCH_FRAMES / 2) * (uint32_t)(hz / 100)));
}
static int16_t gen_next(gen_t *g, int n)
{
    int32_t v;
    if (n < LC3_BENCH_FRAMES / 2 * (g->hz / 100)) {
        v = psin(g->ph); g->ph += g->f; g->f += g->df;
    } else {
        int m = n - LC3_BENCH_FRAMES / 2 * (g->hz / 100);
        g->bz += (uint32_t)(((uint64_t)120u << 32) / (uint32_t)g->hz);
        int32_t b = psin(g->bz) + (psin(g->bz * 2u) >> 1) + (psin(g->bz * 3u) >> 2);   /* harmonics */
        int ph4 = (int)(((uint64_t)m * 4u * 65536u / (uint32_t)g->hz) & 0xFFFF);
        int32_t env = ph4 < 32768 ? ph4 : 65535 - ph4;          /* 4 Hz triangle, 0..32767 */
        g->lcg = g->lcg * 1664525u + 1013904223u;
        int32_t w = (int32_t)(g->lcg >> 16) - 32768;
        g->nz += (uint32_t)((w - (int32_t)g->nz) >> 2);         /* one-pole low-pass */
        v = (int32_t)(((int64_t)b * env) >> 15) / 2 + ((int32_t)g->nz >> 3);
    }
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}
static uint32_t crc_upd(uint32_t c, const uint8_t *p, int n)   /* CRC-32, IEEE, bitwise (outside the timed region) */
{
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    return c;
}
/* codec state: sized for 10 ms at 48 kHz, one channel */
static LC3_ENCODER_MEM_T(10000, 48000) enc_mem;
static LC3_DECODER_MEM_T(10000, 48000) dec_mem;
static uint8_t bits[LC3_BENCH_FRAMES * LC3_BENCH_MAXBYTES];
static int16_t pcm_in[480], pcm_out[480];
#if defined(LC3_KEEP) || defined(LC3_KEEP_OUT)   /* keep the decoded output of the latest configuration (and with LC3_KEEP, the input) for a sample-by-sample check */
int16_t keep_out[48000];
#endif
#ifdef LC3_KEEP
int16_t keep_in[48000];
#endif

void lc3run(int hz, int kbps, lc3res_t *r, lc3detail_t *d, uint32_t (*now)(void))
{
    const int dt = 10000, ns = hz / 100, nb = lc3_frame_bytes(dt, kbps * 1000);
    gen_t g; gen_init(&g, hz);
    memset(r, 0, sizeof *r); r->hz = hz; r->kbps = kbps; r->nbytes = nb; r->enc_min = r->dec_min = 0xFFFFFFFFu;
    r->enc_size = lc3_encoder_size(dt, hz); r->dec_size = lc3_decoder_size(dt, hz);
    lc3_encoder_t enc = lc3_setup_encoder(dt, hz, hz, &enc_mem);
    lc3_decoder_t dec = lc3_setup_decoder(dt, hz, hz, &dec_mem);
    if (!enc || !dec || nb > LC3_BENCH_MAXBYTES) { r->errors = 1000; return; }
    uint64_t esum = 0, dsum = 0; uint32_t bc = 0xFFFFFFFFu;
    for (int f = 0; f < LC3_BENCH_FRAMES; f++) {                    /* encode pass */
        for (int i = 0; i < ns; i++) pcm_in[i] = gen_next(&g, f * ns + i);
#ifdef LC3_KEEP
        for (int i = 0; i < ns; i++) keep_in[f * ns + i] = pcm_in[i];
#endif
        uint8_t *o = bits + f * nb; uint32_t t0 = now ? now() : 0;
        int rc = lc3_encode(enc, LC3_PCM_FORMAT_S16, pcm_in, 1, nb, o);
        uint32_t c = now ? now() - t0 : 0; if (rc) r->errors++;
        d->enc_cyc[f] = c; esum += c; if (c < r->enc_min) r->enc_min = c; if (c > r->enc_max) r->enc_max = c;
        d->enc_crc[f] = ~crc_upd(0xFFFFFFFFu, o, nb); bc = crc_upd(bc, o, nb);
    }
    r->bits_crc = ~bc;
    uint32_t pc = 0xFFFFFFFFu;
    for (int f = 0; f < LC3_BENCH_FRAMES; f++) {                    /* decode pass, from the stored bitstream */
        uint32_t t0 = now ? now() : 0;
        int rc = lc3_decode(dec, bits + f * nb, nb, LC3_PCM_FORMAT_S16, pcm_out, 1);
        uint32_t c = now ? now() - t0 : 0; if (rc) r->errors++;
        d->dec_cyc[f] = c; dsum += c; if (c < r->dec_min) r->dec_min = c; if (c > r->dec_max) r->dec_max = c;
#if defined(LC3_KEEP) || defined(LC3_KEEP_OUT)
        for (int i = 0; i < ns; i++) keep_out[f * ns + i] = pcm_out[i];
#endif
        uint8_t le[960]; for (int i = 0; i < ns; i++) { le[2 * i] = (uint8_t)pcm_out[i]; le[2 * i + 1] = (uint8_t)((uint16_t)pcm_out[i] >> 8); }
        d->dec_crc[f] = ~crc_upd(0xFFFFFFFFu, le, 2 * ns); pc = crc_upd(pc, le, 2 * ns);
    }
    r->pcm_crc = ~pc; r->frames = LC3_BENCH_FRAMES;
    r->enc_lo = (uint32_t)esum; r->enc_hi = (uint32_t)(esum >> 32); r->dec_lo = (uint32_t)dsum; r->dec_hi = (uint32_t)(dsum >> 32);
    r->magic = 0x4C433352u;
}
