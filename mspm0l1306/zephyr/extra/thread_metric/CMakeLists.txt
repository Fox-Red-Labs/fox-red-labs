# SPDX-License-Identifier: Apache-2.0
# Thread-Metric for the LP-MSPM0L1306 (4 KB RAM). The test sources are Zephyr's own, read in place from
# $ZEPHYR_BASE/tests/benchmarks/thread_metric/src. Only the porting layer is replaced (src/), because
# Zephyr's reserves 10 stacks of 1,024 bytes plus buffers, 14 KB in all.
# Choose the test with -DTM_TEST=basic|cooperative|preemptive|interrupt|interrupt_preemption|memory|message|synchronization
# Optional: -DTM_DURATION=<seconds> (default 30, the standard), -DTM_STACK=<bytes> (default 256 for the two five-thread tests, else 512),
#           -DTM_BASIC_ARRAY=<ulongs> (basic test only, default 256; Zephyr's 1024 alone is 4,096 bytes).
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thread_metric_l1306)

set(TM_SRC $ENV{ZEPHYR_BASE}/tests/benchmarks/thread_metric/src)
if(NOT DEFINED TM_TEST)
  message(FATAL_ERROR "Pass -DTM_TEST=<test name>")
endif()
if(NOT DEFINED TM_DURATION)
  set(TM_DURATION 30)
endif()
if(NOT DEFINED TM_STACK)
  # Five-thread tests get 256 bytes per stack, the others 512, so every test links in 4 KB.
  if(TM_TEST STREQUAL cooperative OR TM_TEST STREQUAL preemptive)
    set(TM_STACK 256)
  else()
    set(TM_STACK 512)
  endif()
endif()
if(NOT DEFINED TM_BASIC_ARRAY)
  set(TM_BASIC_ARRAY 256)
endif()

set(TM_FILE_basic                 tm_basic_processing_test.c)
set(TM_FILE_cooperative           tm_cooperative_scheduling_test.c)
set(TM_FILE_preemptive            tm_preemptive_scheduling_test.c)
set(TM_FILE_interrupt             tm_interrupt_processing_test.c)
set(TM_FILE_interrupt_preemption  tm_interrupt_preemption_processing_test.c)
set(TM_FILE_memory                tm_memory_allocation_test.c)
set(TM_FILE_message               tm_message_processing_test.c)
set(TM_FILE_synchronization       tm_synchronization_processing_test.c)
set(TM_THREADS_basic 1)
set(TM_THREADS_cooperative 5)
set(TM_THREADS_preemptive 5)
set(TM_THREADS_interrupt 1)
set(TM_THREADS_interrupt_preemption 2)
set(TM_THREADS_memory 1)
set(TM_THREADS_message 1)
set(TM_THREADS_synchronization 1)
if(NOT DEFINED TM_FILE_${TM_TEST})
  message(FATAL_ERROR "Unknown TM_TEST ${TM_TEST}")
endif()

set(TM_TEST_C ${TM_SRC}/${TM_FILE_${TM_TEST}})
if(TM_TEST STREQUAL basic)
  # Zephyr's basic test sums a 1,024-entry array (4,096 bytes of RAM, the whole chip). Build a copy
  # with a smaller array. The count per period is then NOT comparable with other chips' basic counts.
  file(READ ${TM_SRC}/${TM_FILE_basic} TM_BASIC_TEXT)
  string(REPLACE "1024" "${TM_BASIC_ARRAY}" TM_BASIC_TEXT "${TM_BASIC_TEXT}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/tm_basic_small.c "${TM_BASIC_TEXT}")
  set(TM_TEST_C ${CMAKE_CURRENT_BINARY_DIR}/tm_basic_small.c)
endif()

target_include_directories(app PRIVATE ${TM_SRC})
target_sources(app PRIVATE src/tm_porting_layer_l1306.c ${TM_TEST_C})
target_compile_definitions(app PRIVATE TM_TEST_DURATION=${TM_DURATION}
  TM_TEST_NUM_THREADS=${TM_THREADS_${TM_TEST}} TM_TEST_STACK_SIZE=${TM_STACK})
