// SPI.transfer on SPI0 (PB18 SCLK, PB17 PICO, PB19 POCI) of the LP-MSPM0G3519 with no wire.
// Pass 1: 256 bytes out with the bus as wired; what comes back is recorded, not checked (no loopback wire).
// Pass 2: the SPI block's own loopback bit (CTL1.LBM, DL_SPI_enableLoopbackMode in TI's DriverLib) is set,
// so the receive side sees the transmit side inside the chip; every byte must come back unchanged.
// Prints "SPI: PASS" when pass 2 returns 256 of 256 bytes and transfer16 and the buffer transfer agree.
#include <SPI.h>
#define SPI0_CTL1 (*(volatile uint32_t *)(0x40468000u + 0x1104u))
const pin_size_t cs = 5;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("SPILoop start");
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
  SPI.begin();
  SPISettings s(1000000, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(s);
  digitalWrite(cs, LOW);
  int ff = 0, zero = 0;
  for (int i = 0; i < 256; i++) {
    uint8_t r = SPI.transfer((uint8_t)i);
    ff += r == 0xFF; zero += r == 0x00;
  }
  digitalWrite(cs, HIGH);
  Serial.print("pass 1, no wire: 256 bytes sent, read back 0xFF x"); Serial.print(ff);
  Serial.print(", 0x00 x"); Serial.println(zero);

  uint32_t ctl = SPI0_CTL1;            // LBM only takes while the block is disabled
  SPI0_CTL1 = ctl & ~0x1u;
  SPI0_CTL1 = (ctl & ~0x1u) | 0x2u;
  SPI0_CTL1 = ctl | 0x2u;
  Serial.print("CTL1 0x"); Serial.println(SPI0_CTL1, HEX);
  int good = 0;
  unsigned long t0 = micros();
  for (int i = 0; i < 256; i++) {
    uint8_t v = (uint8_t)(i * 7 + 3);
    good += SPI.transfer(v) == v;
  }
  unsigned long t1 = micros();
  uint16_t w = SPI.transfer16(0xA55A);
  uint8_t buf[16];
  for (int i = 0; i < 16; i++) buf[i] = 0x30 + i;
  SPI.transfer(buf, sizeof(buf));
  int bufgood = 0;
  for (int i = 0; i < 16; i++) bufgood += buf[i] == 0x30 + i;
  SPI0_CTL1 = ctl & ~0x1u;
  SPI0_CTL1 = ctl;
  SPI.endTransaction();
  Serial.print("pass 2, internal loopback: "); Serial.print(good); Serial.print(" of 256 equal, ");
  Serial.print((t1 - t0) / 256); Serial.println(" us per byte at 1 MHz");
  Serial.print("transfer16 0xA55A -> 0x"); Serial.println(w, HEX);
  Serial.print("buffer transfer "); Serial.print(bufgood); Serial.println(" of 16 equal");
  bool ok = good == 256 && w == 0xA55A && bufgood == 16;
  Serial.println(ok ? "SPI: PASS" : "SPI: FAIL");
}

void loop() {
  delay(1000);
}
