# SPDX-License-Identifier: Apache-2.0
# Builds one unchanged Arduino sketch (.ino) on Zephyr with the ArduinoCore-zephyr module.
#   -DSKETCH_DIR=<folder holding Name/Name.ino>  -DSKETCH=<Name>  -DDTC_OVERLAY_FILE=<variant overlay>
#   -DZEPHYR_EXTRA_MODULES=<ArduinoCore-zephyr checkout>
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(arduino_${SKETCH})

# The sketch is included as is. The Arduino IDE adds #include <Arduino.h> and function prototypes; the
# prototypes are not needed because setup() and loop() are defined before use in these sketches.
file(WRITE ${CMAKE_BINARY_DIR}/sketch_main.cpp
  "#include <Arduino.h>\n#include \"${SKETCH_DIR}/${SKETCH}/${SKETCH}.ino\"\n")
target_sources(app PRIVATE ${CMAKE_BINARY_DIR}/sketch_main.cpp)
zephyr_compile_options(-Wno-unused-variable -Wno-comment)
