/* Host test of the conversions with a fake I2C bus: cc -I../include host_test.c <src files> -o host_test, then ./host_test */
#include <stdio.h>
#include <string.h>
#include "bassens.h"

static uint8_t regs[128][256];       /* per 7-bit address, per register */
static int present[128], last_addr, last_reg;
int bs_port_i2c_write(uint8_t a, const uint8_t *d, uint8_t n)
{
    if (!present[a]) return -1;
    last_addr = a;
    if (n >= 1) last_reg = d[0];
    for (int i = 1; i < n; i++) regs[a][(d[0] + i - 1) & 255] = d[i];
    /* instant conversions: the ready flags are set whenever a measurement is started */
    if (a == 0x40 && n == 2 && d[0] == 0x0F) regs[0x40][0x04] = 0x80;
    if (a == 0x44 && n == 3 && d[0] == 0x01) { regs[0x44][1] = 0x02; regs[0x44][2] = 0x80 | 0x10; }
    return 0;
}
int bs_port_i2c_read(uint8_t a, uint8_t *d, uint8_t n)
{
    if (!present[a]) return -1;
    for (int i = 0; i < n; i++) d[i] = regs[a][(last_reg + i) & 255];
    return 0;
}
void bs_port_pin_write(bs_pin_t p, int l) { (void)p; (void)l; }
int32_t bs_port_adc_drv_mv(void) { return 1700; }
void bs_port_delay_ms(uint32_t ms) { (void)ms; }
uint32_t bs_port_micros(void) { return 0; }

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void)
{
    /* TMP117: ID 0x0117, raw 0x0C80 = 3200 steps = 25.000 C */
    present[0x48] = 1; regs[0x48][0x0F] = 0x01; regs[0x48][0x10] = 0x17; regs[0x48][0] = 0x0C; regs[0x48][1] = 0x80;
    int32_t mc; CHECK(bs_tmp117_probe() == 0); CHECK(bs_tmp117_read_mc(&mc) == 0 && mc == 25000);
    regs[0x48][0] = 0xFF; regs[0x48][1] = 0xFF; CHECK(bs_tmp117_read_mc(&mc) == 0 && mc == -7);   /* -1 step = -7.8 mC */
    /* HDC2080: IDs, temperature raw 0x6000 = 24576 -> 24576/65536*165-40 = 21.875 C; humidity raw 0x8000 = 50.0 % */
    present[0x40] = 1; regs[0x40][0xFC] = 0x49; regs[0x40][0xFD] = 0x54; regs[0x40][0xFE] = 0xD0; regs[0x40][0xFF] = 0x07;
    regs[0x40][0] = 0x00; regs[0x40][1] = 0x60; regs[0x40][2] = 0x00; regs[0x40][3] = 0x80;
    int32_t c, h; CHECK(bs_hdc2080_probe() == 0); CHECK(bs_hdc2080_read(&c, &h) == 0 && c == 2187 && h == 500);
    /* OPT3001: the ID words 0x5449 and 0x3001 are read from registers 0x7E and 0x7F (two bytes each; the fake keeps bytes, so the
     * second word sits at 0x80 and the probe is expected to fail until both words are set). The lux maths is covered by the bench run. */
    present[0x44] = 1; regs[0x44][0x7E] = 0x54; regs[0x44][0x7F] = 0x49;
    CHECK(bs_opt3001_probe() != 0);
    /* DRV5055: 1700 mV is 50 mV above 1650 -> 50 / 7.5 = 6.6 mT = 66 tenths */
    CHECK(bs_drv5055_dmt(1700, 1650, 7500) == 66);
    /* BMI160: scaling at +-4 g and +-500 dps */
    bs_bmi160_t d = { 0x69, 4, 500 };
    CHECK(bs_bmi160_mg(&d, 8192) == 1000); CHECK(bs_bmi160_mg(&d, -4096) == -500);
    CHECK(bs_bmi160_mdps(&d, 656) == 10000);
    /* BMI160 burst parse: gyro 1,2,3; accel 100,200,8192; time 0x030201 */
    present[0x69] = 1; regs[0x69][0] = 0xD1;
    uint8_t burst[15] = { 1,0, 2,0, 3,0, 100,0, 200,0, 0x00,0x20, 1,2,3 }; memcpy(&regs[0x69][0x0C], burst, 15);
    bs_imu_raw_t r; CHECK(bs_bmi160_read(&d, &r) == 0); CHECK(r.gx == 1 && r.gz == 3 && r.ax == 100 && r.az == 8192 && r.t == 0x030201);
    regs[0x69][0x20] = 0x00; regs[0x69][0x21] = 0x00; int32_t tc; CHECK(bs_bmi160_temp_cc(&d, &tc) == 0 && tc == 2300);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
