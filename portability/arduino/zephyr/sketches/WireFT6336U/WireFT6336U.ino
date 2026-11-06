// Wire on the LP-MSPM0G3519 through the Zephyr MSPM0 I2C driver: bus scan, then the FT6336U touch
// controller's ID registers (FocalTech FT6x36 register map: 0xA8 vendor 0x11, 0xA3 chip 0x64, 0xA6 firmware).
// Prints "WIRE: PASS" when the scan finds 0x38 and both ID bytes match.
#include <Wire.h>

static int readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return -1;
  if (Wire.requestFrom(addr, (size_t)1) != 1) return -2;
  return Wire.read();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("WireFT6336U start");
  Wire.begin();
  int found = 0;
  bool has38 = false;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    if (Wire.requestFrom(a, (size_t)1) == 1) {
      Wire.read();
      Serial.print("found 0x");
      Serial.println(a, HEX);
      found++;
      if (a == 0x38) has38 = true;
    }
  }
  Serial.print("scan found ");
  Serial.println(found);
  int vendor = readReg(0x38, 0xA8);
  int chip = readReg(0x38, 0xA3);
  int fw = readReg(0x38, 0xA6);
  int pts = readReg(0x38, 0x02);
  Serial.print("vendor 0x"); Serial.println(vendor, HEX);
  Serial.print("chip 0x"); Serial.println(chip, HEX);
  Serial.print("firmware 0x"); Serial.println(fw, HEX);
  Serial.print("touch points "); Serial.println(pts);
  unsigned long t0 = micros();
  int bad = 0;
  for (int i = 0; i < 100; i++) if (readReg(0x38, 0xA3) != 0x64) bad++;
  unsigned long t1 = micros();
  Serial.print("100 chip-id reads, wrong "); Serial.print(bad);
  Serial.print(", us per read "); Serial.println((t1 - t0) / 100);
  bool ok = has38 && vendor == 0x11 && chip == 0x64 && bad == 0;
  Serial.println(ok ? "WIRE: PASS" : "WIRE: FAIL");
}

void loop() {
  delay(1000);
}
