/* Arduino compatibility layer for the TI LP-MSPM0G3507: the Arduino.h every library includes.
 * The API (Print, Stream, String, pinMode, ...) is the unmodified ArduinoCore-API.
 * What this folder adds is the part below the API: GPIO, SysTick time, UART, SPI, bit-banged Wire. */
#ifndef ARDUINO_H
#define ARDUINO_H

#define ARDUINO 10607
#define ARDUINO_ARCH_MSPM0
#define ARDUINO_LP_MSPM0G3507

#include "variant.h"
#include "api/ArduinoAPI.h"

#ifdef __cplusplus
using namespace arduino;
#endif

#define interrupts()   __asm volatile("cpsie i" ::: "memory")
#define noInterrupts() __asm volatile("cpsid i" ::: "memory")

#define clockCyclesPerMicrosecond() (F_CPU / 1000000UL)
#define clockCyclesToMicroseconds(a) ((a) / clockCyclesPerMicrosecond())
#define microsecondsToClockCycles(a) ((a) * clockCyclesPerMicrosecond())

/* Port-register access, used by libraries with a "fast pin I/O" path (Adafruit_BusIO software SPI).
   Port 0 = GPIOA, port 1 = GPIOB. The output register is DOUT31_0 (read-modify-write, not atomic). */
#ifdef __cplusplus
extern "C" volatile uint32_t *compat_port_out(uint8_t port);
extern "C" volatile uint32_t *compat_port_in(uint8_t port);
#define digitalPinToPort(pin)     ((uint8_t)((pin) >> 5))
#define digitalPinToBitMask(pin)  (1UL << ((pin) & 31))
#define portOutputRegister(port)  compat_port_out(port)
#define portInputRegister(port)   compat_port_in(port)
#endif

#ifdef __cplusplus
#include "HardwareSerial.h"
#endif

#endif
