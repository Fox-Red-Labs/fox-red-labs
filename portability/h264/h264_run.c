/*
 * Decode compiled-in Annex B baseline streams with h264bsd (Apache-2.0, from the Android project; wrapper MIT), one NAL unit at a time (as a network would
 * deliver them), and for every picture: time the decoder call, CRC the YUV frame (the correctness check against the Mac), then convert it to RGB565
 * in strips (STRIP_BYTES at a time, never a whole RGB frame) and CRC the strips (stand-in for the display write).
 * Same file for the Mac and every board; the platform supplies plat_ticks() and plat_log().
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "basetype.h"
#include "h264bsd_util.h"
#include "h264bsd_decoder.h"
#include "h264_run.h"

#ifndef PASSES
#define PASSES 3
#endif
#ifndef STRIP_BYTES
#define STRIP_BYTES 10240
#endif
#ifndef MAX_REC
#define MAX_REC 24
#endif
extern void h_reset(void); extern void *h_malloc(size_t); extern void h_free(void *);
volatile struct h264_rec h264_rec[MAX_REC]; volatile uint32_t h264_nrec, h264_done;

static const uint32_t crc_nib[16] = { 0x00000000u,0x1DB71064u,0x3B6E20C8u,0x26D930ACu,0x76DC4190u,0x6B6B51F4u,0x4DB26158u,0x5005713Cu,
	0xEDB88320u,0xF00F9344u,0xD6D6A3E8u,0xCB61B38Cu,0x9B64C2B0u,0x86D3D2D4u,0xA00AE278u,0xBDBDF21Cu };
static uint32_t crc_upd(uint32_t crc, const uint8_t *p, size_t n)
{ crc = ~crc; while (n--) { crc ^= *p++; crc = crc_nib[crc & 15] ^ (crc >> 4); crc = crc_nib[crc & 15] ^ (crc >> 4); } return ~crc; }

static uint16_t strip[STRIP_BYTES / 2];
static inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
/* YCbCr 4:2:0 (BT.601, limited range) to RGB565 for rows y0..y0+n-1 of the cropped picture; cw = coded width */
static void yuv_strip(const uint8_t *pic, uint32_t cw, uint32_t ch, uint32_t left, uint32_t top, uint32_t W, uint32_t y0, uint32_t n)
{
	const uint8_t *Y = pic, *U = pic + cw * ch, *V = U + (cw / 2) * (ch / 2);
	uint16_t *o = strip;
	for (uint32_t y = y0; y < y0 + n; y++) {
		const uint8_t *yr = Y + (top + y) * cw + left, *ur = U + ((top + y) >> 1) * (cw / 2), *vr = V + ((top + y) >> 1) * (cw / 2);
		for (uint32_t x = 0; x < W; x++) {
			uint32_t cx = (left + x) >> 1; int c = (int)yr[x] - 16, d = (int)ur[cx] - 128, e = (int)vr[cx] - 128;
			int r = (298 * c + 409 * e + 128) >> 8, g = (298 * c - 100 * d - 208 * e + 128) >> 8, b = (298 * c + 516 * d + 128) >> 8;
			*o++ = (uint16_t)(((clamp8(r) & 0xF8) << 8) | ((clamp8(g) & 0xFC) << 3) | (clamp8(b) >> 3));
		}
	}
}

static uint32_t next_nal(const uint8_t *p, uint32_t len, uint32_t from)   /* offset of the next 00 00 01 (or 00 00 00 01) after from, or len */
{
	for (uint32_t i = from + 3; i + 2 < len; i++) if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1) return (p[i - 1] == 0 && i > from) ? i - 1 : i;
	return len;
}

