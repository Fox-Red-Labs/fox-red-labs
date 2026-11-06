/* OPT3001 (SBOS681): config 0x01, result 0x00, IDs 0x7E and 0x7F. Result = mantissa (12 bit) << exponent (4 bit) x 0.01 lux. */
#include "bassens.h"

int bs_opt3001_probe(void)
{
    uint16_t m, d;
    if (bs_reg_read16be(BS_ADDR_OPT3001, 0x7E, &m) || bs_reg_read16be(BS_ADDR_OPT3001, 0x7F, &d)) return -1;
    return (m == 0x5449 && d == 0x3001) ? 0 : -2;
}

int bs_opt3001_read_clux(uint32_t *centi_lux)
{
    uint16_t c = 0, r;
    /* automatic range (0xC), 100 ms conversion (CT = 0), single shot (M = 01) */
    if (bs_reg_write16be(BS_ADDR_OPT3001, 0x01, 0xC200)) return -1;
    for (int i = 0; i < 40; i++) {
        bs_port_delay_ms(5);
        if (bs_reg_read16be(BS_ADDR_OPT3001, 0x01, &c)) return -1;
        if (c & 0x0080) break;                                    /* conversion ready */
    }
    if (!(c & 0x0080)) return -2;
    if (bs_reg_read16be(BS_ADDR_OPT3001, 0x00, &r)) return -1;
    *centi_lux = (uint32_t)(r & 0x0FFF) << (r >> 12);
    return 0;
}
