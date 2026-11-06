# Compile-and-link check for gatt_gen's Zephyr output. build.sh generates the services into the build folder.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gatt_gen_check)
target_include_directories(app PRIVATE ${GEN_DIR})
target_sources(app PRIVATE src/main.c ${GEN_DIR}/blinky_service.c ${GEN_DIR}/bench_probe_service.c)
