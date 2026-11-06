// Arduino API over hal.c. Pins 13, 14, 15 are the three LaunchPad LEDs; any other pin is ignored (reads 0).
#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "frl_hal.h"

SPIClass SPI;
TwoWire Wire;
FrlSerial Serial(0);
FrlSerial Serial1(1);

size_t FrlSerial::write(uint8_t c) { if (port_ == 0) hal_putc(c); return 1; }
// Optional start-up text: build.sh AUTOEXEC="..." writes autoexec.h, and the REPL reads that text first, as if typed.
#if defined(__has_include) && __has_include("autoexec.h")
#include "autoexec.h"
static const char *autoexec = FRL_AUTOEXEC;
// Start 500 ms after reset: uLisp empties its input once at start-up and would eat the text.
static int hal_getc2(void) { if (autoexec && *autoexec && hal_ms() > 500) return (unsigned char)*autoexec++; return hal_getc(); }
#else
static int hal_getc2(void) { return hal_getc(); }
#endif
static int peeked = -1;   // one byte of look-ahead for available(); -1 = none
int FrlSerial::available() { if (port_ != 0) return 0; if (peeked < 0) peeked = hal_getc2(); return peeked >= 0; }
int FrlSerial::read() { if (port_ != 0) return -1; if (peeked < 0) peeked = hal_getc2(); int c = peeked; peeked = -1; return c; }

extern "C" {
unsigned long millis(void) { return hal_ms(); }
unsigned long micros(void) { return hal_us(); }
void delay(unsigned long ms) { hal_delay_ms(ms); }
void delayMicroseconds(unsigned int us) { uint32_t s = hal_us(); while ((uint32_t)(hal_us() - s) < us) {} }
static int idx(int pin) { return (pin >= 13 && pin <= 15) ? pin - 13 : -1; }
void pinMode(int pin, int mode) { int i = idx(pin); if (i >= 0 && i < hal_pin_count()) hal_pin_mode(i, mode); }
void digitalWrite(int pin, int v) { int i = idx(pin); if (i >= 0 && i < hal_pin_count()) hal_pin_write(i, v); }
int digitalRead(int pin) { int i = idx(pin); return (i >= 0 && i < hal_pin_count()) ? hal_pin_read(i) : 0; }
int analogRead(int) { return 0; }   // no ADC channel is wired in this port: BoosterPack pins are not driven
}

static uint32_t rng = 2463534242u;
long random(long max) {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;   // xorshift32
  return max > 0 ? (long)(rng % (uint32_t)max) : 0;
}
void randomSeed(unsigned long seed) { if (seed) rng = (uint32_t)seed; }
