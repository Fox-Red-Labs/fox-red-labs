# SPDX-License-Identifier: Apache-2.0
# Zephyr's samples/synchronization with both thread stacks cut. The sample hard-codes STACKSIZE 1024 in
# its source (no Kconfig), so this builds a copy of main.c with that number replaced. Zephyr's file is read
# in place and not edited. -DSYNC_STACK=<bytes> (default 512).
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(synchronization_l1306)
if(NOT DEFINED SYNC_STACK)
  set(SYNC_STACK 512)
endif()
file(READ $ENV{ZEPHYR_BASE}/samples/synchronization/src/main.c SYNC_TEXT)
string(REPLACE "#define STACKSIZE 1024" "#define STACKSIZE ${SYNC_STACK}" SYNC_TEXT "${SYNC_TEXT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/sync_main.c "${SYNC_TEXT}")
target_sources(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/sync_main.c)
