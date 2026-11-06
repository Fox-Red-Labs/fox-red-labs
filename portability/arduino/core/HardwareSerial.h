/* Serial on UART0 (PA10 TX, PA11 RX), the XDS110 backchannel. Not Serial1, no other UARTs. */
#ifndef HARDWARESERIAL_H
#define HARDWARESERIAL_H

#include "api/HardwareSerial.h"

class UartSerial : public arduino::HardwareSerial {
public:
  void begin(unsigned long baud) override;
  void begin(unsigned long baud, uint16_t config) override;   /* SERIAL_8N1 .. SERIAL_8O2, 5 to 8 bits */
  void end() override;
  int available(void) override;
  int peek(void) override;
  int read(void) override;
  void flush(void) override;                                   /* waits until the last bit has left */
  size_t write(uint8_t) override;                              /* blocks while the transmitter is full */
  using Print::write;
  operator bool() override { return true; }
  /* called from the UART0 interrupt */
  void _isr(void);
private:
  static const uint16_t RXSIZE = 128;
  volatile uint16_t head = 0, tail = 0;
  volatile uint8_t buf[RXSIZE];
};
extern UartSerial Serial;

#endif
