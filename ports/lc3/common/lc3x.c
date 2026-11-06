#include <string.h>
#include "lc3.h"
#include "lc3x.h"
#define MAXF 400
#define MAXBITS 28000
typedef struct { uint32_t ph, f, df, bz, lcg, nz; int hz; } gen_t;
static int32_t psin(uint32_t ph) { int32_t x = (int32_t)(ph >> 16) - 32768, a = x < 0 ? -x : x, t = a - 16384, y = 16384 - ((t * t) >> 14); return x < 0 ? -y : y; }
static void gen_init(gen_t *g, int hz, int ch)
{
    memset(g, 0, sizeof *g); g->hz = hz; g->lcg = ch ? 54321u : 12345u; g->bz = ch ? 0x40000000u : 0; g->ph = ch ? 0x20000000u : 0;
    uint64_t f0 = ((uint64_t)100u << 32) / (uint32_t)hz, f1 = ((uint64_t)45u << 32) / 100u;
    g->f = (uint32_t)f0; g->df = (uint32_t)((f1 - f0) / (uint64_t)(hz / 2));
}
static int16_t gen_next(gen_t *g, int n)       /* first half second: sweep; second half second: speech-like */
{
    int32_t v;
    if (n < g->hz / 2) { v = psin(g->ph); g->ph += g->f; g->f += g->df; }
    else {
        int m = n - g->hz / 2;
        g->bz += (uint32_t)(((uint64_t)120u << 32) / (uint32_t)g->hz);
        int32_t b = psin(g->bz) + (psin(g->bz * 2u) >> 1) + (psin(g->bz * 3u) >> 2);
        int ph4 = (int)(((uint64_t)m * 4u * 65536u / (uint32_t)g->hz) & 0xFFFF);
        int32_t env = ph4 < 32768 ? ph4 : 65535 - ph4;
        g->lcg = g->lcg * 1664525u + 1013904223u;
        int32_t w = (int32_t)(g->lcg >> 16) - 32768;
        g->nz += (uint32_t)((w - (int32_t)g->nz) >> 2);
        v = (int32_t)(((int64_t)b * env) >> 15) / 2 + ((int32_t)g->nz >> 3);
    }
    if (v > 32767) v = 32767; if (v < -32768) v = -32768; return (int16_t)v;
}
static uint32_t crc_upd(uint32_t c, const uint8_t *p, int n) { while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); } return c; }
static LC3_ENCODER_MEM_T(10000, 48000) enc_mem[2];
static LC3_DECODER_MEM_T(10000, 48000) dec_mem[2];
static uint8_t bits[MAXBITS];
static int16_t pcm_in[480 * 2], pcm_out[480 * 2];

void lc3xrun(int dt, int hz, int kbps, int nch, int loss_every, lc3xres_t *r, uint32_t (*now)(void))
{
    const int ns = (int)((int64_t)hz * dt / 1000000), nb = lc3_frame_bytes(dt, kbps * 1000), frames = 1000000 / dt;
    gen_t g[2]; gen_init(&g[0], hz, 0); gen_init(&g[1], hz, 1);
    memset(r, 0, sizeof *r); r->dt_us = dt; r->hz = hz; r->kbps = kbps; r->nch = nch; r->loss_every = loss_every; r->nbytes = nb;
    r->enc_min = r->dec_min = 0xFFFFFFFFu; r->enc_size = lc3_encoder_size(dt, hz); r->dec_size = lc3_decoder_size(dt, hz);
    lc3_encoder_t enc[2]; lc3_decoder_t dec[2];
    if (frames > MAXF || nb * nch * frames > MAXBITS || r->enc_size > sizeof enc_mem[0] || r->dec_size > sizeof dec_mem[0]) { r->errors = 1000; return; }
    for (int c = 0; c < nch; c++) {
        enc[c] = lc3_setup_encoder(dt, hz, hz, &enc_mem[c]); dec[c] = lc3_setup_decoder(dt, hz, hz, &dec_mem[c]);
        if (!enc[c] || !dec[c]) { r->errors = 1001; return; }
    }
    uint64_t esum = 0, dsum = 0, gsum = 0, lsum = 0; uint32_t bc = 0xFFFFFFFFu;
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < ns; i++) for (int c = 0; c < nch; c++) pcm_in[i * nch + c] = gen_next(&g[c], f * ns + i);
        uint32_t tot = 0;
        for (int c = 0; c < nch; c++) {
            uint8_t *o = bits + (f * nch + c) * nb; uint32_t t0 = now ? now() : 0;
            int rc = lc3_encode(enc[c], LC3_PCM_FORMAT_S16, pcm_in + c, nch, nb, o);
            tot += now ? now() - t0 : 0; if (rc) r->errors++;
        }
        esum += tot; if (tot < r->enc_min) r->enc_min = tot; if (tot > r->enc_max) r->enc_max = tot;
        bc = crc_upd(bc, bits + f * nch * nb, nch * nb);
    }
    r->bits_crc = ~bc;
    uint32_t pc = 0xFFFFFFFFu;
    for (int f = 0; f < frames; f++) {
        int lost = loss_every && (f % loss_every) == loss_every - 1; uint32_t tot = 0;
        for (int c = 0; c < nch; c++) {
            uint32_t t0 = now ? now() : 0;
            int rc = lc3_decode(dec[c], lost ? 0 : bits + (f * nch + c) * nb, lost ? 0 : nb, LC3_PCM_FORMAT_S16, pcm_out + c, nch);
            tot += now ? now() - t0 : 0; if (rc < 0 || (rc && !lost)) r->errors++;
            if (lost && rc != 1) r->errors++;                       /* a concealed frame returns 1 */
        }
        if (f < frames / 2) r->dec_h1 += tot; else r->dec_h2 += tot;
        dsum += tot; if (tot < r->dec_min) r->dec_min = tot; if (tot > r->dec_max) r->dec_max = tot;
        if (lost) { lsum += tot; r->lost_n++; } else { gsum += tot; r->good_n++; }
        uint8_t le[960 * 2]; for (int i = 0; i < ns * nch; i++) { le[2 * i] = (uint8_t)pcm_out[i]; le[2 * i + 1] = (uint8_t)((uint16_t)pcm_out[i] >> 8); }
        pc = crc_upd(pc, le, 2 * ns * nch);
    }
    r->pcm_crc = ~pc; r->frames = frames;
    r->enc_lo = (uint32_t)esum; r->enc_hi = (uint32_t)(esum >> 32); r->dec_lo = (uint32_t)dsum; r->dec_hi = (uint32_t)(dsum >> 32);
    r->good_lo = (uint32_t)gsum; r->good_hi = (uint32_t)(gsum >> 32); r->lost_lo = (uint32_t)lsum; r->lost_hi = (uint32_t)(lsum >> 32);
    r->magic = 0x4C583352u;
}
