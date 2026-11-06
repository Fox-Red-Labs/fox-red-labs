/* Minimal BMI160 reader: see bmi160.h for the register sources. */
#include "bmi160.h"

static int wr(const bmi160_bus *b, uint8_t reg, uint8_t val)
{
    uint8_t d[2] = { reg, val };
    return b->write(b->ctx, b->addr, d, 2) ? BMI160_ERR_BUS : BMI160_OK;
}

int bmi160_init(const bmi160_bus *b, int with_gyro, uint8_t *id_out)
{
    uint8_t id = 0;
    if (b->read(b->ctx, b->addr, 0x00, &id, 1)) return BMI160_ERR_BUS;
    if (id_out) *id_out = id;
    if (id != BMI160_CHIP_ID) return BMI160_ERR_ID;
    if (wr(b, 0x7E, 0x11)) return BMI160_ERR_BUS;      /* accel: suspend to normal */
    b->delay_ms(b->ctx, 4);                            /* data sheet max 3.8 ms */
    if (with_gyro) {
        if (wr(b, 0x7E, 0x15)) return BMI160_ERR_BUS;  /* gyro: suspend to normal */
        b->delay_ms(b->ctx, 81);                       /* data sheet max 80 ms */
    }
    if (wr(b, 0x40, 0x28)) return BMI160_ERR_BUS;      /* ACC_CONF: 100 Hz, normal filter */
    if (wr(b, 0x41, 0x03)) return BMI160_ERR_BUS;      /* ACC_RANGE: +-2 g */
    if (with_gyro) {
        if (wr(b, 0x42, 0x28)) return BMI160_ERR_BUS;  /* GYR_CONF: 100 Hz */
        if (wr(b, 0x43, 0x00)) return BMI160_ERR_BUS;  /* GYR_RANGE: +-2000 deg/s */
    }
    return BMI160_OK;
}

int bmi160_read(const bmi160_bus *b, int16_t out[6])
{
    uint8_t d[12];
    if (b->read(b->ctx, b->addr, 0x0C, d, sizeof d)) return BMI160_ERR_BUS;
    for (int i = 0; i < 3; i++) {
        out[3 + i] = (int16_t)(uint16_t)(d[2 * i] | (d[2 * i + 1] << 8));        /* gyro at 0x0C */
        out[i]     = (int16_t)(uint16_t)(d[6 + 2 * i] | (d[6 + 2 * i + 1] << 8)); /* accel at 0x12 */
    }
    return BMI160_OK;
}
