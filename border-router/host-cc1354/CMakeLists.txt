#
# Thread border-router host on the LP-EM-CC1354P10-1 (milestone M1 of ../README.md).
#
# OpenThread full Thread device whose radio is a TI RCP (CC2340R5, ot-rcp image) on a UART, built on TI's own
# OpenThread port (ot-ti) for everything but the radio. Build with ./build.sh <dir>; see ../README.md.
#

cmake_minimum_required(VERSION 3.16)
project(otbr-host-cc1354 VERSION 1.4.0.0)

set(OT_TI "$ENV{HOME}/ot-ti" CACHE PATH "ot-ti checkout (TI's OpenThread port, release thread-v1.4-ti-2.1) with its openthread submodule")
set(TI_SIMPLELINK_BOARD "LP_EM_CC1354P10_1" CACHE STRING "")
set(TI_SIMPLELINK_KERNEL "freertos" CACHE STRING "")
set(TI_SRC_DIR ${OT_TI}/src CACHE PATH "ot-ti platform sources (linker script, FreeRTOS config)")

include(${OT_TI}/boards.cmake)

set(OT_PACKAGE_VERSION "1.4.0.0" CACHE STRING "OpenThread version" FORCE)
set(OT_PLATFORM_LIB "openthread-host-cc1354")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

set(FREERTOS_CONFIG_DIR ${PROJECT_SOURCE_DIR}/platform)   # ot-ti's FreeRTOSConfig.h with a talking configASSERT

# Same OpenThread switches as ot-ti's top level
set(OT_EXTERNAL_MBEDTLS "ti-mbedtls" CACHE STRING "Use TI hardware accelerated mbedtls" FORCE)
set(OT_BUILTIN_MBEDTLS_MANAGEMENT ON CACHE BOOL "Have OpenThread manage mbedtls")
set(OT_BUILD_EXECUTABLES OFF CACHE BOOL "Build OpenThread executables")
set(BUILD_TESTING OFF CACHE BOOL "Build test executables")

# This project's own
set(OT_PLATFORM "external" CACHE STRING "")
set(OT_FTD ON CACHE BOOL "")
set(OT_MTD OFF CACHE BOOL "")
set(OT_RCP OFF CACHE BOOL "")
set(OT_APP_CLI OFF CACHE BOOL "")
set(OT_APP_NCP OFF CACHE BOOL "")
set(OT_APP_RCP OFF CACHE BOOL "")
set(OT_PING_SENDER ON CACHE BOOL "the config header turns it on; OpenThread would otherwise derive OFF from OT_APP_CLI and redefine it")
set(OT_LIB_CONFIG "openthread-host-lib-config.h" CACHE STRING "spinel library config (frame buffer sizes)")
set(OT_COMPILE_WARNING_AS_ERROR OFF CACHE BOOL "")

add_subdirectory(${OT_TI}/openthread openthread)
add_subdirectory(${OT_TI}/third_party/freertos freertos)
add_subdirectory(${OT_TI}/third_party/ti_simplelink_sdk ti_simplelink_sdk)
add_subdirectory(${OT_TI}/third_party/mbedtls mbedtls)
add_subdirectory(platform)
add_subdirectory(app)

set(OPENTHREAD_CONFIG_FILE "openthread-core-host-cc1354-config.h" CACHE STRING "")
set(OPENTHREAD_PROJECT_CORE_CONFIG_FILE "openthread-core-host-cc1354-config.h" CACHE STRING "")
set(OPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE "openthread-core-cc13xx_cc26xx-config-check.h" CACHE STRING "")

target_compile_definitions(ot-config INTERFACE
    "NCP_BUILD_ENABLED=0"
    "OPENTHREAD_CONFIG_FILE=\"${OPENTHREAD_CONFIG_FILE}\""
    "OPENTHREAD_PROJECT_CORE_CONFIG_FILE=\"${OPENTHREAD_PROJECT_CORE_CONFIG_FILE}\""
    "OPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE=\"${OPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE}\""
)

# Our config and platform headers first, then ot-ti's (FreeRTOSConfig.h, system.h, the check file)
target_include_directories(ot-config INTERFACE
    ${PROJECT_SOURCE_DIR}/platform
    ${OT_TI}/src
)
