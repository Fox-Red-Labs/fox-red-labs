/* Pin map for the Arduino compatibility layer on the LP-MSPM0G3507.
 *
 * Arduino pin number = 32 * port + bit.   PA0..PA31 are 0..31, PB0..PB27 are 32..59.
 * Use the PA(n) and PB(n) macros so the sketch reads like the schematic. A pin number
 * that does not exist on this chip (PB28 to PB31, or 60 and up) stops the program at a
 * breakpoint instead of being ignored.
 */
#ifndef VARIANT_H
#define VARIANT_H

#define PA(n) (n)
#define PB(n) (32 + (n))
#define NUM_DIGITAL_PINS 60

/* LaunchPad */
#define LED_BUILTIN   PA(0)    /* red LED, LED1 */
#define PIN_S1        PA(18)   /* user button S1 */
#define PIN_S2        PB(21)   /* user button S2 */

/* Waveshare 3.5inch Capacitive Touch LCD, wired as in display-test/README.md */
#define PIN_LCD_CS    PB(6)    /* GPIO chip select, see CS_GAP below */
#define PIN_LCD_DC    PB(1)
#define PIN_LCD_RST   PB(4)
#define PIN_LCD_BL    PA(28)   /* high = backlight on */
#define PIN_TP_RST    PB(0)
#define PIN_TP_INT    PB(16)

/* Fixed by SysConfig (syscfg/arduino.syscfg) */
#define PIN_SPI_SCK   PB(9)    /* SPI1 */
#define PIN_SPI_MOSI  PB(8)
#define PIN_SPI_MISO  PB(7)
#define PIN_SERIAL_TX PA(10)   /* UART0, XDS110 backchannel */
#define PIN_SERIAL_RX PA(11)

/* Wire is bit-banged on GPIO (the I2C1 block did not reach the touch chip, see README) */
#define PIN_WIRE_SDA  PB(3)
#define PIN_WIRE_SCL  PB(2)

/* Hardware trap 1 (display-test README): after chip select goes high, the module's level
 * translator needs 10 us before it can go low again. digitalWrite(CS_GAP_PIN, HIGH) waits
 * CS_GAP_US after raising the pin, so the next falling edge is always at least that late. */
#define CS_GAP_PIN    PIN_LCD_CS
#define CS_GAP_US     10

#define F_CPU 32000000UL

#endif
