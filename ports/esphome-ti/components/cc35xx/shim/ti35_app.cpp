// TI CC35xx shim: TI's main_freertos.c starts mainThread() on a pthread; ESPHome's setup() and loop() run there.
#include <cstdlib>
#include <new>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
void InitTerm(void);
int LED_IF_init(void);  /* TI's wifi_if.c drives the green LED as a Wi-Fi status light (WIFI_LED_INDEX) */
void FRL_log(const char *fmt, ...);
void frl_api_selftest_start(void) __attribute__((weak));
}

void setup();
void loop();

// C++ new/delete on the FreeRTOS heap, as C malloc already is in TI's configuration.
void *operator new(size_t n) { return pvPortMalloc(n); }
void *operator new[](size_t n) { return pvPortMalloc(n); }
void *operator new(size_t n, std::align_val_t) { return pvPortMalloc(n); }
void operator delete(void *p) noexcept { vPortFree(p); }
void operator delete[](void *p) noexcept { vPortFree(p); }
void operator delete(void *p, size_t) noexcept { vPortFree(p); }
void operator delete[](void *p, size_t) noexcept { vPortFree(p); }
void operator delete(void *p, std::align_val_t) noexcept { vPortFree(p); }

extern "C" void *mainThread(void *arg) {
  (void) arg;
  InitTerm();
  LED_IF_init();
  FRL_log("\r\n[esphome-cc35xx] boot, free heap %u\r\n", (unsigned) xPortGetFreeHeapSize());
  setup();
  if (frl_api_selftest_start)
    frl_api_selftest_start();
  FRL_log("[esphome-cc35xx] setup done, free heap %u\r\n", (unsigned) xPortGetFreeHeapSize());
  uint32_t n = 0;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    loop();
    n++;
    if (xTaskGetTickCount() - last >= pdMS_TO_TICKS(20000)) {
      last = xTaskGetTickCount();
      FRL_log("[esphome-cc35xx] alive, %u loops, free heap %u, min %u\r\n", (unsigned) n, (unsigned) xPortGetFreeHeapSize(),
              (unsigned) xPortGetMinimumEverFreeHeapSize());
    }
  }
  return nullptr;
}
