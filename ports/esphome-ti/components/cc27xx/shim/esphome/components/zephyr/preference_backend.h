// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: ESPHome's saved-settings backend, kept in RAM and written to flash through TI's NVS driver (see
// preferences_f3.cpp). Same class names as ESPHome's zephyr backend so the generated code and esphome/core/preferences.h
// need no change. Shadows esphome/components/zephyr/preference_backend.h; ESPHome's own file is not edited.
#pragma once
#ifdef USE_ZEPHYR
#ifdef CONFIG_SETTINGS

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace esphome::zephyr {

class ZephyrPreferenceBackend final {
 public:
  explicit ZephyrPreferenceBackend(uint32_t type) : type_(type) {}
  ZephyrPreferenceBackend(uint32_t type, std::vector<uint8_t> &&data) : data(std::move(data)), type_(type) {}

  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  uint32_t get_type() const { return this->type_; }

  std::vector<uint8_t> data;
  bool dirty{false};

 protected:
  uint32_t type_ = 0;
};

class ZephyrPreferences;
ZephyrPreferences *get_preferences();

}  // namespace esphome::zephyr

namespace esphome {
using PreferenceBackend = zephyr::ZephyrPreferenceBackend;
}  // namespace esphome

#endif  // CONFIG_SETTINGS
#endif  // USE_ZEPHYR
