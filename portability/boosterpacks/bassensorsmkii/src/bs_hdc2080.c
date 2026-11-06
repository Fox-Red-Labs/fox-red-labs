/* HDC2080 (SNAS678): ID at 0xFC (manufacturer) and 0xFE (device), little endian; data 0x00 to 0x03; measurement trigger 0x0F bit 0. */
#include "bassens.h"

int bs_hdc2080_probe(void)
{
    uint8_t b[2];
    if (bs_reg_read(BS_ADDR_HDC2080, 0xFC, b, 2)) return -1;
    if ((uint16_t)(b[0] | b[1] << 8) != 0x5449) return -2;
    if (bs_reg_read(BS_ADDR_HDC2080, 0xFE, b, 2)) return -1;
    return (uint16_t)(b[0] | b[1] << 8) == 0x07D0 ? 0 : -2;
}

int bs_hdc2080_read(int32_t *centi_c, int32_t *tenth_pct)
{
    uint8_t b[4], st = 0;
    if (bs_reg_write(BS_ADDR_HDC2080, 0x0F, 0x01)) return -1;   /* 14-bit temperature and humidity, one shot */
    for (int i = 0; i < 40; i++) {                                /* conversion takes about 2 ms */
        bs_port_delay_ms(1);
        if (bs_reg_read(BS_ADDR_HDC2080, 0x04, &st, 1)) return -1;
        if (st & 0x80) break;                                     /* DRDY_STATUS */
    }
    if (!(st & 0x80)) return -2;
    if (bs_reg_read(BS_ADDR_HDC2080, 0x00, b, 4)) return -1;
    uint32_t t = (uint32_t)(b[1] << 8 | b[0]), h = (uint32_t)(b[3] << 8 | b[2]);
    *centi_c = (int32_t)(t * 16500u / 65536u) - 4000;
    *tenth_pct = (int32_t)(h * 1000u / 65536u);
    return 0;
}
