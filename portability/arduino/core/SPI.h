/* SPI on SPI1 (SCK PB9, MOSI PB8, MISO PB7), controller mode only. Chip select is the sketch's job (GPIO).
   The old methods (setClockDivider, setBitOrder, setDataMode, ...) are left out, so a sketch that uses
   them fails to compile. The ArduinoCore-API base class (SPIClass = HardwareSPI) forces four methods
   that this layer does not implement (usingInterrupt, notUsingInterrupt, attachInterrupt,
   detachInterrupt): calling one stops the program at a breakpoint (see compat_unsupported). */
#ifndef SPI_H
#define SPI_H

#include "Arduino.h"
#include "api/HardwareSPI.h"    /* SPISettings, SPI_MODE0..3, SPI_HAS_TRANSACTION */

class MspSPI : public arduino::HardwareSPI {
public:
  void begin() override;
  void end() override;
  void beginTransaction(SPISettings settings) override;
  void endTransaction(void) override;
  uint8_t transfer(uint8_t data) override;
  uint16_t transfer16(uint16_t data) override;         /* most significant byte first */
  void transfer(void *buf, size_t count) override;     /* in place, like every Arduino core */
  void usingInterrupt(int) override;
  void notUsingInterrupt(int) override;
  void attachInterrupt() override;
  void detachInterrupt() override;
private:
  bool begun_ = false;
  bool inTransaction_ = false;
  SPISettings current_;
  bool haveCurrent_ = false;
  void apply(const SPISettings &s);
};
extern MspSPI SPI;

#endif
