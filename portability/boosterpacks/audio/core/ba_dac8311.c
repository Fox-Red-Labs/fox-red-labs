#include "ba_dac8311.h"

static void shift_bits(const ba_hal_t *h, uint16_t word, int nbits)
{
    /* Idle SCLK low. Data sits on DIN while SCLK is high; the DAC takes it on the falling edge. */
    for (int i = nbits - 1; i >= 0; i--) {
        h->sclk(1);
        h->din((word >> i) & 1u);
        h->sclk(0);
    }
}

void ba_dac_write16(const ba_hal_t *h, uint16_t word)
{
    h->sync(0);
    shift_bits(h, word, 16);
    h->sync(1);
}

void ba_dac_write_split8(const ba_hal_t *h, uint16_t word)
{
    h->sync(0);
    shift_bits(h, (uint16_t)(word >> 8), 8);
    h->sync(1); /* the mistake: chip select rises after the first byte */
    h->sync(0);
    shift_bits(h, (uint16_t)(word & 0xFFu), 8);
    h->sync(1);
}

uint16_t ba_dac_code_from_s16(int16_t s, unsigned vol16)
{
    int32_t v = ((int32_t)s * (int32_t)vol16) / 16; /* +/-32767 at vol 16 */
    int32_t code = (int32_t)BA_DAC_MIDSCALE + (v / 4); /* 14 bits: +/-8191 around mid-scale */
    if (code < 0) code = 0;
    if (code > (int32_t)BA_DAC_MAX) code = (int32_t)BA_DAC_MAX;
    return (uint16_t)code;
}
