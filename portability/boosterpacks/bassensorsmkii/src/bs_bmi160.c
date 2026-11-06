/* BMI160 inertial sensor (Bosch BST-BMI160-DS000) and the BMM150 compass reached through its secondary interface. */
#include "bassens.h"

enum { R_CHIP_ID = 0x00, R_PMU = 0x03, R_DATA_MAG = 0x04, R_DATA_GYR = 0x0C, R_TEMP = 0x20,
       R_ACC_CONF = 0x40, R_ACC_RANGE = 0x41, R_GYR_CONF = 0x42, R_GYR_RANGE = 0x43, R_MAG_CONF = 0x44,
       R_MAG_IF0 = 0x4B, R_MAG_IF1 = 0x4C, R_MAG_IF2 = 0x4D, R_MAG_IF3 = 0x4E, R_MAG_IF4 = 0x4F,
       R_IF_CONF = 0x6B, R_CMD = 0x7E };

static int odr_code(uint16_t hz)
{
    switch (hz) { case 25: return 6; case 50: return 7; case 100: return 8; case 200: return 9; case 400: return 10; default: return -1; }
}

int bs_bmi160_init(bs_bmi160_t *d, uint8_t acc_g, uint16_t gyr_dps, uint16_t odr_hz)
{
    uint8_t id = 0, ar, gr;
    int oc = odr_code(odr_hz);
    if (oc < 0) return -4;
    switch (acc_g) { case 2: ar = 0x03; break; case 4: ar = 0x05; break; case 8: ar = 0x08; break; case 16: ar = 0x0C; break; default: return -4; }
    switch (gyr_dps) { case 2000: gr = 0; break; case 1000: gr = 1; break; case 500: gr = 2; break; case 250: gr = 3; break; case 125: gr = 4; break; default: return -4; }
    d->addr = 0;
    for (uint8_t a = 0x69; a >= 0x68; a--)
        if (bs_reg_read(a, R_CHIP_ID, &id, 1) == 0 && id == 0xD1) { d->addr = a; break; }
    if (!d->addr) return -1;
    d->acc_range = acc_g; d->gyr_range = gyr_dps;
    if (bs_reg_write(d->addr, R_CMD, 0xB6)) return -1;          /* soft reset */
    bs_port_delay_ms(30);
    if (bs_reg_write(d->addr, R_CMD, 0x11)) return -1;          /* accelerometer normal mode, 3.8 ms */
    bs_port_delay_ms(10);
    if (bs_reg_write(d->addr, R_CMD, 0x15)) return -1;          /* gyroscope normal mode, 80 ms */
    bs_port_delay_ms(90);
    uint8_t pmu = 0;
    if (bs_reg_read(d->addr, R_PMU, &pmu, 1)) return -1;
    if ((pmu & 0x3C) != 0x14) return -2;                        /* accelerometer and gyroscope both in normal mode */
    if (bs_reg_write(d->addr, R_ACC_CONF, (uint8_t)(0x20 | oc))) return -1;   /* normal bandwidth filter */
    if (bs_reg_write(d->addr, R_ACC_RANGE, ar)) return -1;
    if (bs_reg_write(d->addr, R_GYR_CONF, (uint8_t)(0x20 | oc))) return -1;
    if (bs_reg_write(d->addr, R_GYR_RANGE, gr)) return -1;
    return 0;
}

int bs_bmi160_read(const bs_bmi160_t *d, bs_imu_raw_t *r)
{
    uint8_t b[15];
    if (bs_reg_read(d->addr, R_DATA_GYR, b, 15)) return -1;     /* gyro x y z, accel x y z, sensor time (0x0C to 0x1A) */
    r->gx = (int16_t)(b[1] << 8 | b[0]);  r->gy = (int16_t)(b[3] << 8 | b[2]);   r->gz = (int16_t)(b[5] << 8 | b[4]);
    r->ax = (int16_t)(b[7] << 8 | b[6]);  r->ay = (int16_t)(b[9] << 8 | b[8]);   r->az = (int16_t)(b[11] << 8 | b[10]);
    r->t = (uint32_t)b[12] | (uint32_t)b[13] << 8 | (uint32_t)b[14] << 16;
    return 0;
}

