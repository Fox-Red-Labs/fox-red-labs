/* DAC8311: 14-bit DAC behind a 16-bit frame. One frame is SYNC low for exactly 16 SCLK falling edges, data
 * most significant bit first: bit 15 PD1, bit 14 PD0, bits 13 to 0 the code. The output updates after the 16th
 * falling edge. SYNC rising before the 16th edge cancels the frame (data sheet "SYNC interrupt"), so a host
 * that raises chip select between two 8-bit transfers makes the DAC ignore every sample. */
#ifndef BA_DAC8311_H
#define BA_DAC8311_H
#include <stdint.h>
#include "ba_hal.h"

#define BA_DAC_PD_NORMAL 0u
#define BA_DAC_MIDSCALE 8192u
#define BA_DAC_MAX 16383u

static inline uint16_t ba_dac_word(uint16_t code14, uint8_t pd) { return (uint16_t)(((pd & 3u) << 14) | (code14 & 0x3FFFu)); }

/* One correct frame: SYNC low across all 16 clocks. */
void ba_dac_write16(const ba_hal_t *h, uint16_t word);
/* The faulty frame (experiment E237): two 8-bit transfers, SYNC raised between them. The DAC never updates. */
void ba_dac_write_split8(const ba_hal_t *h, uint16_t word);
/* A signed 16-bit sample (full scale +/-32767, scaled by vol 0 to 16) as a mid-scale-centered 14-bit code. */
uint16_t ba_dac_code_from_s16(int16_t s, unsigned vol16);

#endif
