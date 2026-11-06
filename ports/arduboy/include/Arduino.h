/*
 * Arduino core subset for running Arduboy2 games on an Arm MCU or a host (Fox Red Labs, BSD-3-Clause).
 * Only what games call: types, math helpers, time, random, PROGMEM, F(), Print.
 * No C++ standard library headers are used, so it builds with tiarmclang and arm-none-eabi-gcc
 * without libc++/libstdc++.
 */
#ifndef FRL_ARDUINO_H
#define FRL_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>

typedef uint8_t byte;
typedef uint8_t boolean;
typedef uint16_t word;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define _BV(b) (1u << (b))
#define bit(b) (1u << (b))
#define bitRead(v, b) (((v) >> (b)) & 1)
#define bitSet(v, b) ((v) |= (1UL << (b)))
#define bitClear(v, b) ((v) &= ~(1UL << (b)))
#define bitWrite(v, b, x) ((x) ? bitSet(v, b) : bitClear(v, b))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define radians(d) ((d) * DEG_TO_RAD)
#define degrees(r) ((r) * RAD_TO_DEG)
#define sq(x) ((x) * (x))
#ifdef abs
#undef abs
#endif
#define abs(x) ((x) > 0 ? (x) : -(x))
#define cli()
#define sei()
#define interrupts()
#define noInterrupts()

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(PSTR(s)))

#ifdef __cplusplus
extern "C" {
#endif
unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void randomSeed(unsigned long seed);
long frl_random_max(long max);
long frl_random_range(long lo, long hi);
long map(long x, long in_min, long in_max, long out_min, long out_max);
static inline void pinMode(uint8_t, uint8_t) {}
static inline void digitalWrite(uint8_t, uint8_t) {}
static inline int digitalRead(uint8_t) { return 0; }
static inline int analogRead(uint8_t) { return 0; }
#ifdef __cplusplus
}
static inline long random(long max) { return frl_random_max(max); }
static inline long random(long lo, long hi) { return frl_random_range(lo, hi); }

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *buf, size_t n) { size_t k = 0; while (n--) k += write(*buf++); return k; }
    size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }

    size_t print(const __FlashStringHelper *f) { return write((const char *)f); }
    size_t print(const char *s) { return write(s); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(unsigned char v, int base = DEC) { return print((unsigned long)v, base); }
    size_t print(int v, int base = DEC) { return print((long)v, base); }
    size_t print(unsigned int v, int base = DEC) { return print((unsigned long)v, base); }
    size_t print(long v, int base = DEC);
    size_t print(unsigned long v, int base = DEC);
    size_t print(double v, int digits = 2);
    size_t println() { return write((uint8_t)'\r') + write((uint8_t)'\n'); }
    size_t println(const __FlashStringHelper *f) { return print(f) + println(); }
    size_t println(const char *s) { return print(s) + println(); }
    size_t println(char c) { return print(c) + println(); }
    size_t println(unsigned char v, int base = DEC) { return print(v, base) + println(); }
    size_t println(int v, int base = DEC) { return print(v, base) + println(); }
    size_t println(unsigned int v, int base = DEC) { return print(v, base) + println(); }
    size_t println(long v, int base = DEC) { return print(v, base) + println(); }
    size_t println(unsigned long v, int base = DEC) { return print(v, base) + println(); }
    size_t println(double v, int digits = 2) { return print(v, digits) + println(); }
};
#endif

#endif