int32_t bs_bmi160_mg(const bs_bmi160_t *d, int16_t raw)
{
    int32_t per_g = 32768 / d->acc_range;                       /* 16384 counts per g at 2 g */
    return (int32_t)raw * 1000 / per_g;
}

int32_t bs_bmi160_mdps(const bs_bmi160_t *d, int16_t raw)
{
    int32_t lsb_x10 = (int32_t)(328000 / d->gyr_range);         /* 16.4 counts per deg/s at 2000 -> 164, 2624 at 125 */
    return (int32_t)raw * 10000 / lsb_x10;
}

int bs_bmi160_temp_cc(const bs_bmi160_t *d, int32_t *centi_c)
{
    uint8_t b[2];
    if (bs_reg_read(d->addr, R_TEMP, b, 2)) return -1;
    *centi_c = 2300 + (int32_t)(int16_t)(b[1] << 8 | b[0]) * 100 / 512;   /* 0x0000 = 23 C, 1/512 K per count */
    return 0;
}

/* ---- BMM150 behind the BMI160 ---- */
static int mag_write(const bs_bmi160_t *d, uint8_t reg, uint8_t v)
{
    if (bs_reg_write(d->addr, R_MAG_IF4, v) || bs_reg_write(d->addr, R_MAG_IF3, reg)) return -1;
    bs_port_delay_ms(5);
    return 0;
}

int bs_bmm150_init(const bs_bmi160_t *d)
{
    uint8_t id = 0;
    if (bs_reg_write(d->addr, R_CMD, 0x19)) return -1;          /* secondary interface normal mode */
    bs_port_delay_ms(2);
    if (bs_reg_write(d->addr, R_IF_CONF, 0x20)) return -1;      /* secondary interface = magnetometer */
    if (bs_reg_write(d->addr, R_MAG_IF0, BS_ADDR_BMM150 << 1)) return -1;
    if (bs_reg_write(d->addr, R_MAG_IF1, 0x80)) return -1;      /* setup mode (manual) */
    if (mag_write(d, 0x4B, 0x01)) return -1;                    /* BMM150 power control: out of suspend */
    bs_port_delay_ms(5);
    if (bs_reg_write(d->addr, R_MAG_IF2, 0x40)) return -1;      /* read the BMM150 chip ID (0x32) into DATA_0 */
    bs_port_delay_ms(5);
    if (bs_reg_read(d->addr, R_DATA_MAG, &id, 1)) return -1;
    if (id != 0x32) return -2;
    if (mag_write(d, 0x51, 0x04) || mag_write(d, 0x52, 0x0E)) return -1;   /* regular preset: 9 repetitions XY, 15 Z */
    if (bs_reg_write(d->addr, R_MAG_CONF, 0x06)) return -1;     /* 25 Hz */
    if (bs_reg_write(d->addr, R_MAG_IF2, 0x42)) return -1;      /* data starts at BMM150 register 0x42 */
    if (bs_reg_write(d->addr, R_MAG_IF3, 0x4C) || bs_reg_write(d->addr, R_MAG_IF4, 0x02)) return -1;   /* forced measurement each cycle */
    if (bs_reg_write(d->addr, R_MAG_IF1, 0x02)) return -1;      /* data mode, 8 byte burst */
    bs_port_delay_ms(50);
    return 0;
}

int bs_bmm150_read_raw(const bs_bmi160_t *d, int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t b[6];
    if (bs_reg_read(d->addr, R_DATA_MAG, b, 6)) return -1;
    *x = (int16_t)(b[1] << 8 | b[0]) >> 3;                      /* 13 bit */
    *y = (int16_t)(b[3] << 8 | b[2]) >> 3;
    *z = (int16_t)(b[5] << 8 | b[4]) >> 1;                      /* 15 bit */
    return 0;
}
