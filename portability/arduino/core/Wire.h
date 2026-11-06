/* Wire (I2C controller) bit-banged on two GPIO pins, open drain with the chip's pull-ups.
   Why not the I2C1 block: the touch chip on the Waveshare module answered only software I2C
   (display-test README, trap 2). Controller mode only: begin(address), onReceive and onRequest
   are left out, so a sketch that needs target mode fails to compile. */
#ifndef WIRE_H
#define WIRE_H

#include "Arduino.h"

#define BUFFER_LENGTH 32

class TwoWire : public Stream {
public:
  TwoWire(pin_size_t sda, pin_size_t scl);
  void begin();                                        /* controller */
  void end();
  void setClock(uint32_t hz);                          /* upper bound, the loop overhead makes it slower */
  void beginTransmission(uint8_t address);
  uint8_t endTransmission(bool stopBit);
  uint8_t endTransmission(void) { return endTransmission(true); }
  size_t requestFrom(uint8_t address, size_t len, bool stopBit);
  size_t requestFrom(uint8_t address, size_t len) { return requestFrom(address, len, true); }
  uint8_t requestFrom(uint8_t address, uint8_t len, uint8_t stopBit) { return (uint8_t)requestFrom(address, (size_t)len, stopBit != 0); }
  uint8_t requestFrom(uint8_t address, uint8_t len) { return (uint8_t)requestFrom(address, (size_t)len, true); }
  uint8_t requestFrom(int address, int len, int stopBit) { return (uint8_t)requestFrom((uint8_t)address, (size_t)len, stopBit != 0); }
  uint8_t requestFrom(int address, int len) { return (uint8_t)requestFrom((uint8_t)address, (size_t)len, true); }
  size_t write(uint8_t b) override;
  size_t write(const uint8_t *data, size_t len) override;
  using Print::write;
  int available(void) override { return rxLen_ - rxPos_; }
  int read(void) override { return rxPos_ < rxLen_ ? rx_[rxPos_++] : -1; }
  int peek(void) override { return rxPos_ < rxLen_ ? rx_[rxPos_] : -1; }
  void flush(void) override {}
private:
  pin_size_t sda_, scl_;
  uint32_t half_;           /* delay cycles per half clock */
  uint8_t addr_ = 0;
  uint8_t tx_[BUFFER_LENGTH], rx_[BUFFER_LENGTH];
  uint8_t txLen_ = 0, rxLen_ = 0, rxPos_ = 0;
  bool overflow_ = false;
  void sdaLow(); void sdaHigh(); void sclLow(); void sclHigh();
  bool sdaRead();
  void delayHalf();
  void start(); void stop();
  bool writeByte(uint8_t v);       /* true = acknowledged */
  uint8_t readByte(bool ack);
};
extern TwoWire Wire;

#endif