int h264_main(void)
{
	h264_nrec = 0;
	for (unsigned si = 0; si < h264_nstreams; si++) {
		const uint8_t *src = h264_streams[si].data; uint32_t slen = h264_streams[si].len, maxnal = 0;
		for (uint32_t o = 0; o < slen;) { uint32_t e = next_nal(src, slen, o); if (e - o > maxnal) maxnal = e - o; o = e; }
		for (int pass = 0; pass < PASSES; pass++) {
			volatile struct h264_rec *r = &h264_rec[h264_nrec < MAX_REC ? h264_nrec : MAX_REC - 1];   /* the last slot is reused if MAX_REC is too small */
			storage_t dec; uint32_t all = 0, first = 0, last = 0, rgball = 0, pics = 0, conc = 0;
			uint32_t tIdr = 0, tP = 0, tPmax = 0, tRgb = 0, tTot = 0, W = 0, H = 0, status = 0, used_rd = 0, base = 0, inited = 0, inbuf = 0;
			h_reset();
			uint8_t *buf = (uint8_t *)h_malloc(maxnal + 4); inbuf = (uint32_t)h_cur; base = inbuf; h_peak = h_cur; h_nallocs = 0; /* input buffer is counted apart */
			if (!buf) { status = 3; goto rec; }
			if (h264bsdInit(&dec, 1) != HANTRO_OK) { status = 1; goto rec; }
			inited = 1;
			for (uint32_t off = 0; off < slen;) {
				uint32_t end = next_nal(src, slen, off), nal = end - off, len = (end + 4 <= slen) ? nal + 4 : nal, used = 0;
				memcpy(buf, src + off, len);   /* the next start code follows the NAL, so the decoder sees where it ends */            /* the decoder edits its input (emulation-prevention bytes) */
				uint32_t nal_dt = 0;
				while (len > 0) {
					uint32_t t0 = plat_ticks(); uint32_t rc = h264bsdDecode(&dec, buf + used, len, 0, &used_rd);
					uint32_t dt = plat_ticks() - t0; tTot += dt; nal_dt += dt; dt = nal_dt; len -= used_rd; used += used_rd; if (used >= nal) len = 0;
					if (rc == H264BSD_PIC_RDY) {
						uint32_t id, idr, err; uint8_t *pic = h264bsdNextOutputPicture(&dec, &id, &idr, &err);
						uint32_t cw = h264bsdPicWidth(&dec) * 16, ch = h264bsdPicHeight(&dec) * 16, cf, cl, cw2, ct, ch2; conc += err;
						h264bsdCroppingParams(&dec, &cf, &cl, &cw2, &ct, &ch2);   /* gives left offset, width, top offset, height in pixels */
						uint32_t left = cf ? cl : 0, top = cf ? ct : 0; W = cf ? cw2 : cw; H = cf ? ch2 : ch;
						uint32_t c = crc_upd(0, pic, (size_t)cw * ch * 3 / 2);
						if (pics == 0) { first = c; tIdr = dt; } else { tP += dt; if (dt > tPmax) tPmax = dt; }
						last = c; all = crc_upd(all, (uint8_t *)&c, 4); pics++;
						uint32_t rows = (STRIP_BYTES / 2 / W) & ~1u; if (rows > 16) rows = 16;
						for (uint32_t y = 0; y < H; y += rows) {
							uint32_t n = (y + rows <= H) ? rows : H - y, t1 = plat_ticks();
							yuv_strip(pic, cw, ch, left, top, W, y, n); tRgb += plat_ticks() - t1;
							rgball = crc_upd(rgball, (const uint8_t *)strip, (size_t)n * W * 2);
						}
					} else if (rc >= H264BSD_ERROR) { status = 2; conc = 1000 + rc; goto done; }   /* refused or broken stream: rc is stored as 1000 + code in the concealed field */
					else if (used_rd == 0 && rc != H264BSD_HDRS_RDY) break;
				}
				off = end;
			}
done:		if (inited) h264bsdShutdown(&dec);
rec:
			r->magic = 0; r->stream = si; r->pass = pass; r->w = W; r->h = H; r->frames = pics;
			r->crc_first = first; r->crc_last = last; r->crc_all = all; r->rgb_all = rgball;
			r->cyc_total = tTot; r->cyc_idr = tIdr; r->cyc_p = tP; r->cyc_pmax = tPmax; r->cyc_rgb = tRgb;
			r->peak_heap = (uint32_t)(h_peak - base); r->allocs = (uint32_t)h_nallocs; r->inbuf = inbuf; r->concealed = conc; r->status = status;
			r->magic = H264_REC_MAGIC; if (h264_nrec < MAX_REC) h264_nrec++;
			plat_log("H264R s=%s p=%d st=%u %ux%u fr=%u crc=%08x/%08x/%08x rgb=%08x cyc=%u idr=%u p=%u pmax=%u rgbc=%u heap=%u in=%u allocs=%u conc=%u\r\n",
				h264_streams[si].name, pass, (unsigned)status, (unsigned)W, (unsigned)H, (unsigned)pics, (unsigned)first, (unsigned)last, (unsigned)all, (unsigned)rgball,
				(unsigned)tTot, (unsigned)tIdr, (unsigned)tP, (unsigned)tPmax, (unsigned)tRgb, (unsigned)(h_peak - base), (unsigned)inbuf, (unsigned)h_nallocs, (unsigned)conc);
		}
	}
	return 0;
}
