#include "Wire.h"
#include "pins_internal.h"

TwoWire Wire(PIN_WIRE_SDA, PIN_WIRE_SCL);

TwoWire::TwoWire(pin_size_t sda, pin_size_t scl) : sda_(sda), scl_(scl), half_(CPUCLK_FREQ / 200000) {}

void TwoWire::sdaLow()  { DL_GPIO_enableOutput(pin_port(sda_), pin_mask(sda_)); }
void TwoWire::sdaHigh() { DL_GPIO_disableOutput(pin_port(sda_), pin_mask(sda_)); }
void TwoWire::sclLow()  { DL_GPIO_enableOutput(pin_port(scl_), pin_mask(scl_)); }
bool TwoWire::sdaRead() { return DL_GPIO_readPins(pin_port(sda_), pin_mask(sda_)) != 0; }
void TwoWire::sclHigh()
{
  DL_GPIO_disableOutput(pin_port(scl_), pin_mask(scl_));
  for (int t = 0; t < 2000 && !DL_GPIO_readPins(pin_port(scl_), pin_mask(scl_)); t++) ;   /* clock stretching */
}
void TwoWire::delayHalf() { delay_cycles(half_); }

void TwoWire::begin()
{
  /* plain GPIO inputs with pull-ups; output data 0, so "output enabled" drives the line low */
  pin_size_t p[2] = { sda_, scl_ };
  for (int i = 0; i < 2; i++) {
    DL_GPIO_initDigitalInputFeatures(pin_iomux(p[i]), DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
                                     DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(pin_port(p[i]), pin_mask(p[i]));
    DL_GPIO_disableOutput(pin_port(p[i]), pin_mask(p[i]));
  }
}
void TwoWire::end() { sdaHigh(); sclHigh(); }

void TwoWire::setClock(uint32_t hz)
{
  if (hz == 0) hz = 100000;
  uint32_t c = CPUCLK_FREQ / (2 * hz);
  half_ = c > 12 ? c - 12 : 1;    /* about 12 cycles of pin access per half period */
}

void TwoWire::start() { sdaHigh(); sclHigh(); delayHalf(); sdaLow(); delayHalf(); sclLow(); delayHalf(); }
void TwoWire::stop()  { sdaLow(); delayHalf(); sclHigh(); delayHalf(); sdaHigh(); delayHalf(); }

bool TwoWire::writeByte(uint8_t v)
{
  for (int b = 7; b >= 0; b--) {
    if (v & (1 << b)) sdaHigh(); else sdaLow();
    delayHalf(); sclHigh(); delayHalf(); sclLow();
  }
  sdaHigh(); delayHalf(); sclHigh(); delayHalf();
  bool ack = !sdaRead();
  sclLow(); delayHalf();
  return ack;
}

uint8_t TwoWire::readByte(bool ack)
{
  uint8_t v = 0;
  sdaHigh();
  for (int b = 7; b >= 0; b--) {
    delayHalf(); sclHigh(); delayHalf();
    if (sdaRead()) v |= 1 << b;
    sclLow();
  }
  if (ack) sdaLow(); else sdaHigh();
  delayHalf(); sclHigh(); delayHalf(); sclLow(); sdaHigh(); delayHalf();
  return v;
}

void TwoWire::beginTransmission(uint8_t address) { addr_ = address; txLen_ = 0; overflow_ = false; }

size_t TwoWire::write(uint8_t b)
{
  if (txLen_ >= BUFFER_LENGTH) { overflow_ = true; return 0; }
  tx_[txLen_++] = b;
  return 1;
}
size_t TwoWire::write(const uint8_t *data, size_t len)
{
  size_t n = 0;
  while (n < len && write(data[n])) n++;
  return n;
}

/* 0 success, 1 data too long, 2 NACK on address, 3 NACK on data */
uint8_t TwoWire::endTransmission(bool stopBit)
{
  if (overflow_) { overflow_ = false; return 1; }
  start();
  if (!writeByte(addr_ << 1)) { stop(); return 2; }
  for (uint8_t i = 0; i < txLen_; i++)
    if (!writeByte(tx_[i])) { stop(); return 3; }
  if (stopBit) stop();   /* otherwise SCL stays low and the next start() is a repeated start */
  txLen_ = 0;
  return 0;
}

size_t TwoWire::requestFrom(uint8_t address, size_t len, bool stopBit)
{
  if (len > BUFFER_LENGTH) len = BUFFER_LENGTH;
  rxLen_ = rxPos_ = 0;
  start();
  if (!writeByte((address << 1) | 1)) { stop(); return 0; }
  for (size_t i = 0; i < len; i++) rx_[i] = readByte(i < len - 1);
  rxLen_ = (uint8_t)len;
  if (stopBit) stop();
  return len;
}
