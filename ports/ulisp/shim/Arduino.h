// Small Arduino API shim for uLisp on MSPM0, written fresh for this repo over TI's MSPM0 DriverLib.
// It covers only what ulisp-arm.ino calls: Serial on the back-channel UART, millis/micros/delay, pinMode/digitalWrite/digitalRead
// on the three LaunchPad LEDs (pins 13, 14, 15), and stubs for analog, tone, SPI and I2C.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define LSBFIRST 0
#define MSBFIRST 1
#define PSTR(s) (s)
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define PROGMEM
typedef uint8_t byte;
typedef bool boolean;
typedef uint8_t BitOrder;
enum eAnalogReference { AR_DEFAULT };

#define LED_BUILTIN 13
#define PIN_LED_A 13   // first LED the board lists (G5187: PA17)
#define PIN_LED_B 14   // second LED (G5187: PB13)
#define PIN_LED_C 15   // third LED (G5187: PA24)

extern "C" {
unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int analogRead(int pin);
}
static inline void analogWrite(int, int) {}
static inline void analogReference(eAnalogReference) {}
static inline void analogReadResolution(int) {}
static inline void analogWriteResolution(int) {}
static inline void tone(int, unsigned int) {}      // quiet hours: no audible output on any build
static inline void noTone(int) {}
static inline void interrupts(void) {}
static inline void noInterrupts(void) {}
long random(long max);
void randomSeed(unsigned long seed);

class FrlSerial {
public:
  FrlSerial(int port) : port_(port) {}
  void begin(unsigned long) {}
  void end() {}
  size_t write(uint8_t c);
  size_t print(const char *s) { size_t n = 0; while (*s) { write((uint8_t)*s++); n++; } return n; }
  size_t println(const char *s) { size_t n = print(s); write('\r'); write('\n'); return n + 2; }
  int available();
  int read();
  void flush() {}
  operator bool() const { return true; }
private:
  int port_;
};
extern FrlSerial Serial;
extern FrlSerial Serial1;
