#include "ba_ws2812.h"

void ba_ws2812_encode(const uint8_t (*rgb)[3], unsigned n, uint8_t *out)
{
    unsigned o = 0;
    for (unsigned i = 0; i < n; i++) {
        const uint8_t grb[3] = { rgb[i][1], rgb[i][0], rgb[i][2] };
        for (unsigned c = 0; c < 3; c++) {
            uint32_t bits = 0;  /* 24 SPI bits for 8 LED bits */
            for (int b = 7; b >= 0; b--) bits = (bits << 3) | (((grb[c] >> b) & 1u) ? 6u : 4u);
            out[o++] = (uint8_t)(bits >> 16);
            out[o++] = (uint8_t)(bits >> 8);
            out[o++] = (uint8_t)bits;
        }
    }
    for (unsigned t = 0; t < BA_WS_TAIL; t++) out[o++] = 0;
}
