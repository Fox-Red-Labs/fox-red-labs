/* Pin map for the Arduino compatibility layer on the LP-MSPM0G5187.
 *
 * Arduino pin number = 32 * port + bit.   PA0..PA31 are 0..31, PB0..PB26 are 32..58.
 * Use the PA(n) and PB(n) macros so the sketch reads like the schematic. A pin number
 * that does not exist on this chip stops the program at a breakpoint instead of being ignored.
 */
#ifndef VARIANT_H
#define VARIANT_H

#define PA(n) (n)
#define PB(n) (32 + (n))
#define NUM_DIGITAL_PINS 59

#define BOARD_NAME "LP-MSPM0G5187"

/* LaunchPad LEDs: PA17 (first in the list of TI's own blink), PB13, PA24 */
#define LED_BUILTIN   PA(17)
#define PIN_LED2      PB(13)
#define PIN_LED3      PA(24)

/* Fixed by SysConfig (arduino.syscfg) */
#define PIN_SPI_SCK   PB(9)    /* the J20 header, block UC3 */
#define PIN_SPI_MOSI  PB(8)
#define PIN_SPI_MISO  PB(7)
#define PIN_SERIAL_TX PA(10)   /* UC0, XDS110 backchannel */
#define PIN_SERIAL_RX PA(11)

/* Wire is bit-banged on GPIO; the kit's I2C pins are header pins 9 (PA9, clock) and 10 (PA8, data) */
#define PIN_WIRE_SDA  PA(8)
#define PIN_WIRE_SCL  PA(9)

/* No display module on this board, so no chip-select gap to enforce (the G3507 variant sets one for its screen) */
#define CS_GAP_PIN    NUM_DIGITAL_PINS
#define CS_GAP_US     0

#define F_CPU 32000000UL

#endif
