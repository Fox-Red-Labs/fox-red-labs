#include <string.h>
#include "beamformer.h"

void bf_init(bf_t *b, const int16_t taps[BF_CH][BF_TAPS])
{
	memset(b, 0, sizeof *b);
	memcpy(b->taps, taps, sizeof b->taps);
}

static int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

void bf_process(bf_t *b, const int16_t *mic, int16_t *out, int n)
{
	int32_t sum[BF_MAX_FRAME];
	for (int i = 0; i < n; i++) sum[i] = 0;
	for (int c = 0; c < BF_CH; c++) {
		int16_t *x = b->hist[c] + (BF_TAPS - 1);          /* x[i] is this frame's sample i; x[-1..-15] the last frame's tail */
		for (int i = 0; i < n; i++) x[i] = mic[i * BF_CH + c];
		const int16_t *h = b->taps[c];
		for (int i = 0; i < n; i++) {
			int32_t acc = 0;                               /* |sum h| < 1.5 in Q15, so 16 taps of 16-bit samples fit in 32 bits */
			for (int j = 0; j < BF_TAPS; j++) acc += (int32_t)h[j] * x[i - j];
			sum[i] += (acc + (1 << 14)) >> 15;
		}
		memmove(b->hist[c], b->hist[c] + n, (BF_TAPS - 1) * sizeof(int16_t));
	}
	for (int i = 0; i < n; i++) out[i] = sat16((sum[i] + 2) >> 2);
}
