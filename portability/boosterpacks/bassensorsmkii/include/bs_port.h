/* Port interface of the BP-BASSENSORSMKII drivers: what a board has to provide.
 * Plain C, no vendor headers. Everything is blocking and single threaded. */
#ifndef BS_PORT_H
#define BS_PORT_H
#include <stdint.h>

/* The four header pins that switch sensor power, and the two LED pins. The library decides the
 * electrical level (HDC and DRV are low = on, OPT and TMP are high = on). */
typedef enum {
    BS_PIN_HDC_PWR = 6,   /* HDC2080 power switch, BoosterPack pin 6 */
    BS_PIN_DRV_PWR = 24,  /* DRV5055 power switch, pin 24 */
    BS_PIN_OPT_PWR = 27,  /* OPT3001 V+, pin 27 */
    BS_PIN_TMP_PWR = 28,  /* TMP117 V+, pin 28 */
    BS_PIN_LED_RED = 39,  /* red LED through 1 kOhm, pin 39 (high = on); a board without the pin ignores it */
    BS_PIN_LED_GREEN = 40 /* green LED through 470 Ohm, pin 40 */
} bs_pin_t;

/* I2C controller on BoosterPack pins 9 (SCL) and 10 (SDA), 7-bit addresses, 100 kHz. Return 0 on success, -1 on a NACK or timeout. */
int  bs_port_i2c_write(uint8_t addr, const uint8_t *data, uint8_t n);   /* n may be 0 (address probe) */
int  bs_port_i2c_read(uint8_t addr, uint8_t *data, uint8_t n);
void bs_port_pin_write(bs_pin_t pin, int level);
/* DRV5055 output on BoosterPack pin 2, in millivolts against the 3.3 V rail (12-bit code * 3300 / 4095). Returns -1 when the board cannot read it. */
int32_t bs_port_adc_drv_mv(void);
void     bs_port_delay_ms(uint32_t ms);
uint32_t bs_port_micros(void);          /* free running, wraps after about 71 minutes */
#endif
