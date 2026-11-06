// ESP32-C6: the G3519 port's portable core (frl_gamepad.c, no library, no radio) on the original chip, same console block.
extern "C" {
#include "frl_gamepad.h"
}
static void hexs(String &s, const uint8_t *b, size_t n) { char t[4]; for (size_t i = 0; i < n; i++) { snprintf(t, 4, "%02x", b[i]); s += t; } }
static uint8_t map_[FRL_GP_MAX_DESC]; static size_t map_len; static uint8_t rpt[4][32]; static size_t rl[4];
static uint32_t desc_us, pack_us; static volatile uint8_t sink;
void setup() {
  Serial.begin(115200);
  frl_gp_config_t cfg; frl_gp_t g;
  frl_gp_default_config(&cfg); frl_gp_init(&g, &cfg);
  map_len = frl_gp_build_descriptor(&g, map_, sizeof map_);
  rl[0] = frl_gp_pack(&g, rpt[0]);
  frl_gp_press(&g, BUTTON_5); frl_gp_press(&g, BUTTON_16); frl_gp_pressStart(&g);
  frl_gp_setAxes(&g, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767); frl_gp_setHat1(&g, HAT_DOWN_RIGHT);
  rl[1] = frl_gp_pack(&g, rpt[1]);
  frl_gp_release(&g, BUTTON_5); frl_gp_releaseStart(&g); frl_gp_setHat1(&g, HAT_CENTERED); frl_gp_setAxes(&g, 0, 0, 0, 0, 0, 0, 0, 0);
  rl[2] = frl_gp_pack(&g, rpt[2]);
  frl_gp_press(&g, BUTTON_1); frl_gp_press(&g, BUTTON_8); frl_gp_press(&g, BUTTON_9); frl_gp_pressSelect(&g);
  frl_gp_setAxes(&g, -32768, -1, 1, 255, 256, -256, 12345, -12345); frl_gp_setHat1(&g, HAT_UP_LEFT);
  rl[3] = frl_gp_pack(&g, rpt[3]);
  uint8_t scratch[32];
  uint32_t t0 = ESP.getCycleCount();
  for (int i = 0; i < 200; i++) sink = (uint8_t)frl_gp_build_descriptor(&g, map_, sizeof map_);
  uint32_t t1 = ESP.getCycleCount();
  for (int i = 0; i < 1000; i++) sink = (uint8_t)frl_gp_pack(&g, scratch);
  uint32_t t2 = ESP.getCycleCount();
  uint32_t mhz = ESP.getCpuFreqMHz();
  desc_us = (t1 - t0) / 200 / mhz; pack_us = (t2 - t1) / 1000 / mhz;
  map_len = frl_gp_build_descriptor(&g, map_, sizeof map_);
}
void loop() {
  Serial.println("FRL-GP1 begin");
  Serial.printf("INFO heap_free=%u uptime_ms=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)millis());
  Serial.printf("TIME desc_us=%u pack_us=%u\n", desc_us, pack_us);
  String s = "MAP len=" + String((unsigned)map_len) + " "; hexs(s, map_, map_len); Serial.println(s);
  for (int i = 0; i < 4; i++) { s = "RPT" + String(i + 1) + " len=" + String((unsigned)rl[i]) + " "; hexs(s, rpt[i], rl[i]); Serial.println(s); }
  Serial.println("FRL-GP1 end"); delay(3000);
}
