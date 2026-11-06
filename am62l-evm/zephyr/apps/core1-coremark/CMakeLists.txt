# CoreMark on Zephyr (E242 follow-up): EEMBC sources from $COREMARK_DIR, our timing port in port/. Not part of the repo's tree: fetch
# https://github.com/eembc/coremark and export COREMARK_DIR before west build.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(core1_coremark)
set(CM $ENV{COREMARK_DIR})
target_sources(app PRIVATE src/main.c ${CM}/core_list_join.c ${CM}/core_matrix.c ${CM}/core_state.c ${CM}/core_util.c
  ${CM}/core_main.c port/core_portme.c)
target_sources_ifdef(CONFIG_FRL_RING_CONSOLE app PRIVATE src/ring_console.c)
target_include_directories(app PRIVATE ${CM} port)
set_source_files_properties(${CM}/core_main.c PROPERTIES COMPILE_DEFINITIONS "main=coremark_main")
target_compile_definitions(app PRIVATE PERFORMANCE_RUN=1 ITERATIONS=${ITER} FLAGS_STR="-O2" MEM_METHOD=MEM_STATIC TOTAL_DATA_SIZE=2000 HAS_PRINTF=0 HAS_STDIO=0 MAIN_HAS_NOARGC=0)
