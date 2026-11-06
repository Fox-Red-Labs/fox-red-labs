// TI CC35xx shim (copied from the cc27xx platform): preferences stubbed (ESPHome's no-op Preferences, selected because CONFIG_SETTINGS is not defined).
#pragma once
#ifdef USE_ZEPHYR
namespace esphome::zephyr {
void setup_preferences();
}
#endif
