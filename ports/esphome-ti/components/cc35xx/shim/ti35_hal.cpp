// TI CC35xx shim: ESPHome's zephyr HAL/core/wake/gpio/preferences surface on FreeRTOS + TI drivers.
// Derived from the cc27xx platform's ti_hal.cpp; the CC35xx changes are the reset, the MAC address and the GPIO mapping.
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/wake.h"
#include "esphome/core/preferences.h"
#include "esphome/components/zephyr/gpio.h"
#include "esphome/components/zephyr/preferences.h"

extern "C" {
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/HwiP.h>
int FRL_mac_address(uint8_t out[6]);
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
  for (uint32_t i = 0; i < us * 40u; i++) __asm__ volatile("");
}
uint32_t arch_get_cpu_cycle_count() { return 0; }
void arch_feed_wdt() {}
void arch_init() {}
void arch_restart() {
  *reinterpret_cast<volatile uint32_t *>(0xE000ED0Cu) = 0x05FA0004u;  // SCB->AIRCR = VECTKEY | SYSRESETREQ
  for (;;) {
  }
}

// ---- helpers.h platform classes ----
Mutex::Mutex() { this->handle_ = xSemaphoreCreateMutex(); }
Mutex::~Mutex() { vSemaphoreDelete(static_cast<SemaphoreHandle_t>(this->handle_)); }
void Mutex::lock() { xSemaphoreTake(static_cast<SemaphoreHandle_t>(this->handle_), portMAX_DELAY); }
bool Mutex::try_lock() { return xSemaphoreTake(static_cast<SemaphoreHandle_t>(this->handle_), 0) == pdTRUE; }
void Mutex::unlock() { xSemaphoreGive(static_cast<SemaphoreHandle_t>(this->handle_)); }
InterruptLock::InterruptLock() { state_ = HwiP_disable(); }
InterruptLock::~InterruptLock() { HwiP_restore(state_); }
bool random_bytes(uint8_t *data, size_t len) {
  // Not cryptographic, so it reports failure: callers that need real randomness (API encryption) stop instead of
  // using it, while ESPHome's scheduler jitter, which ignores the result, still gets a seed. TI's TRNG driver is the fix.
  static uint32_t s = 0x2545F491u ^ xTaskGetTickCount();
  for (size_t i = 0; i < len; i++) {
    s = s * 1664525u + 1013904223u;
    data[i] = static_cast<uint8_t>(s >> 24);
  }
  return false;
}
void get_mac_address_raw(uint8_t *mac) {
  if (FRL_mac_address(mac) != 0) {
    static const uint8_t fallback[6] = {0x02, 0x46, 0x52, 0x4c, 0x35, 0x01};  // locally administered placeholder
    for (int i = 0; i < 6; i++) mac[i] = fallback[i];
  }
}

// ---- wake (wake_zephyr.cpp's surface, on a FreeRTOS semaphore) ----
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

// ---- preferences (stubbed: nothing survives a reset yet) ----
ESPPreferences *global_preferences = nullptr;
namespace zephyr {
static ESPPreferences prefs_instance;
void setup_preferences() { global_preferences = &prefs_instance; }

// ---- gpio: the TI GPIO driver on CC35xx indexes pins by GPIO number ----
void ZephyrGPIOPin::setup() { this->pin_mode(this->flags_); }
void ZephyrGPIOPin::pin_mode(gpio::Flags flags) {
  if (flags & gpio::FLAG_OUTPUT) GPIO_setConfig(this->pin_, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
  else GPIO_setConfig(this->pin_, GPIO_CFG_IN_PU);
}
bool ZephyrGPIOPin::digital_read() { return (GPIO_read(this->pin_) != 0) != this->inverted_; }
void ZephyrGPIOPin::digital_write(bool value) { GPIO_write(this->pin_, value != this->inverted_); }
size_t ZephyrGPIOPin::dump_summary(char *buffer, size_t len) const { return snprintf(buffer, len, "GPIO%u", this->pin_); }
}  // namespace zephyr

#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS
// socket.cpp's Zephyr branch (which defines this) is renamed out in the build copy, because the same file's Zephyr
// address helpers need Zephyr's network API. Sockets are polled every loop pass, so a socket is always worth reading.
namespace socket {
bool socket_ready_fd(int fd, bool loop_monitored) { (void) fd; (void) loop_monitored; return true; }
}  // namespace socket
#endif
}  // namespace esphome
