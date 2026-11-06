// I2C stub for uLisp: the calls compile, the bus is not driven.
#pragma once
#include "Arduino.h"
class TwoWire {
public:
  void begin() {}
  void end() {}
  void beginTransaction() {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 4; }   // 4 = other error: no device found
  uint8_t requestFrom(uint8_t, unsigned int) { return 0; }
  int read() { return -1; }
  size_t write(uint8_t) { return 0; }
};
extern TwoWire Wire;
