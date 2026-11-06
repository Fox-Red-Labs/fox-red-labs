# Tool tag on Zephyr. The codec and the model are the same files the TI BLE5-Stack build uses.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tool_tag)

target_sources(app PRIVATE
  src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/tt_proto.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../ai/out/tt_model.c)
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto
  ${CMAKE_CURRENT_SOURCE_DIR}/../ai/out)
