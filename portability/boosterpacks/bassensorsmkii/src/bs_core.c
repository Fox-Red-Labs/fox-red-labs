/* Power switches, register helpers and the address scan. */
#include "bassens.h"

void bs_power(bs_pin_t pin, int on)
{
    /* HDC2080 and DRV5055 sit behind P-channel switches (low = on); OPT3001 and TMP117 are powered straight from the pin (high = on). */
    int low_is_on = (pin == BS_PIN_HDC_PWR || pin == BS_PIN_DRV_PWR);
    bs_port_pin_write(pin, low_is_on ? !on : !!on);
}

void bs_power_all(int on)
{
    bs_power(BS_PIN_HDC_PWR, on);
    bs_power(BS_PIN_DRV_PWR, on);
    bs_power(BS_PIN_OPT_PWR, on);
    bs_power(BS_PIN_TMP_PWR, on);
    if (on) bs_port_delay_ms(50);
}

int bs_reg_read(uint8_t addr, uint8_t reg, uint8_t *d, uint8_t n)
{
    if (bs_port_i2c_write(addr, &reg, 1)) return -1;
    return bs_port_i2c_read(addr, d, n) ? -1 : 0;
}

int bs_reg_write(uint8_t addr, uint8_t reg, uint8_t v)
{
    uint8_t b[2] = { reg, v };
    return bs_port_i2c_write(addr, b, 2) ? -1 : 0;
}

int bs_reg_read16be(uint8_t addr, uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    if (bs_reg_read(addr, reg, b, 2)) return -1;
    *v = (uint16_t)((b[0] << 8) | b[1]);
    return 0;
}

int bs_reg_write16be(uint8_t addr, uint8_t reg, uint16_t v)
{
    uint8_t b[3] = { reg, (uint8_t)(v >> 8), (uint8_t)v };
    return bs_port_i2c_write(addr, b, 3) ? -1 : 0;
}

int bs_scan(uint8_t *found, int max)
{
    int n = 0;
    for (uint8_t a = 0x08; a < 0x78; a++)
        if (bs_port_i2c_write(a, 0, 0) == 0) { if (n < max) found[n] = a; n++; }
    return n;
}
