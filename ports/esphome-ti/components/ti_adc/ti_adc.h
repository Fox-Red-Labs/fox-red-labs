// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::ti_adc {

class TiAdcSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void update() override;
  void dump_config() override;

 protected:
  uint8_t pin_{0};
};

}  // namespace esphome::ti_adc
