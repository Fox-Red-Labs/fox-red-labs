/* Not from a library: a small check of the compatibility layer itself, no display library.
   Flash this first. If it prints and blinks, Serial, millis, micros, delay, GPIO and Wire work.
   Serial (115200 8N1, XDS110 backchannel): once a second, uptime from millis() and micros(),
   the two buttons, and the touch chip's three ID registers read over Wire (expect 0x11 0x64 0x12).
   The red LED toggles every 500 ms. S1 is active-high on this LaunchPad, so holding it does not stop the toggling (see FAQ). */
#include <Wire.h>

#define TP_ADDR 0x38

static int readReg(uint8_t reg)
{
  Wire.beginTransmission(TP_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (size_t)1) != 1) return -2;
  return Wire.read();
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);
  pinMode(PIN_TP_RST, OUTPUT);          // touch chip reset, then it needs about 300 ms
  digitalWrite(PIN_TP_RST, LOW); delay(10);
  digitalWrite(PIN_TP_RST, HIGH); delay(300);
  Wire.begin();
  Serial.println("shimcheck: Arduino layer on LP-MSPM0G3507");
}

void loop()
{
  static uint32_t last = 0, blink = 0;
  uint32_t now = millis();
  if (now - blink >= 500) {
    blink = now;
    if (digitalRead(PIN_S1) == HIGH) digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  if (now - last >= 1000) {
    last = now;
    Serial.print("millis=");  Serial.print(now);
    Serial.print(" micros=");  Serial.print(micros());
    Serial.print(" S1=");      Serial.print(digitalRead(PIN_S1));
    Serial.print(" S2=");      Serial.print(digitalRead(PIN_S2));
    Serial.print(" touch vendor/chip/fw=0x");
    Serial.print(readReg(0xA8), HEX); Serial.print(" 0x");
    Serial.print(readReg(0xA3), HEX); Serial.print(" 0x");
    Serial.println(readReg(0xA6), HEX);
  }
}
