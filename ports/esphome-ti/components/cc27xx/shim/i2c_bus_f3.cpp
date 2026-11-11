// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: ESPHome's I2C bus (the class ESPHome calls ZephyrI2CBus) on TI's I2C driver. The generated code constructs
// the bus with a device pointer the shim ignores; the bus number is the order of `i2c:` entries in the YAML, and each
// bus is a SysConfig instance (CONFIG_FRL_I2C_<n>) with the pins the YAML names (periph.py).
#include "esphome/core/defines.h"
#if defined(USE_I2C) && defined(USE_ZEPHYR)
#include <esphome/components/i2c/i2c_bus_zephyr.h>
#include "esphome/core/log.h"

extern "C" {
#include <ti/drivers/I2C.h>
#include "ti_drivers_config.h"
#include "frl_periph.h"
}

namespace esphome::i2c {

static const char *const TAG = "i2c";

struct FrlI2cBus {
  uint8_t cfg;
  uint32_t hz;
};
static const FrlI2cBus buses[] = FRL_I2C_BUSES;
static I2C_Handle handles[FRL_I2C_COUNT ? FRL_I2C_COUNT : 1];
static uint8_t next_bus = 0;

void ZephyrI2CBus::setup() {
  uint8_t n = next_bus++;
  this->port_ = n;
  if (n >= FRL_I2C_COUNT) {
    ESP_LOGE(TAG, "I2C bus %u has no TI driver instance", n);
    this->mark_failed();
    return;
  }
  I2C_init();
  I2C_Params params;
  I2C_Params_init(&params);
  params.bitRate = this->frequency_hz_ >= 1000000 ? I2C_1000kHz : (this->frequency_hz_ >= 400000 ? I2C_400kHz : I2C_100kHz);
  handles[n] = I2C_open(buses[n].cfg, &params);
  if (handles[n] == nullptr) {
    ESP_LOGE(TAG, "I2C_open failed for bus %u", n);
    this->mark_failed();
    return;
  }
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning I2C bus for active devices...");
    this->i2c_scan_();
  }
}

void ZephyrI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG,
                "I2C Bus:\n"
                "  SDA Pin: DIO%u\n"
                "  SCL Pin: DIO%u\n"
                "  Frequency: %u Hz (TI I2C driver)",
                this->sda_pin_, this->scl_pin_, (unsigned) this->frequency_hz_);
  if (this->scan_) {
    ESP_LOGI(TAG, "Results from I2C bus scan:");
    if (scan_results_.empty()) {
      ESP_LOGI(TAG, "Found no I2C devices!");
    } else {
      for (const auto &s : scan_results_) {
        if (s.second) {
          ESP_LOGI(TAG, "Found I2C device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

ErrorCode ZephyrI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                                    size_t read_count) {
  if (this->port_ >= FRL_I2C_COUNT || handles[this->port_] == nullptr) return ERROR_NOT_INITIALIZED;
  I2C_Transaction t{};
  uint8_t dummy = 0;
  t.targetAddress = address;
  if (write_count == 0 && read_count == 0) {
    // address probe (the bus scan): one zero byte written to the address; a device that ACKs is there
    t.writeBuf = &dummy;
    t.writeCount = 1;
  } else {
    t.writeBuf = const_cast<uint8_t *>(write_buffer);
    t.writeCount = write_count;
    t.readBuf = read_buffer;
    t.readCount = read_count;
  }
  bool ok = I2C_transfer(handles[this->port_], &t);
  if (ok) return ERROR_OK;
  if (t.status == I2C_STATUS_ADDR_NACK || t.status == I2C_STATUS_DATA_NACK) return ERROR_NOT_ACKNOWLEDGED;
  if (t.status == I2C_STATUS_TIMEOUT) return ERROR_TIMEOUT;
  return ERROR_UNKNOWN;
}

void ZephyrI2CBus::set_frequency(uint32_t frequency) { this->frequency_hz_ = frequency; }

}  // namespace esphome::i2c
#endif
