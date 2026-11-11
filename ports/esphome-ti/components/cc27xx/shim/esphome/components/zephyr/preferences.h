// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: ESPHome's preferences manager on TI's NVS driver (see preferences_f3.cpp).
#pragma once
#ifdef USE_ZEPHYR
#ifdef CONFIG_SETTINGS

#include "esphome/core/preference_backend.h"
#include <vector>

namespace esphome::zephyr {

class ZephyrPreferences final : public PreferencesMixin<ZephyrPreferences> {
 public:
  using PreferencesMixin<ZephyrPreferences>::make_preference;
  void open();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) {
    return this->make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type);
  /// One-shot read of a stored preference by key, without registering a backend
  bool load_from_key(uint32_t type, uint8_t *data, size_t len);
  bool sync();
  bool reset();

  std::vector<ZephyrPreferenceBackend *> backends_;

 protected:
  ZephyrPreferenceBackend *find_backend_(uint32_t type);
};

void setup_preferences();

}  // namespace esphome::zephyr

DECLARE_PREFERENCE_ALIASES(esphome::zephyr::ZephyrPreferences)

#endif  // CONFIG_SETTINGS
#endif  // USE_ZEPHYR
