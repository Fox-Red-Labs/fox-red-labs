/* Voice front-end bench harness: beamformer, SpeexDSP echo canceller and noise suppressor, 10 ms frames at 16 kHz,
 * fed from a test signal in flash (fe_data.c, made by gen_signal.py). Portable C; the platform supplies two hooks. */
#ifndef VOICEFE_RUN_H
#define VOICEFE_RUN_H
#include <stdint.h>
#define FE_RATE 16000
#define FE_FRAME 160                 /* 10 ms */
#ifndef FE_TAIL
#define FE_TAIL 3200                 /* 200 ms echo tail */
#endif
#ifndef FE_LOOPS
#define FE_LOOPS 8                   /* passes over the clip; 8 x 4 s = 32 s of audio */
#endif
#define FE_HBINS 200                 /* histogram of frame time, 0.1 ms bins up to 20 ms */
uint32_t plat_ticks(void);           /* free-running counter, tick_hz per second; differences are taken mod 2^32 */
void plat_log(const char *fmt, ...); /* one report line, already ending in \r\n */
/* Optional: called after every frame with the echo canceller's output and the final output (host reference run). */
typedef void (*fe_sink_fn)(const int16_t *aec, const int16_t *out, int n);
int fe_main(uint32_t tick_hz, fe_sink_fn sink);
extern volatile unsigned fe_arena_used, fe_arena_fail, fe_speex_msgs, fe_speex_fatal;
extern const unsigned fe_arena_size;
extern volatile uint32_t fe_done;    /* 0x46454430 when finished */
#endif
