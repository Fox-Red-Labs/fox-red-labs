// TI F3 shim: ESPHome's zephyr HAL/core/wake/gpio/preferences/temperature surface on FreeRTOS + TI drivers.
// One file on purpose: this is the whole "platform" the link test needed.
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/wake.h"
#include "esphome/core/preferences.h"
#include "esphome/components/zephyr/gpio.h"
#include "esphome/components/zephyr/preferences.h"
#if __has_include("esphome/components/internal_temperature/internal_temperature.h")
#include "esphome/components/internal_temperature/internal_temperature.h"
#define FRL_HAVE_INTERNAL_TEMPERATURE 1
#endif

extern "C" {
#include <FreeRTOS.h>
#include <semphr.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/HwiP.h>
void ti_fcfg_mac(uint8_t out[8]);
void ti_reset_system(void);
int32_t ti_die_temperature_c(void);
#include "ti_drivers_config.h"
}

namespace esphome {

// ---- hal.h ----
uint64_t millis_64() {
  static uint32_t last = 0;
  static uint32_t high = 0;
  uint32_t now = millis();
  if (now < last) high++;
  last = now;
  return (static_cast<uint64_t>(high) << 32) | now;
}
void delayMicroseconds(uint32_t us) {
  for (uint32_t i = 0; i < us * 24u; i++) __asm__ volatile("");
}
uint32_t arch_get_cpu_cycle_count() { return 0; }
void arch_feed_wdt() {}
void arch_init() {}
void arch_restart() { ti_reset_system(); __builtin_unreachable(); }

// ---- core.cpp (helpers.h platform classes) ----
Mutex::Mutex() { this->handle_ = xSemaphoreCreateMutex(); }
Mutex::~Mutex() { vSemaphoreDelete(static_cast<SemaphoreHandle_t>(this->handle_)); }
void Mutex::lock() { xSemaphoreTake(static_cast<SemaphoreHandle_t>(this->handle_), portMAX_DELAY); }
bool Mutex::try_lock() { return xSemaphoreTake(static_cast<SemaphoreHandle_t>(this->handle_), 0) == pdTRUE; }
void Mutex::unlock() { xSemaphoreGive(static_cast<SemaphoreHandle_t>(this->handle_)); }
InterruptLock::InterruptLock() { state_ = HwiP_disable(); }
InterruptLock::~InterruptLock() { HwiP_restore(state_); }
bool random_bytes(uint8_t *data, size_t len) {
  static uint32_t s = 0x2545F491u ^ xTaskGetTickCount();  // not cryptographic, so it reports failure; TI's RNG driver is the real source
  for (size_t i = 0; i < len; i++) {
    s = s * 1664525u + 1013904223u;
    data[i] = static_cast<uint8_t>(s >> 24);
  }
  return false;
}
void get_mac_address_raw(uint8_t *mac) {
  uint8_t full[8];
  ti_fcfg_mac(full);
  for (int i = 0; i < 6; i++) mac[i] = full[i];
}

// ---- wake_zephyr.cpp ----
static SemaphoreHandle_t wake_sem = nullptr;
volatile uint8_t g_wake_requested = 0;
void wake_loop_threadsafe() {
  wake_request_set();
  if (wake_sem != nullptr) xSemaphoreGive(wake_sem);
}
namespace internal {
void wakeable_delay(uint32_t ms) {
  if (wake_sem == nullptr) wake_sem = xSemaphoreCreateBinary();
  if (ms == 0) {
    yield();
    return;
  }
  xSemaphoreTake(wake_sem, ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(ms));
}
}  // namespace internal

// ---- preferences (stubbed) ----
ESPPreferences *global_preferences = nullptr;
namespace zephyr {
static ESPPreferences prefs_instance;
void setup_preferences() { global_preferences = &prefs_instance; }

// ---- gpio.cpp: LED pins only; DIO number -> GPIO driver config index from sysconfig ----
static int cfg_index(uint8_t dio) {
  switch (dio) {
#ifdef FRL_BOARD_CC2340R5
    case 14: return CONFIG_GPIO_RLED;  // LP-EM-CC2340R5: red LED on DIO14, green on DIO15 (checked in the generated ti_drivers_config.c)
    case 15: return CONFIG_GPIO_GLED;
#endif
    case 16: return CONFIG_GPIO_RLED;
    case 12: return CONFIG_GPIO_GLED;
    default: return -1;
  }
}
void ZephyrGPIOPin::setup() { this->pin_mode(this->flags_); }
void ZephyrGPIOPin::pin_mode(gpio::Flags flags) {
  int i = cfg_index(this->pin_);
  if (i < 0) return;
  if (flags & gpio::FLAG_OUTPUT) GPIO_setConfig(i, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
  else GPIO_setConfig(i, GPIO_CFG_IN_PU);
}
bool ZephyrGPIOPin::digital_read() {
  int i = cfg_index(this->pin_);
  return (i >= 0 ? GPIO_read(i) != 0 : false) != this->inverted_;
}
void ZephyrGPIOPin::digital_write(bool value) {
  int i = cfg_index(this->pin_);
  if (i >= 0) GPIO_write(i, value != this->inverted_);
}
size_t ZephyrGPIOPin::dump_summary(char *buffer, size_t len) const { return snprintf(buffer, len, "DIO%u", this->pin_); }
}  // namespace zephyr

// ---- internal_temperature_zephyr.cpp (only when the YAML uses ESPHome's internal_temperature; the cc27xx platform ships die_temperature) ----
#ifdef FRL_HAVE_INTERNAL_TEMPERATURE
namespace internal_temperature {
void InternalTemperatureSensor::setup() {}
void InternalTemperatureSensor::update() { this->publish_state(static_cast<float>(ti_die_temperature_c()) / 16.0f /* Q28.4: TI returns degrees times 16 */); }
}  // namespace internal_temperature
#endif
}  // namespace esphome
