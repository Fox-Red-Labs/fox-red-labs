#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::die_temperature {

/// The chip's own temperature sensor on TI F3 chips (CC27xx: the AUX temperature sense; CC23xx: not wired yet, reads 0).
class DieTemperatureSensor final : public sensor::Sensor, public PollingComponent {
 public:
  void dump_config() override;
  void update() override;
};

}  // namespace esphome::die_temperature
