# CoreMark under Zephyr on the FRDM-MCXA153, board-neutral: the shared EEMBC sources and Zephyr port of
# mspm0g3507/mcu-bench/src/coremark, with an app main of its own (no TI headers, nothing of the TI app changed).
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcxa153_coremark)
if(NOT DEFINED BENCH_COREMARK_ITERATIONS)
  set(BENCH_COREMARK_ITERATIONS 3500)
endif()
set(CM ${CMAKE_CURRENT_LIST_DIR}/../../mspm0g3507/mcu-bench/src/coremark)
target_sources(app PRIVATE src/main.c ${CM}/core_list_join.c ${CM}/core_main.c ${CM}/core_matrix.c ${CM}/core_state.c ${CM}/core_util.c ${CM}/core_portme.c)
target_include_directories(app PRIVATE ${CM})
target_compile_definitions(app PRIVATE ITERATIONS=${BENCH_COREMARK_ITERATIONS} PERFORMANCE_RUN=1 COMPILER_FLAGS="Zephyr 4.4.2 defaults, GCC 12.2.0")
set_source_files_properties(${CM}/core_main.c PROPERTIES COMPILE_DEFINITIONS "main=coremark_main")
