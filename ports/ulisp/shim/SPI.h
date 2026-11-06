// SPI stub for uLisp: the calls compile, the bus is not driven (BoosterPack pins are left alone).
#pragma once
#include "Arduino.h"
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
class SPISettings { public: SPISettings(unsigned long, uint8_t, uint8_t) {} };
class SPIClass {
public:
  void begin() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t) { return 0; }
};
extern SPIClass SPI;
