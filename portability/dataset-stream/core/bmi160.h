/* Minimal Bosch BMI160 reader for dataset-stream: accelerometer and gyroscope over I2C.
 *
 * Board-independent: the board passes three small functions (I2C write, I2C register read,
 * millisecond delay). Register facts are from Bosch data sheet BST-BMI160-DS000-09
 * (revision 1.0, November 2020), sections 2.11.1 to 2.11.14 and 2.11.38:
 *   0x00 CHIPID reads 0xD1
 *   0x0C..0x11 gyro x, y, z; 0x12..0x17 accel x, y, z; 16-bit, low byte first
 *   0x40 ACC_CONF 0x28 = 100 Hz output, normal filter (also the reset value)
 *   0x41 ACC_RANGE 0x03 = +-2 g, 16384 counts per g (reset value)
 *   0x42 GYR_CONF 0x28 = 100 Hz; 0x43 GYR_RANGE 0x00 = +-2000 deg/s, 16.4 counts per deg/s
 *   0x7E CMD: 0x11 accel to normal mode (max 3.8 ms), 0x15 gyro to normal mode (max 80 ms)
 * Both sensors are in suspend mode after power-up (PMU_STATUS 0x03 resets to 0), so nothing
 * is measured until the two commands are written.
 *
 * On BP-BASSENSORSMKII the BMI160 answers at I2C address 0x69 (TI user guide SLAU829).
 */
#ifndef DS_BMI160_H_
#define DS_BMI160_H_

#include <stddef.h>
#include <stdint.h>

#define BMI160_ADDR_BASSENSORS 0x69u
#define BMI160_CHIP_ID 0xD1u

typedef struct {
    /* Write n bytes (first byte is the register) to the 7-bit address. Return 0 on success. */
    int (*write)(void *ctx, uint8_t addr, const uint8_t *data, size_t n);
    /* Read n bytes starting at register reg. Return 0 on success. */
    int (*read)(void *ctx, uint8_t addr, uint8_t reg, uint8_t *data, size_t n);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
    uint8_t addr;
} bmi160_bus;

enum { BMI160_OK = 0, BMI160_ERR_BUS = -1, BMI160_ERR_ID = -2 };

/* Check the chip ID, wake the accelerometer (and the gyroscope if with_gyro), set 100 Hz and
 * the ranges above. If id_out is not NULL it gets the ID byte read. */
int bmi160_init(const bmi160_bus *b, int with_gyro, uint8_t *id_out);

/* One reading: out[0..2] accel x, y, z, out[3..5] gyro x, y, z, raw counts. */
int bmi160_read(const bmi160_bus *b, int16_t out[6]);

#endif
