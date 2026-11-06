#include <SPI.h>
// SPI.transfer() on spi0: SCLK DIO18 (BoosterPack pin 7), MOSI DIO13 (pin 15), MISO DIO12 (pin 14).
// With a wire from MOSI to MISO each byte comes back unchanged; with no wire the read data is undefined.
void setup() {
  Serial.begin(115200);
  delay(100);
  SPI.begin();
  const uint8_t tx[8] = {0x00, 0xFF, 0xA5, 0x5A, 0x01, 0x80, 0x3C, 0xC3};
  int match = 0;
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  Serial.print("tx/rx");
  for (int i = 0; i < 8; i++) {
    uint8_t rx = SPI.transfer(tx[i]);
    if (rx == tx[i]) match++;
    Serial.print(" "); Serial.print(tx[i], HEX); Serial.print("/"); Serial.print(rx, HEX);
  }
  SPI.endTransaction();
  Serial.println();
  Serial.print("loopback match "); Serial.print(match); Serial.println(" of 8");
}
void loop() { delay(1000); }
