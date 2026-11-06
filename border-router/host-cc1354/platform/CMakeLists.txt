#
# OpenThread platform library for the border-router host on the LP-EM-CC1354P10-1.
#
# ot-ti's platform (alarm, settings, entropy, UART console, process loop) with its on-chip radio replaced by
# OpenThread's RadioSpinel talking to a TI RCP over a second UART. Mirrors ot-ti/src/CMakeLists.txt for the
# CC13x4 case, minus radio.c, diag.c and the NCP SPI peripheral.
#

if(NOT $ENV{TI_SYSCONFIG_ROOT} STREQUAL "")
    set(TI_SYSCONFIG_INSTALL_DIR $ENV{TI_SYSCONFIG_ROOT} CACHE PATH "")
else()
    set(TI_SYSCONFIG_INSTALL_DIR /Applications/ti/sysconfig_1.26.3 CACHE PATH "")
endif()
set(TI_SYSCONFIG_CMD ${TI_SYSCONFIG_INSTALL_DIR}/sysconfig_cli.sh CACHE PATH "")

set(SYSCFG_DIR ${CMAKE_CURRENT_BINARY_DIR}/syscfg)

# No RF module in the .syscfg, so no ti_radio_config.c here.
set(SYSCONFIG_OUTPUT_C
    ${SYSCFG_DIR}/ti_devices_config.c
    ${SYSCFG_DIR}/ti_devices_config.h
    ${SYSCFG_DIR}/ti_drivers_config.c
    ${SYSCFG_DIR}/ti_drivers_config.h
    ${SYSCFG_DIR}/ti_log_config.c
)
set(SYSCONFIG_OUTPUT_OTHER
    ${SYSCFG_DIR}/syscfg_c.rov.xs
    ${SYSCFG_DIR}/ti_build_config.opt
    ${SYSCFG_DIR}/ti_utils_build_linker.cmd.genlibs
    ${SYSCFG_DIR}/ti_utils_build_linker.cmd.genmap
    ${SYSCFG_DIR}/ti_utils_runtime_Makefile
    ${SYSCFG_DIR}/ti_utils_runtime_model.gv
)

# ot-ti creates these empty so a build with logging off still has them.
file(APPEND ${SYSCFG_DIR}/ti_log_config.c "")
file(APPEND ${SYSCFG_DIR}/ti_log_config.h "")

add_custom_command(
    OUTPUT ${SYSCONFIG_OUTPUT_C} ${SYSCONFIG_OUTPUT_OTHER}
    COMMAND ${TI_SYSCONFIG_CMD}
        -s ${TI_SIMPLELINK_SDK_DIR}/.metadata/product.json
        -o ${SYSCFG_DIR}
        --board /ti/boards/${TI_SIMPLELINK_BOARD}
        --compiler gcc
        --rtos ${TI_SIMPLELINK_KERNEL}
        ${CMAKE_CURRENT_SOURCE_DIR}/openthread.syscfg
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openthread.syscfg
    VERBATIM
)
add_custom_target(build_syscfg DEPENDS ${SYSCONFIG_OUTPUT_C})

set(OT_TI_SRC ${OT_TI}/src)

add_library(${OT_PLATFORM_LIB}
    ${SYSCONFIG_OUTPUT_C}
    ${OT_TI_SRC}/alarm.c
    ${OT_TI_SRC}/alarm_micro.c
    ${OT_TI_SRC}/entropy.c
    ${OT_TI_SRC}/infra_if.c
    ${OT_TI_SRC}/misc.c
    ${OT_TI_SRC}/settings.c
    ${OT_TI_SRC}/system.c
    console_uart.c
    spinel_uart.cpp
    radio_spinel_glue.cpp
    $<TARGET_OBJECTS:openthread-platform-utils>
)
add_dependencies(${OT_PLATFORM_LIB} build_syscfg)

target_link_libraries(${OT_PLATFORM_LIB}
    PUBLIC
        cc13xx-cc26xx-sdk
        openthread-radio-spinel
        openthread-spinel-rcp
        openthread-hdlc
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_PROPERTY:NAME>.map
        -L${TI_SIMPLELINK_SDK_DIR}/source
        -L${SYSCFG_DIR}
        ${SYSCFG_DIR}/ti_utils_build_linker.cmd.genlibs
        cc13xx-cc26xx-driverlib
    PRIVATE
        ot-config
)

target_compile_definitions(${OT_PLATFORM_LIB}
    PUBLIC
        OT_TI_KERNEL_${TI_SIMPLELINK_KERNEL}
    PRIVATE
        # Same view of the core as the spinel library itself, so RadioSpinel has one layout everywhere.
        OPENTHREAD_FTD=0
        OPENTHREAD_MTD=0
        OPENTHREAD_RADIO=0
)

target_compile_options(${OT_PLATFORM_LIB}
    PUBLIC
        @${SYSCFG_DIR}/ti_utils_build_compiler.opt
)

target_include_directories(${OT_PLATFORM_LIB}
    PUBLIC
        ${SYSCFG_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OT_TI_SRC}
    PRIVATE
        ${OT_TI}/openthread/include
        ${OT_TI}/openthread/src
        ${OT_TI}/openthread/src/core
        ${OT_TI}/openthread/examples/platforms
)
