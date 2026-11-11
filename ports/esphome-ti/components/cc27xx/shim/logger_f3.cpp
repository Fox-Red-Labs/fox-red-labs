// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: ESPHome's logger writes to the LaunchPad kit's backchannel UART (the XDS110 application port) through TI's
// UART2 driver. Replaces logger_zephyr.cpp, which needs Zephyr's UART API. The UART instance is declared in SysConfig
// by periph.py (CONFIG_FRL_UART2_LOG) when the YAML has `logger:`.
#include "esphome/core/defines.h"
#if defined(FRL_TI_F3) && defined(USE_LOGGER)
#include "esphome/components/logger/logger.h"
#include "esphome/core/log.h"

extern "C" {
#include <FreeRTOS.h>
#include <semphr.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include "frl_periph.h"
}

namespace esphome::logger {

#ifdef FRL_HAVE_LOG_UART
static UART2_Handle log_uart = nullptr;
static SemaphoreHandle_t log_mutex = nullptr;
#endif

void Logger::pre_setup() {
  global_logger = this;
#ifdef FRL_HAVE_LOG_UART
  if (this->baud_rate_ > 0) {
    UART2_Params params;
    UART2_Params_init(&params);
    params.baudRate = this->baud_rate_;
    log_uart = UART2_open(CONFIG_FRL_UART2_LOG, &params);
    log_mutex = xSemaphoreCreateMutex();
  }
#endif
  ESP_LOGI("logger", "Log initialized");
}

void HOT Logger::write_msg_(const char *msg, uint16_t len) {
#ifdef FRL_HAVE_LOG_UART
  if (log_uart == nullptr) return;
  size_t written = 0;
  // two tasks log (ZBOSS and ESPHome's loop): keep each line whole on the wire
  bool locked = log_mutex != nullptr && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
  if (locked) xSemaphoreTake(log_mutex, portMAX_DELAY);
  UART2_write(log_uart, msg, len, &written);
  if (locked) xSemaphoreGive(log_mutex);
#else
  (void) msg;
  (void) len;
#endif
}

const LogString *Logger::get_uart_selection_() { return LOG_STR("backchannel UART"); }

}  // namespace esphome::logger
#endif
