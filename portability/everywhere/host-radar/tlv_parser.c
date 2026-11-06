#include "tlv_parser.h"
#include <math.h>
#include <string.h>

enum { S_SYNC, S_HDR, S_TLVHDR, S_PAYLOAD, S_TAIL };
static const uint8_t MAGIC[8] = {2, 1, 4, 3, 6, 5, 8, 7};

void tlv_init(tlv_parser_t *p) { memset(p, 0, sizeof *p); p->st = S_SYNC; }

static uint32_t u32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

static int finish(tlv_parser_t *p)
{
    tlv_frame_t *f = &p->f;
    f->rmin_mm = (f->points && p->rmin2 >= 0.f) ? (int32_t)(sqrtf(p->rmin2) * 1000.f + 0.5f) : -1;
    p->st = S_SYNC; p->m = 0; p->frames_done++;
    return 1;
}

static void tlv_start_header(tlv_parser_t *p)
{
    p->st = S_TLVHDR; p->thn = 0;
}

static void take_payload(tlv_parser_t *p, uint8_t b)
{
    uint32_t k = p->tgot;
    if (p->ttype == 315 && k < 2) p->p3[k] = b;
    if (p->ttype == 315 && k == 0) p->f.pres_b0 = b;
    if (p->ttype == 315 && k == 1) p->f.pres_z0 = b;
    if (p->ttype == 1) {
        p->pt[k & 15u] = b;
        if ((k & 15u) == 15u) {
            float x, y, z; memcpy(&x, p->pt, 4); memcpy(&y, p->pt + 4, 4); memcpy(&z, p->pt + 8, 4);
            float r2 = x * x + y * y + z * z;
            if (p->f.points == 0 || r2 < p->rmin2) p->rmin2 = r2;
            p->f.points++;
        }
    }
    p->tgot++;
}

int tlv_feed(tlv_parser_t *p, uint8_t b)
{
    switch (p->st) {
    case S_SYNC:
        if (b == MAGIC[p->m]) { p->hdr[p->m] = b; if (++p->m == 8) { p->st = S_HDR; p->hn = 8; } }
        else if (b == MAGIC[0]) { p->hdr[0] = b; p->m = 1; }
        else p->m = 0;
        return 0;
    case S_HDR:
        p->hdr[p->hn++] = b;
        if (p->hn < 40) return 0;
        {
            uint32_t len = u32(p->hdr + 12);
            if (len < 40u || len > 65536u) {      /* not a header: resume the scan just after the magic word */
                uint8_t saved[32]; memcpy(saved, p->hdr + 8, 32);
                p->st = S_SYNC; p->m = 0;
                for (int i = 0; i < 32; i++) if (tlv_feed(p, saved[i])) return 1;
                return 0;
            }
            tlv_frame_t *f = &p->f;
            f->version = u32(p->hdr + 8); f->length = len; f->platform = u32(p->hdr + 16); f->frame = u32(p->hdr + 20);
            f->cycles = u32(p->hdr + 24); f->objs = u32(p->hdr + 28); f->ntlv = u32(p->hdr + 32); f->subframe = u32(p->hdr + 36);
            f->pres_b0 = f->pres_z0 = -1; f->points = 0; f->rmin_mm = -1;
            p->rmin2 = 0.f; p->left = len - 40u; p->tidx = 0;
            if (f->ntlv == 0 || p->left < 8) { if (p->left == 0) return finish(p); p->st = S_TAIL; return 0; }
            tlv_start_header(p);
        }
        return 0;
    case S_TLVHDR:
        p->th[p->thn++] = b; p->left--;
        if (p->thn < 8) { if (p->left == 0) return finish(p); return 0; }
        p->ttype = u32(p->th); p->tlen = u32(p->th + 4);
        p->tidx++; p->tgot = 0;
        if (p->tlen > p->left) p->tlen = p->left;
        if (p->tlen == 0) goto next_tlv;
        p->st = S_PAYLOAD;
        return 0;
    case S_PAYLOAD:
        take_payload(p, b); p->left--;
        if (p->tgot < p->tlen) return 0;
    next_tlv:
        if (p->tidx >= p->f.ntlv || p->left < 8) { if (p->left == 0) return finish(p); p->st = S_TAIL; return 0; }
        tlv_start_header(p);
        return 0;
    case S_TAIL:
        if (--p->left == 0) return finish(p);
        return 0;
    }
    return 0;
}
