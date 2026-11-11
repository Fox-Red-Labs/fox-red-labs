// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"

namespace esphome::ti_pwm {

class TiPwmOutput : public output::FloatOutput, public Component {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_frequency(uint32_t hz) { this->hz_ = hz; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;
  uint8_t pin_{0};
  uint32_t hz_{1000};
  bool ok_{false};
};

}  // namespace esphome::ti_pwm
