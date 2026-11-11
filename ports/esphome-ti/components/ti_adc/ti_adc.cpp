// SPDX-License-Identifier: GPL-3.0-only
#include "ti_adc.h"
#include "esphome/core/log.h"

extern "C" int frl_adc_read_uv(int dio, int32_t *uv, int32_t *raw);  // shim/ti_periph.cpp

namespace esphome::ti_adc {

static const char *const TAG = "ti_adc";

void TiAdcSensor::update() {
  int32_t uv = 0, raw = 0;
  int rc = frl_adc_read_uv(this->pin_, &uv, &raw);
  if (rc != 0) {
    ESP_LOGW(TAG, "ADC read on DIO%u failed (%d)", this->pin_, rc);
    this->publish_state(NAN);
    return;
  }
  ESP_LOGD(TAG, "DIO%u raw %d, %d uV", this->pin_, (int) raw, (int) uv);
  this->publish_state(uv / 1.0e6f);
}

void TiAdcSensor::dump_config() {
  LOG_SENSOR("", "TI ADC", this);
  ESP_LOGCONFIG(TAG, "  Pin: DIO%u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::ti_adc
