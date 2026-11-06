/* Four-mic delay-and-sum beamformer, fixed point. Each channel goes through a short fractional-delay FIR (Q15 taps)
 * that lines it up with the look direction, then the four are averaged. Portable C, no platform calls. */
#ifndef VOICEFE_BEAMFORMER_H
#define VOICEFE_BEAMFORMER_H
#include <stdint.h>
#define BF_CH 4
#define BF_TAPS 16
#define BF_MAX_FRAME 160
typedef struct {
	int16_t hist[BF_CH][BF_TAPS - 1 + BF_MAX_FRAME];   /* per channel: last BF_TAPS-1 samples, then this frame */
	int16_t taps[BF_CH][BF_TAPS];                         /* Q15, reversed order not needed: y[n] = sum h[j] x[n-j] */
} bf_t;
void bf_init(bf_t *b, const int16_t taps[BF_CH][BF_TAPS]);
/* mic: n frames of BF_CH interleaved samples; out: n samples. n <= BF_MAX_FRAME. */
void bf_process(bf_t *b, const int16_t *mic, int16_t *out, int n);
#endif
