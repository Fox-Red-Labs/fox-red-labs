# SPDX-License-Identifier: Apache-2.0
# Zephyr's samples/subsys/logging/logger with the demo thread stack cut. The sample hard-codes
# STACKSIZE (1024) in main.c, so this builds a copy with that number replaced. -DLOG_STACK=<bytes> (default 512).
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(logger_l1306)
set(LS $ENV{ZEPHYR_BASE}/samples/subsys/logging/logger/src)
if(NOT DEFINED LOG_STACK)
  set(LOG_STACK 512)
endif()
file(READ ${LS}/main.c LOG_TEXT)
string(REPLACE "#define STACKSIZE (1024)" "#define STACKSIZE (${LOG_STACK})" LOG_TEXT "${LOG_TEXT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/logger_main.c "${LOG_TEXT}")
target_include_directories(app PRIVATE ${LS})
target_sources(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/logger_main.c
  ${LS}/ext_log_system.c ${LS}/ext_log_system_adapter.c ${LS}/sample_instance.c ${LS}/sample_module.c)
