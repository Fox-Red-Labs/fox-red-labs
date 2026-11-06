#
# The host application: ot-ti's CLI app (FreeRTOS entry, OpenThread CLI on the debugger UART) unchanged,
# linked against the host platform library whose radio is the RCP over spinel.
#

set(OT_TI_CLI ${OT_TI}/examples/apps/cli)

add_executable(otbr-host-cc1354
    ${OT_TI_CLI}/cli_uart.cpp
    main.c
    freertos_main.c
    die.c
)

target_include_directories(otbr-host-cc1354 PRIVATE
    ${OT_TI}/openthread/include
    ${OT_TI}/openthread/examples/platforms
    ${OT_TI}/openthread/src
    ${OT_TI}/openthread/src/core
    ${OT_TI}/src
)

# Static libraries, so the platform and the spinel libraries appear more than once to settle every reference.
target_link_libraries(otbr-host-cc1354 PRIVATE
    openthread-cli-ftd
    ${OT_PLATFORM_LIB}
    openthread-ftd
    ${OT_PLATFORM_LIB}
    openthread-radio-spinel
    openthread-spinel-rcp
    openthread-hdlc
    openthread-ftd
    ${OT_PLATFORM_LIB}
    openthread-cli-ftd
    ti-mbedtls
    ot-config-ftd
    ot-config
)

set_target_properties(otbr-host-cc1354 PROPERTIES SUFFIX .out)
