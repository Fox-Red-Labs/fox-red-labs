# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcu_bench)

if(NOT DEFINED BENCH_COREMARK_ITERATIONS)
  set(BENCH_COREMARK_ITERATIONS 3000)
endif()

# CoreMark data size. 2000 is the standard performance run. Boards with 8 KB of RAM or less
# get 1200, CoreMark's own profile-run size: the seeds change to 8,8,8 and CoreMark still checks
# its results against known CRCs, but the score is NOT comparable with 2000-byte scores.
if(NOT DEFINED BENCH_COREMARK_DATA)
  if(CONFIG_SRAM_SIZE LESS_EQUAL 8)
    set(BENCH_COREMARK_DATA 1200)
  else()
    set(BENCH_COREMARK_DATA 2000)
  endif()
endif()
if(BENCH_COREMARK_DATA EQUAL 2000)
  set(BENCH_RUN PERFORMANCE_RUN=1)
elseif(BENCH_COREMARK_DATA EQUAL 1200)
  set(BENCH_RUN PROFILE_RUN=1)
else()
  set(BENCH_RUN VALIDATION_RUN=1)
endif()

if(BENCH_O3)
  # Whole application at -O3 with loop unrolling; the kernel and C library stay at -O2.
  set(BENCH_OPT "-O3 -funroll-loops")
  target_compile_options(app PRIVATE -O3 -funroll-loops)
elseif(CONFIG_SPEED_OPTIMIZATIONS)
  set(BENCH_OPT "-O2")
else()
  set(BENCH_OPT "-Os")
endif()

# EEMBC CoreMark core files, one copy for every board; only core_portme.* is this board's.
set(COREMARK ${CMAKE_CURRENT_SOURCE_DIR}/../../../third-party/coremark)

target_sources(app PRIVATE
  src/main.c
  ${COREMARK}/core_list_join.c
  ${COREMARK}/core_main.c
  ${COREMARK}/core_matrix.c
  ${COREMARK}/core_state.c
  ${COREMARK}/core_util.c
  src/coremark/core_portme.c
)
if(CONFIG_SOC_SERIES_MSPM0G)
  # The math accelerator driver is not built by Zephyr for the MSPM0 board, so take it from the TI HAL module.
  # (CC2340R5 and MSPM0L1306 have no math accelerator.)
  target_sources(app PRIVATE ${ZEPHYR_TI_MODULE_DIR}/mspm0/source/ti/driverlib/dl_mathacl.c)
endif()
target_include_directories(app PRIVATE src/coremark ${COREMARK})
target_compile_definitions(app PRIVATE
  ITERATIONS=${BENCH_COREMARK_ITERATIONS}
  TOTAL_DATA_SIZE=${BENCH_COREMARK_DATA}
  ${BENCH_RUN}
  BENCH_OPT="${BENCH_OPT}"
  COMPILER_FLAGS="${BENCH_OPT} -mcpu=cortex-m0plus -mthumb, Zephyr 4.4.2 defaults"
)
if(BENCH_COREMARK_ONLY)
  # CoreMark alone: the other tests and the second thread are compiled out.
  target_compile_definitions(app PRIVATE BENCH_COREMARK_ONLY=1)
endif()
# CoreMark's entry point is called from our main().
set_source_files_properties(${COREMARK}/core_main.c PROPERTIES COMPILE_DEFINITIONS "main=coremark_main")

# BENCH_O3 alone is what the G3507 runs used: target_compile_options puts -O3 BEFORE Zephyr's own
# -O2 on the compile line, and GCC obeys the last -O, so the app really builds at -O2 -funroll-loops.
# BENCH_TRUE_O3=1 adds -O3 -funroll-loops as a source-file property, which CMake places after every
# target option, so -O3 is the last -O for main.c and the CoreMark files. The kernel stays at -O2.
if(BENCH_O3 AND BENCH_TRUE_O3)
  get_target_property(BENCH_APP_SOURCES app SOURCES)
  set_property(SOURCE ${BENCH_APP_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -O3 -funroll-loops)
endif()
