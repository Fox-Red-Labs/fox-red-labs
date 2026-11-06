// Fox Red Labs check harness for ESP32-BLE-Gamepad (lemmingDev, MIT). Runs the library unmodified.
// Prints the check block (protocol FRL-GP1) every 3 s, then behaves like upstream examples/Generic/Gamepad.
#include <Arduino.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <NimBLEDevice.h>
#define private public
#define protected public
#include <BleGamepad.h>
#undef private
#undef protected

BleGamepad bleGamepad;

static String blockText;
static void hexline(const char *tag, const uint8_t *p, size_t n) {
  char b[8]; blockText += tag; blockText += " len="; blockText += String((unsigned)n); blockText += " ";
  for (size_t i = 0; i < n; i++) { snprintf(b, sizeof b, "%02x", p[i]); blockText += b; }
  blockText += "\n";
}
static void report(const char *tag) {
  bleGamepad.sendGenericReport();
  NimBLEAttValue v = bleGamepad.inputGamepad->getValue();
  hexline(tag, v.data(), v.size());
}
static uint32_t descUs = 0, packUs = 0;
static bool checked = false;
static void runChecks() {
  hexline("MAP", bleGamepad.tempHidReportDescriptor, bleGamepad.hidReportDescriptorSize);
  report("RPT1");  // idle
  bleGamepad.press(BUTTON_5); bleGamepad.press(BUTTON_16); bleGamepad.pressStart();
  bleGamepad.setAxes(32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
  bleGamepad.setHat1(HAT_DOWN_RIGHT);
  report("RPT2");
  bleGamepad.release(BUTTON_5); bleGamepad.releaseStart(); bleGamepad.setHat1(HAT_CENTERED);
  bleGamepad.setAxes(0, 0, 0, 0, 0, 0, 0, 0);
  report("RPT3");
  bleGamepad.press(BUTTON_1); bleGamepad.press(BUTTON_8); bleGamepad.press(BUTTON_9); bleGamepad.pressSelect();
  bleGamepad.setAxes(-32768, -1, 1, 255, 256, -256, 12345, -12345);
  bleGamepad.setHat1(HAT_UP_LEFT);
  report("RPT4");
  uint32_t t = micros();
  for (int i = 0; i < 200; i++) { bleGamepad.hidReportDescriptorSize = 0; bleGamepad.buildGenericDescriptor(); }
  descUs = (micros() - t) / 200;
  t = micros();
  for (int i = 0; i < 1000; i++) bleGamepad.sendGenericReport();
  packUs = (micros() - t) / 1000;
  // leave the gamepad idle for the upstream loop
  bleGamepad.resetButtons(); bleGamepad.setHat1(HAT_CENTERED); bleGamepad.setAxes(0, 0, 0, 0, 0, 0, 0, 0);
  checked = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
  uint32_t t0 = millis();
  while (bleGamepad.inputGamepad == nullptr && millis() - t0 < 8000) delay(10);
  delay(500);
  runChecks();
}

static uint32_t lastBlock = 0;
void loop() {
  if (millis() - lastBlock > 3000) {
    lastBlock = millis();
    Serial.println("FRL-GP1 begin");
    Serial.printf("INFO heap_free=%u uptime_ms=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)millis());
    Serial.printf("TIME desc_us=%u pack_us=%u\n", descUs, packUs);
    Serial.print(blockText);
    Serial.println("FRL-GP1 end");
  }
  if (bleGamepad.isConnected()) {
    Serial.println("Press buttons 5, 16 and start. Move all enabled axes to max. Set DPAD (hat 1) to down right.");
    bleGamepad.press(BUTTON_5); bleGamepad.press(BUTTON_16); bleGamepad.pressStart();
    bleGamepad.setAxes(32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
    bleGamepad.setHat1(HAT_DOWN_RIGHT);
    delay(500);
    Serial.println("Release button 5 and start. Move all axes to min. Set DPAD (hat 1) to centred.");
    bleGamepad.release(BUTTON_5); bleGamepad.releaseStart(); bleGamepad.setHat1(HAT_CENTERED);
    bleGamepad.setAxes(0, 0, 0, 0, 0, 0, 0, 0);
    delay(500);
  }
}
