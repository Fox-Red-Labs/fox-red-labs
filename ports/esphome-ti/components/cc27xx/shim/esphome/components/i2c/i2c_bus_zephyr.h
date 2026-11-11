// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: ESPHome's I2C bus class for the Zephyr build, with TI's I2C driver underneath (i2c_bus_f3.cpp).
// Same class name and constructor as ESPHome's i2c_bus_zephyr.h, so the generated code needs no change; shadows that file.
#pragma once

#ifdef USE_ZEPHYR

#include "i2c_bus.h"
#include "esphome/core/component.h"

struct device;  // the generated code passes a Zephyr device pointer; the shim ignores it

namespace esphome::i2c {

class ZephyrI2CBus final : public InternalI2CBus, public Component {
 public:
  explicit ZephyrI2CBus(const device *i2c_dev) { (void) i2c_dev; }
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { this->sda_pin_ = sda_pin; }
  void set_scl_pin(uint8_t scl_pin) { this->scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency);

  int get_port() const override { return this->port_; }

 protected:
  uint8_t sda_pin_{};
  uint8_t scl_pin_{};
  uint8_t port_{0};
  uint32_t frequency_hz_{100000};
};

}  // namespace esphome::i2c

#endif
