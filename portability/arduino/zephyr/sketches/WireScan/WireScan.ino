#include <Wire.h>
// Wire on i2c0 (SCL DIO24 = BoosterPack pin 9, SDA DIO0 = pin 10): scan 0x08..0x77, then read the
// HDC2080 ID (0x40, registers 0xFC..0xFF, expect 49 54 D0 07), its temperature, and the OPT3001 ID
// (0x44, registers 0x7E and 0x7F, expect 0x5449 and 0x3001) and one lux reading. BoosterPack pin 27 (OPT3001 power, high = on)
// is DIO5 = D4 here. Pin 6 (HDC2080 power, low = on) is not connected on this LaunchPad.
static int readRegs(uint8_t addr, uint8_t reg, uint8_t *buf, int n) {
  Wire.beginTransmission(addr); Wire.write(reg);
  if (Wire.endTransmission() != 0) return -1;
  int got = Wire.requestFrom(addr, (size_t)n);
  for (int i = 0; i < got && Wire.available(); i++) buf[i] = Wire.read();
  return got;
}
void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(4, OUTPUT); digitalWrite(4, HIGH);   // BP pin 27: OPT3001 on
  delay(20);
  Wire.begin();
  int found = 0;
  Serial.print("scan:");
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.print(" 0x"); Serial.print(a, HEX); found++; }
  }
  Serial.println(); Serial.print("found "); Serial.println(found);
  uint8_t id[4] = {0};
  int n = readRegs(0x40, 0xFC, id, 4);
  Serial.print("HDC2080 id n="); Serial.print(n);
  for (int i = 0; i < 4; i++) { Serial.print(" "); Serial.print(id[i], HEX); }
  Serial.println();
  if (n == 4) {
    uint8_t cfg[2] = {0x0F, 0x01};          // MEAS_CONFIG: start one measurement
    Wire.beginTransmission(0x40); Wire.write(cfg, 2); Wire.endTransmission();
    delay(10);
    uint8_t t[4] = {0};
    readRegs(0x40, 0x00, t, 4);
    uint16_t traw = t[0] | (t[1] << 8), hraw = t[2] | (t[3] << 8);
    Serial.print("HDC2080 T_raw="); Serial.print(traw); Serial.print(" T_C="); Serial.print(traw * 165.0 / 65536.0 - 40.0);
    Serial.print(" RH_raw="); Serial.print(hraw); Serial.print(" RH="); Serial.println(hraw * 100.0 / 65536.0);
  }
  uint8_t o[2] = {0};
  n = readRegs(0x44, 0x7E, o, 2); Serial.print("OPT3001 mfr n="); Serial.print(n); Serial.print(" 0x"); Serial.println((o[0] << 8) | o[1], HEX);
  n = readRegs(0x44, 0x7F, o, 2); Serial.print("OPT3001 dev n="); Serial.print(n); Serial.print(" 0x"); Serial.println((o[0] << 8) | o[1], HEX);
  if (n == 2) {
    uint8_t c[3] = {0x01, 0xCE, 0x10};       // CONFIG: automatic range, 800 ms, continuous
    Wire.beginTransmission(0x44); Wire.write(c, 3); Wire.endTransmission();
    delay(1000);
    readRegs(0x44, 0x00, o, 2);
    uint16_t r = (o[0] << 8) | o[1];
    Serial.print("OPT3001 raw=0x"); Serial.print(r, HEX);
    Serial.print(" lux="); Serial.println(0.01 * (1 << (r >> 12)) * (r & 0x0FFF));
  }
}
void loop() { delay(1000); }
