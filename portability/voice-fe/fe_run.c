#include <string.h>
#include "fe_run.h"
#include "fe_data.h"
#include "beamformer.h"
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"

typedef struct { bf_t bf; int16_t bfo[FE_FRAME], aec[FE_FRAME], out[FE_FRAME]; } fe_ws_t;
static fe_ws_t ws;                                     /* the front end's own static buffers */
static uint16_t hist[FE_HBINS + 1];                    /* bench only */
volatile uint32_t fe_done;

static uint32_t crc_tab[256];
static void crc_init(void)
{
	for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_tab[i] = c; }
}
static uint32_t crc16s(uint32_t c, const int16_t *x, int n)   /* CRC-32 of the samples as little-endian bytes */
{
	for (int i = 0; i < n; i++) {
		uint16_t v = (uint16_t)x[i];
		c = crc_tab[(c ^ v) & 0xFF] ^ (c >> 8);
		c = crc_tab[(c ^ (v >> 8)) & 0xFF] ^ (c >> 8);
	}
	return c;
}
static uint64_t energy(const int16_t *x, int n) { uint64_t e = 0; for (int i = 0; i < n; i++) e += (uint64_t)((int32_t)x[i] * x[i]); return e; }
#define H32(v) (unsigned long)((uint64_t)(v) >> 32), (unsigned long)((uint64_t)(v) & 0xFFFFFFFFu)

int fe_main(uint32_t tick_hz, fe_sink_fn sink)
{
	crc_init();
	int rate = FE_RATE, on = 1;
	SpeexEchoState *st = speex_echo_state_init(FE_FRAME, FE_TAIL);
	unsigned heap_echo = fe_arena_used;
	speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);
	SpeexPreprocessState *pp = speex_preprocess_state_init(FE_FRAME, FE_RATE);
	speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_DENOISE, &on);
	speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_ECHO_STATE, st);
	unsigned heap_init = fe_arena_used;
	bf_init(&ws.bf, fe_bf_taps);
	const int nf = FE_CLIP / FE_FRAME;
	plat_log("FE,CFG,frame=%d,tail=%d,rate=%d,clip=%d,loops=%d,tick_hz=%lu,ch=%d,bf_taps=%d\r\n", FE_FRAME, FE_TAIL, FE_RATE, FE_CLIP, FE_LOOPS, (unsigned long)tick_hz, BF_CH, BF_TAPS);
	uint64_t s_bf = 0, s_aec = 0, s_ns = 0, s_tot = 0;
	uint32_t mx = 0, mx_i = 0, mx1 = 0, mn = 0xFFFFFFFFu, bin = tick_hz / 10000u;
	for (int L = 0; L < FE_LOOPS; L++) {
		uint32_t c_out = 0xFFFFFFFFu, c_aec = 0xFFFFFFFFu; uint64_t e_in = 0, e_aec = 0, e_out = 0;
		for (int f = 0; f < nf; f++) {
			const int16_t *mic = fe_mic + (uint32_t)f * FE_FRAME * BF_CH, *far = fe_far + (uint32_t)f * FE_FRAME;
			uint32_t t0 = plat_ticks();
			bf_process(&ws.bf, mic, ws.bfo, FE_FRAME);
			uint32_t t1 = plat_ticks();
			speex_echo_cancellation(st, ws.bfo, far, ws.aec);
			uint32_t t2 = plat_ticks();
			memcpy(ws.out, ws.aec, sizeof ws.out);
			speex_preprocess_run(pp, ws.out);
			uint32_t t3 = plat_ticks();
			uint32_t d = t3 - t0;
			s_bf += t1 - t0; s_aec += t2 - t1; s_ns += t3 - t2; s_tot += d;
			if (d > mx) { mx = d; mx_i = (uint32_t)(L * nf + f); }
			if (L > 0 && d > mx1) mx1 = d;
			if (d < mn) mn = d;
			uint32_t b = bin ? d / bin : 0; hist[b > FE_HBINS ? FE_HBINS : b]++;
			c_out = crc16s(c_out, ws.out, FE_FRAME); c_aec = crc16s(c_aec, ws.aec, FE_FRAME);
			e_in += energy(ws.bfo, FE_FRAME); e_aec += energy(ws.aec, FE_FRAME); e_out += energy(ws.out, FE_FRAME);
			if (sink) sink(ws.aec, ws.out, FE_FRAME);
		}
		plat_log("FE,L,%d,%08lx,%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx\r\n", L, (unsigned long)~c_out, (unsigned long)~c_aec, H32(e_in), H32(e_aec), H32(e_out));
	}
	uint32_t frames = (uint32_t)(FE_LOOPS * nf);
	plat_log("FE,T,%lu,%08lx%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx,%lu,%lu,%lu,%lu\r\n", (unsigned long)frames, H32(s_bf), H32(s_aec), H32(s_ns), H32(s_tot),
		(unsigned long)mx, (unsigned long)mx_i, (unsigned long)mx1, (unsigned long)mn);
	for (int i = 0; i <= FE_HBINS; i++) if (hist[i]) plat_log("FE,H,%d,%u\r\n", i, (unsigned)hist[i]);
	plat_log("FE,MEM,heap=%u,heap_echo=%u,heap_after=%u,arena=%u,ws=%u,msgs=%u,fatal=%u,fail=%u\r\n", heap_init, heap_echo, (unsigned)fe_arena_used, fe_arena_size,
		(unsigned)sizeof ws, (unsigned)fe_speex_msgs, (unsigned)fe_speex_fatal, (unsigned)fe_arena_fail);
	plat_log("FE,DONE\r\n");
	fe_done = 0x46454430u;
	return 0;
}
