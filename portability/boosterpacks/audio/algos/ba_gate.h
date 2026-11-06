/* Noise gate and a simple echo, 16-bit samples, integer only.
 * Gate: peak envelope follower (fast attack, slow release), open above thr_open, close below thr_close after a hold time,
 * gain ramps in Q15 so it never clicks. Echo: one delay line with feedback and a wet mix; the line lives in a buffer the caller supplies. */
#ifndef BA_GATE_H
#define BA_GATE_H
#include <stdint.h>

typedef struct {
    int32_t env;            /* envelope, same units as the samples */
    int32_t gain;           /* Q15, 0 closed .. 32768 open */
    int32_t thr_open, thr_close;
    uint16_t hold, hold_cnt;
    int32_t up_step, down_step; /* gain change per sample */
    uint8_t is_open, atk_shift, rel_shift;
} ba_gate_t;

/* thr_open/thr_close in sample units (e.g. 400 and 250 of 32767); hold in samples; attack_ms/release_ms set the gain ramps. */
void ba_gate_init(ba_gate_t *g, int32_t thr_open, int32_t thr_close, uint16_t hold, unsigned attack_ms, unsigned release_ms, unsigned rate_hz);
int16_t ba_gate_process(ba_gate_t *g, int16_t x);

typedef struct { int16_t *buf; uint16_t len, pos; int16_t fb_q15, wet_q15; } ba_echo_t;
/* buf must hold delay_samples entries. fb_q15: feedback gain (16384 = 0.5), wet_q15: echo level in the output. */
void ba_echo_init(ba_echo_t *e, int16_t *buf, uint16_t delay_samples, int16_t fb_q15, int16_t wet_q15);
int16_t ba_echo_process(ba_echo_t *e, int16_t x);

#endif
