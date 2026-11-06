/* Feature frames for the alarm listener; see features.c and host/features.py. */
#ifndef FEATURES_H_
#define FEATURES_H_
#include <stdint.h>
#define FE_NCH 13   /* 9 band dB, total dB, peak kHz/4, crest dB/10, peak dB */
void fe_init(void);
/* One frame from 1,024 samples at ring[(start + i) % len]. */
void fe_frame(const int32_t *ring, uint32_t len, uint32_t start, float o[FE_NCH]);
#endif
