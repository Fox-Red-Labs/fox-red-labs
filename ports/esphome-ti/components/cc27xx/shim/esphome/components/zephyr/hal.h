// TI F3 shim for ESPHome's "zephyr" HAL header. Same names, FreeRTOS underneath.
// Shadows esphome/components/zephyr/hal.h on the include path; ESPHome's own file is not edited.
#pragma once
#ifdef USE_ZEPHYR
#include <cstdint>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
}
#define IRAM_ATTR
#define PROGMEM
// The generated main.cpp passes these to ZephyrGPIOPin; the shim ignores them.
#define DEVICE_DT_GET_OR_NULL(x) (static_cast<const void *>(nullptr))
#define DEVICE_DT_GET(x) (static_cast<const struct device *>(nullptr))
#define DT_NODELABEL(x) 0
extern "C" {
size_t frl_heap_free(void);      // FreeRTOS heap bytes free now (shim/ti_periph.cpp)
size_t frl_heap_min_free(void);  // lowest value since boot
}
namespace esphome::zephyr {}
namespace esphome {
inline bool in_isr_context() { return false; }
inline void yield() { taskYIELD(); }
inline void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
inline uint32_t millis() { return static_cast<uint32_t>(xTaskGetTickCount()) * portTICK_PERIOD_MS; }
uint64_t millis_64();
inline uint32_t micros() { return millis() * 1000u; }
void delayMicroseconds(uint32_t us);
uint32_t arch_get_cpu_cycle_count();
inline uint32_t arch_get_cpu_freq_hz() { return 96000000u; }
void arch_feed_wdt();
void arch_init();
}  // namespace esphome
#endif
