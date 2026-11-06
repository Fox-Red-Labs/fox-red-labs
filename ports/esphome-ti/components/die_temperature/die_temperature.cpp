#include "die_temperature.h"
#include "esphome/core/log.h"

extern "C" int32_t ti_die_temperature_c(void);  // shim/ti_glue.c: TI driverlib, returns degrees Celsius times 16

namespace esphome::die_temperature {

static const char *const TAG = "die_temperature";

void DieTemperatureSensor::update() { this->publish_state(static_cast<float>(ti_die_temperature_c()) / 16.0f); }

void DieTemperatureSensor::dump_config() { LOG_SENSOR("", "Die temperature", this); }

}  // namespace esphome::die_temperature
