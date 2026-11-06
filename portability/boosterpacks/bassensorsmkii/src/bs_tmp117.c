/* TMP117 (SNOSD82): ID register 0x0F, temperature register 0x00, 7.8125 mC per step, continuous conversion after power-up. */
#include "bassens.h"

int bs_tmp117_probe(void)
{
    uint16_t id;
    if (bs_reg_read16be(BS_ADDR_TMP117, 0x0F, &id)) return -1;
    return (id & 0x0FFF) == 0x0117 ? 0 : -2;
}

int bs_tmp117_read_mc(int32_t *milli_c)
{
    uint16_t raw;
    if (bs_reg_read16be(BS_ADDR_TMP117, 0x00, &raw)) return -1;
    if (raw == 0x8000) return -3;                 /* the sensor's "no data yet" value */
    *milli_c = (int32_t)(int16_t)raw * 625 / 80;  /* 7.8125 = 625 / 80 */
    return 0;
}
