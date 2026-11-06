#include <string.h>
#include "opus.h"
#include "opusrun.h"
/* ---- Test signal: same generator as ports/lc3/common/lc3x.c, one second, integer arithmetic only. ---- */
typedef struct { uint32_t ph, f, df, bz, lcg, nz; int hz; } gen_t;
static int32_t psin(uint32_t ph) { int32_t x = (int32_t)(ph >> 16) - 32768, a = x < 0 ? -x : x, t = a - 16384, y = 16384 - ((t * t) >> 14); return x < 0 ? -y : y; }
static void gen_init(gen_t *g, int hz)
{
    memset(g, 0, sizeof *g); g->hz = hz; g->lcg = 12345u;
    uint64_t f0 = ((uint64_t)100u << 32) / (uint32_t)hz, f1 = ((uint64_t)45u << 32) / 100u;
    g->f = (uint32_t)f0; g->df = (uint32_t)((f1 - f0) / (uint64_t)(hz / 2));
}
static int16_t gen_next(gen_t *g, int n)
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

#ifndef OP_ENC_MEM
#define OP_ENC_MEM 60000
#endif
#ifndef OP_DEC_MEM
#define OP_DEC_MEM 30000
#endif
static uint32_t enc_mem[OP_ENC_MEM / 4], dec_mem[OP_DEC_MEM / 4];
static uint8_t pkts[16000];                  /* all packets of a run, back to back */
static uint16_t plen[100];
static int16_t pcm_in[960], pcm_out[960];
#ifdef OP_KEEP                               /* keep the whole second of input and output for a quality check */
int16_t op_keep_in[48000], op_keep_out[48000];
#endif

void oprun(int hz, int frame_us, int kbps, int app_kind, int cplx, oprun_t *r, uint32_t *enc_crc, uint32_t *dec_crc, uint32_t (*now)(void))
{
    const int ns = (int)((int64_t)hz * frame_us / 1000000), frames = 1000000 / frame_us;
    static const int apps[3] = { OPUS_APPLICATION_VOIP, OPUS_APPLICATION_AUDIO, OPUS_APPLICATION_RESTRICTED_LOWDELAY };
    memset(r, 0, sizeof *r); r->hz = hz; r->frame_us = frame_us; r->kbps = kbps; r->app = app_kind; r->complexity = cplx;
    r->enc_min = r->dec_min = r->bytes_min = 0xFFFFFFFFu;
    r->enc_size = opus_encoder_get_size(1); r->dec_size = opus_decoder_get_size(1);
    if (r->enc_size > sizeof enc_mem || r->dec_size > sizeof dec_mem) { r->errors = 1000; return; }
    OpusEncoder *enc = (OpusEncoder *)enc_mem; OpusDecoder *dec = (OpusDecoder *)dec_mem;
    if (opus_encoder_init(enc, hz, 1, apps[app_kind]) != OPUS_OK || opus_decoder_init(dec, hz, 1) != OPUS_OK) { r->errors = 1001; return; }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(kbps * 1000));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(cplx));
    opus_encoder_ctl(enc, OPUS_SET_VBR(0));                        /* constant bit rate, like LC3 */
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
    opus_encoder_ctl(enc, OPUS_SET_DTX(0));
    opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(0));
    opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(frame_us == 20000 ? OPUS_FRAMESIZE_20_MS : OPUS_FRAMESIZE_10_MS));
    opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(hz == 16000 ? OPUS_BANDWIDTH_WIDEBAND : OPUS_BANDWIDTH_FULLBAND));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(app_kind == 0 ? OPUS_SIGNAL_VOICE : OPUS_SIGNAL_MUSIC));
    gen_t g; gen_init(&g, hz);
    uint64_t esum = 0, dsum = 0; uint32_t bc = 0xFFFFFFFFu; int off = 0;
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < ns; i++) pcm_in[i] = gen_next(&g, f * ns + i);
#ifdef OP_KEEP
        for (int i = 0; i < ns; i++) op_keep_in[f * ns + i] = pcm_in[i];
#endif
        uint32_t t0 = now ? now() : 0;
        int n = opus_encode(enc, pcm_in, ns, pkts + off, 1275);
        uint32_t c = now ? now() - t0 : 0;
        if (n < 1 || off + n > (int)sizeof pkts) { r->errors++; break; }
        plen[f] = (uint16_t)n; esum += c; if (c < r->enc_min) r->enc_min = c; if (c > r->enc_max) r->enc_max = c;
        bc = crc_upd(bc, pkts + off, n); if (enc_crc) enc_crc[f] = ~crc_upd(0xFFFFFFFFu, pkts + off, n);
        { unsigned toc = pkts[off]; unsigned cfg = toc >> 3; if (f == 0) r->toc0 = toc; if (cfg < 12) r->n_silk++; else if (cfg < 16) r->n_hybrid++; else r->n_celt++; }
        r->bytes_total += n; if ((uint32_t)n < r->bytes_min) r->bytes_min = n; if ((uint32_t)n > r->bytes_max) r->bytes_max = n;
        off += n;
    }
    r->bits_crc = ~bc;
    uint32_t pc = 0xFFFFFFFFu; off = 0;
    for (int f = 0; f < frames; f++) {
        uint32_t t0 = now ? now() : 0;
        int n = opus_decode(dec, pkts + off, plen[f], pcm_out, ns, 0);
        uint32_t c = now ? now() - t0 : 0;
        if (n != ns) { r->errors++; }
#ifdef OP_KEEP
        for (int i = 0; i < ns; i++) op_keep_out[f * ns + i] = pcm_out[i];
#endif
        dsum += c; if (c < r->dec_min) r->dec_min = c; if (c > r->dec_max) r->dec_max = c;
        uint8_t le[1920]; for (int i = 0; i < ns; i++) { le[2 * i] = (uint8_t)pcm_out[i]; le[2 * i + 1] = (uint8_t)((uint16_t)pcm_out[i] >> 8); }
        pc = crc_upd(pc, le, 2 * ns); off += plen[f]; if (dec_crc) dec_crc[f] = ~crc_upd(0xFFFFFFFFu, le, 2 * ns);
    }
    r->pcm_crc = ~pc; r->frames = frames;
    r->enc_lo = (uint32_t)esum; r->enc_hi = (uint32_t)(esum >> 32); r->dec_lo = (uint32_t)dsum; r->dec_hi = (uint32_t)(dsum >> 32);
    r->magic = 0x4F505252u;
}
