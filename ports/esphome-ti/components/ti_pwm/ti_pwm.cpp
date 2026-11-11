// SPDX-License-Identifier: GPL-3.0-only
#include "ti_pwm.h"
#include "esphome/core/log.h"

extern "C" int frl_pwm_start(int dio, uint32_t hz);  // shim/ti_periph.cpp
extern "C" int frl_pwm_set(int dio, uint16_t duty);

namespace esphome::ti_pwm {

static const char *const TAG = "ti_pwm";

void TiPwmOutput::setup() {
  this->ok_ = frl_pwm_start(this->pin_, this->hz_) == 0;
  if (!this->ok_) {
    ESP_LOGE(TAG, "PWM on DIO%u did not start", this->pin_);
    this->mark_failed();
  }
}

void TiPwmOutput::write_state(float state) {
  if (!this->ok_) return;
  if (state < 0.0f) state = 0.0f;
  if (state > 1.0f) state = 1.0f;
  frl_pwm_set(this->pin_, static_cast<uint16_t>(state * 65535.0f));
}

void TiPwmOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "TI PWM output:\n  Pin: DIO%u\n  Frequency: %u Hz", this->pin_, (unsigned) this->hz_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace esphome::ti_pwm
