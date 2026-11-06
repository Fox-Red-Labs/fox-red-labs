/*
 * heatshrink (static allocation, window 8 bits, lookahead 4 bits, 32 byte input buffer) on 2 KB of
 * English-like text. Split out of the original text image (L1306 port).
 */
#include <string.h>
#include "tb.h"
#include "heatshrink_encoder.h"
#include "heatshrink_decoder.h"
#include "english2k.h"

#define IN_SZ 2048U
static uint8_t comp[IN_SZ + IN_SZ / 2U + 8U];
static uint8_t decomp[IN_SZ + 16U];
static heatshrink_encoder hse;
static heatshrink_decoder hsd;

static uint32_t hs_compress(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap, int *ok)
{
	size_t count = 0;
	uint32_t sunk = 0, polled = 0;
	heatshrink_encoder_reset(&hse);
	while (sunk < n) {
		if (heatshrink_encoder_sink(&hse, (uint8_t *)&in[sunk], n - sunk, &count) < 0) { *ok = 0; return 0; }
		sunk += (uint32_t)count;
		if (sunk == n) heatshrink_encoder_finish(&hse);
		HSE_poll_res pres;
		do {
			pres = heatshrink_encoder_poll(&hse, &out[polled], cap - polled, &count);
			if (pres < 0) { *ok = 0; return 0; }
			polled += (uint32_t)count;
		} while (pres == HSER_POLL_MORE);
		if (sunk == n && heatshrink_encoder_finish(&hse) != HSER_FINISH_DONE) { *ok = 0; }
	}
	return polled;
}

static uint32_t hs_expand(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap, int *ok)
{
	size_t count = 0;
	uint32_t sunk = 0, polled = 0;
	heatshrink_decoder_reset(&hsd);
	while (sunk < n) {
		if (heatshrink_decoder_sink(&hsd, (uint8_t *)&in[sunk], n - sunk, &count) < 0) { *ok = 0; return 0; }
		sunk += (uint32_t)count;
		if (sunk == n) heatshrink_decoder_finish(&hsd);
		HSD_poll_res pres;
		do {
			pres = heatshrink_decoder_poll(&hsd, &out[polled], cap - polled, &count);
			if (pres < 0) { *ok = 0; return 0; }
			polled += (uint32_t)count;
		} while (pres == HSDR_POLL_MORE);
		if (sunk == n && heatshrink_decoder_finish(&hsd) != HSDR_FINISH_DONE) { *ok = 0; }
	}
	return polled;
}

int main(void)
{
	uint32_t c, clen = 0, dlen = 0;
	int ok;
	TB_NOTE(nt);

	tb_start("text_hs");

	/* ---- heatshrink ---- */
	ok = 1;
	TB_BEST(c, 3, clen = hs_compress(english2k, IN_SZ, comp, sizeof comp, &ok));
	tb_ns(&nt, "in=2048;out="); tb_nu(&nt, clen);
	tb_ns(&nt, ";ratio_pct="); tb_nu(&nt, clen * 100U / IN_SZ);
	tb_ns(&nt, ";W8_L4_static");
	tb_result("heatshrink", "compress_2k", c, ok && clen > 0 && clen < IN_SZ, nt.b);

	memset(decomp, 0, sizeof decomp);
	ok = 1;
	TB_BEST(c, 3, dlen = hs_expand(comp, clen, decomp, sizeof decomp, &ok));
	{
		TB_NOTE(n2);
		tb_ns(&n2, "out="); tb_nu(&n2, dlen);
		tb_result("heatshrink", "decompress_2k", c, ok && dlen == IN_SZ && !memcmp(decomp, english2k, IN_SZ), n2.b);
	}

	tb_done();
}
