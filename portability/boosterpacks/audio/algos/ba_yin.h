/* Pitch detection by the YIN method (de Cheveigne and Kawahara, 2002) in integer arithmetic.
 * One frame is BA_YIN_W + BA_YIN_TAUMAX samples (DC removed, signed 16-bit). At 8 kHz that is 384 samples, 48 ms,
 * and the lowest period it can see is 127 samples (63 Hz). Work: W * TAUMAX multiply-accumulates (about 32,000). */
#ifndef BA_YIN_H
#define BA_YIN_H
#include <stdint.h>

#define BA_YIN_W 256
#define BA_YIN_TAUMAX 128
#define BA_YIN_FRAME (BA_YIN_W + BA_YIN_TAUMAX)

typedef struct {
    uint32_t freq_x256;  /* frequency in 1/256 Hz; 0 when no pitch was found */
    uint32_t tau_q8;     /* period in samples, Q8 */
    uint16_t cmnd_q15;   /* the normalised difference at the period; low is a clean pitch (0.15 = 4915 is the usual limit) */
    uint8_t voiced;      /* 1 when the first dip fell below the threshold */
} ba_yin_result_t;

/* x: BA_YIN_FRAME samples, mean already removed. thr_q15: dip threshold (4915 is 0.15). */
void ba_yin(const int16_t *x, uint32_t rate_hz, uint16_t thr_q15, ba_yin_result_t *r);

/* Nearest equal-tempered note (MIDI number, A4 = 69 = 440 Hz) and the error in tenths of a cent (+/-500). Returns 0 when freq is 0. */
int ba_pitch_note(uint32_t freq_x256, int *midi, int *cents_x10);

#endif
