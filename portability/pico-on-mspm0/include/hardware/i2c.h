/* Pico SDK on MSPM0: hardware/i2c.h. i2c0 is the board's I2C controller (on the LP-MSPM0G3519, I2C2 on PC3 SDA / PC2 SCL).
 * Blocking calls only, polled; return the byte count or PICO_ERROR_GENERIC when the address or a byte is not acknowledged. */
#ifndef PICO_MSPM0_I2C_H
#define PICO_MSPM0_I2C_H
#include "pico/types.h"
#include "pico/error.h"
#include "pico_board.h"
typedef struct i2c_inst i2c_inst_t;
extern i2c_inst_t *const i2c0;
#define i2c_default i2c0
uint i2c_init(i2c_inst_t *i2c, uint baudrate);
int i2c_read_blocking(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop);
#endif
