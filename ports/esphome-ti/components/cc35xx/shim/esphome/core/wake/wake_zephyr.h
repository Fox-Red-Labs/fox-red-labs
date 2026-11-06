// TI CC35xx shim over ESPHome's wake_zephyr.h: the same declarations (implemented on a FreeRTOS semaphore in
// ti35_hal.cpp), plus the two socket-registration hooks the lwIP socket class calls when ESPHome's lwIP fast-select
// path is off. They return "not monitored", so each socket is polled (non-blocking) on every loop pass.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR

#include "esphome/core/hal.h"

namespace esphome {

void wake_loop_threadsafe();
inline void wake_loop_any_context() { wake_loop_threadsafe(); }
inline void wake_loop_isrsafe() { wake_loop_threadsafe(); }

#ifdef USE_CC35XX
inline bool wake_register_fd(int fd) { (void) fd; return false; }
inline void wake_unregister_fd(int fd) { (void) fd; }
#endif

namespace internal {
void wakeable_delay(uint32_t ms);
}  // namespace internal

}  // namespace esphome

#endif  // USE_ZEPHYR
