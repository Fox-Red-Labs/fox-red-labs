/* BP-BASSENSORSMKII drivers: TMP117, HDC2080, OPT3001, DRV5055, BMI160 and the BMM150 behind it.
 * Written from the data sheets and SLAU829, in integer maths (no FPU needed), over bs_port.h.
 * Every call returns 0 on success and a negative number on failure; no call blocks forever. */
#ifndef BASSENS_H
#define BASSENS_H
#include <stdint.h>
#include "bs_port.h"

#define BS_ADDR_HDC2080 0x40
#define BS_ADDR_OPT3001 0x44
#define BS_ADDR_TMP117  0x48
#define BS_ADDR_BMI160  0x69   /* SDO tied to V+ on the pack; 0x68 is tried as well */
#define BS_ADDR_BMM150  0x13   /* on the BMI160's secondary bus, never on the host bus */

/* Power: bs_power_all(1) switches on HDC2080, OPT3001, TMP117 and DRV5055, waits 50 ms. */
void bs_power_all(int on);
void bs_power(bs_pin_t pin, int on);

/* Register helpers (big endian for the TI parts, little endian for Bosch, as each data sheet has it). */
int bs_reg_read(uint8_t addr, uint8_t reg, uint8_t *d, uint8_t n);
int bs_reg_write(uint8_t addr, uint8_t reg, uint8_t v);
int bs_reg_read16be(uint8_t addr, uint8_t reg, uint16_t *v);
int bs_reg_write16be(uint8_t addr, uint8_t reg, uint16_t v);

/* Address scan 0x08 to 0x77: fills found[] (up to max) and returns how many answered. */
int bs_scan(uint8_t *found, int max);

/* TMP117: temperature in milli-degrees C (7.8125 mC per step). */
int bs_tmp117_probe(void);                 /* device ID register 0x0F, low 12 bits = 0x117 */
int bs_tmp117_read_mc(int32_t *milli_c);

/* HDC2080: one triggered measurement, temperature in centi-degrees C, humidity in tenths of a percent RH. */
int bs_hdc2080_probe(void);                /* manufacturer 0x5449, device 0x07D0 */
int bs_hdc2080_read(int32_t *centi_c, int32_t *tenth_pct);

/* OPT3001: one single-shot conversion (100 ms), result in hundredths of a lux. */
int bs_opt3001_probe(void);                /* manufacturer 0x5449, device 0x3001 */
int bs_opt3001_read_clux(uint32_t *centi_lux);

/* DRV5055: output voltage and field. Quiescent output is half the supply; sensitivity is per variant. */
int bs_drv5055_read_mv(int32_t *mv);
/* Field in tenths of a milli-tesla from a voltage: (mv - rest_mv) * 10 / sens_uv_per_mt * 1000. DRV5055A4 at 3.3 V: rest 1650, 7500 uV/mT. */
int32_t bs_drv5055_dmt(int32_t mv, int32_t rest_mv, int32_t sens_uv_per_mt);

/* BMI160 accelerometer and gyroscope. */
typedef struct {
    uint8_t addr;           /* 0x69 or 0x68 once found */
    uint8_t acc_range;      /* 2, 4, 8 or 16 (g) */
    uint16_t gyr_range;     /* 125, 250, 500, 1000 or 2000 (deg/s) */
} bs_bmi160_t;
typedef struct {
    int16_t ax, ay, az;     /* raw counts */
    int16_t gx, gy, gz;     /* raw counts */
    uint32_t t;             /* sensor time, 39.0625 us per count, 24 bit */
} bs_imu_raw_t;
/* Reset, find it at 0x69 or 0x68, check chip ID 0xD1, start both sensors at odr_hz (25, 50, 100, 200 or 400). */
int bs_bmi160_init(bs_bmi160_t *d, uint8_t acc_range_g, uint16_t gyr_range_dps, uint16_t odr_hz);
int bs_bmi160_read(const bs_bmi160_t *d, bs_imu_raw_t *r);          /* one 15-byte burst */
int32_t bs_bmi160_mg(const bs_bmi160_t *d, int16_t raw);            /* accelerometer counts to milli-g */
int32_t bs_bmi160_mdps(const bs_bmi160_t *d, int16_t raw);          /* gyroscope counts to milli-degrees per second */
int bs_bmi160_temp_cc(const bs_bmi160_t *d, int32_t *centi_c);      /* die temperature, about 23 C at zero */

/* BMM150 compass through the BMI160's secondary interface. Raw, uncompensated counts (1/16 uT per count nominal);
 * the factory trim correction is not applied. */
int bs_bmm150_init(const bs_bmi160_t *d);
int bs_bmm150_read_raw(const bs_bmi160_t *d, int16_t *x, int16_t *y, int16_t *z);
#endif
