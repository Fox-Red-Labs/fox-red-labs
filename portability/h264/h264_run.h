#ifndef H264_RUN_H
#define H264_RUN_H
#include <stdint.h>
#include <stddef.h>
struct h264_stream { const char *name; const unsigned char *data; unsigned len; };
extern const struct h264_stream h264_streams[]; extern const unsigned h264_nstreams;
/* One record per stream and pass. Read back over the debug probe on boards without a console (symbol h264_rec, count h264_nrec). */
struct h264_rec {
	uint32_t magic, stream, pass, w, h, frames;
	uint32_t crc_first, crc_last, crc_all, rgb_all;      /* CRC-32 of the YUV frames, and of the RGB565 strips */
	uint32_t cyc_total, cyc_idr, cyc_p, cyc_pmax, cyc_rgb; /* decoder calls only; RGB565 conversion apart */
	uint32_t peak_heap, allocs, inbuf, concealed, status;
};
#define H264_REC_MAGIC 0x48323634u
#define H264_DONE_MAGIC 0xD0D0264Du
extern volatile struct h264_rec h264_rec[]; extern volatile uint32_t h264_nrec, h264_done;
extern uint32_t plat_ticks(void);                  /* CPU cycles */
extern void plat_log(const char *fmt, ...);         /* console line, or nothing */
extern size_t h_cur, h_peak, h_nallocs;
#endif
