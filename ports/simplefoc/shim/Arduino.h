/* Arduino.h for SimpleFOC on a bare-metal TI core: only what SimpleFOC's core files name.
   No Arduino core is used. Time comes from hal/am13_hal.cpp (DWT cycle counter), the three Hall pins from GPIO reads. */
#ifndef FRL_ARDUINO_SHIM_H
#define FRL_ARDUINO_SHIM_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 1
#define RISING 2
#define FALLING 3
#define noInterrupts()
#define interrupts()
#define F(x) (x)
#define NOT_SET -12345.0f
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#define abs_f(x) fabsf(x)

typedef uint8_t byte;
class __FlashStringHelper;
/* SimpleFOC writes monitor text to a Print object; here every method discards it. */
class Print {
public:
    virtual ~Print() {}
    size_t print(const char*) { return 0; }
    size_t print(char) { return 0; }
    size_t print(int, int = 10) { return 0; }
    size_t print(unsigned int, int = 10) { return 0; }
    size_t print(long, int = 10) { return 0; }
    size_t print(float, int = 2) { return 0; }
    size_t print(double, int = 2) { return 0; }
    size_t println(const char*) { return 0; }
    size_t println(char) { return 0; }
    size_t println(int, int = 10) { return 0; }
    size_t println(long, int = 10) { return 0; }
    size_t println(float, int = 2) { return 0; }
    size_t println(double, int = 2) { return 0; }
    size_t println(void) { return 0; }
};

unsigned long micros(void);
unsigned long millis(void);
void delay(unsigned long ms);
void delayMicroseconds(unsigned long us);
void pinMode(int pin, int mode);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
void analogWrite(int pin, int value);
/* HallSensor is used in its polling mode (use_interrupt = 0), so these are never called; they only have to compile. */
#define digitalPinToInterrupt(p) (p)
inline void attachInterrupt(int, void (*)(void), int) {}
#endif
