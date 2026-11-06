// TI CC35xx shim: ESPHome's logger writes to TI's console UART (the XDS110 application port, 115200 8N1, opened by
// TI's InitTerm() at boot). Replaces logger_zephyr.cpp, which needs Zephyr's UART API.
#include "esphome/core/defines.h"
#if defined(USE_CC35XX) && defined(USE_LOGGER)
#include "esphome/components/logger/logger.h"
#include "esphome/core/log.h"

extern "C" void FRL_log(const char *fmt, ...);

namespace esphome::logger {

void Logger::pre_setup() {
  global_logger = this;
  ESP_LOGI("logger", "Log initialized (TI console UART)");
}

void HOT Logger::write_msg_(const char *msg, uint16_t len) { FRL_log("%.*s\r\n", (int) len, msg); }

const LogString *Logger::get_uart_selection_() { return LOG_STR("TI console UART"); }

}  // namespace esphome::logger
#endif
