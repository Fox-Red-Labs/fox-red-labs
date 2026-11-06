# Chained demo for the LP-EM-CC35X1: TI mqtt_client + ble_wifi_provisioning + ota_example in one image.
# Based on TI's examples/rtos/LP_EM_CC35X1/demos/mqtt_client/freertos/ticlang/makefile (SimpleLink Wi-Fi SDK
# 10.20.00.39). Run from an empty build folder with -f (build.sh does this); SysConfig writes its files there.
# TI sources are compiled in place from the SDK; only the files in this folder are ours or changed copies.
#
# Differences from TI's makefile:
#   - OBJECTS: mqtt_client_app.obj replaced by chained_app.obj; wifi_if.obj built from the changed copy here;
#     added nimble_host_provisioning.obj (TI, unmodified), ota_server/ota_fwu/https_client/network_mbedtls.obj
#     (TI, unmodified), prov_glue/ota_glue/cred_nv.obj (ours), cred_record/cmd_parse.obj (portability/iot-chain)
#   - CFLAGS: the FWU include paths and -DTHREADSTACKSIZE=10752 from TI's ota_example makefile (TLS runs on the
#     main thread); include paths for this folder, portability/iot-chain/core and the config header
#   - the syscfg is TI's mqtt_client.syscfg, unmodified; TI's linker.cmd is copied into the build folder unmodified

SIMPLELINK_WIFI_SDK_INSTALL_DIR ?= /Applications/ti/simplelink_wifi_sdk_10_20_00_39
HERE   ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO   ?= $(abspath $(HERE)/../..)
CONFIG_DIR ?= $(HERE)/config-example

include $(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/imports.mak

DEMOS = $(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/examples/rtos/LP_EM_CC35X1/demos
MQTT  = $(DEMOS)/mqtt_client
PROV  = $(DEMOS)/ble_wifi_provisioning
OTA   = $(DEMOS)/ota_example
CORE  = $(REPO)/portability/iot-chain/core
SYSCFG_SRC = $(MQTT)/freertos/mqtt_client.syscfg

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"

SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/.metadata/product.json --product $(SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR)/.metadata/product.json
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output . $(SYSCFG_SRC))

SYSCFG_C_FILES = $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES = $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

V := @
ifeq ($(VERBOSE), 1)
  V :=
endif

# TI mqtt_client objects (unmodified sources, except wifi_if.c which is the copy here)
OBJ_MQTT = button_if.obj dns_if.obj led_if.obj mqtt_if.obj tcpip_if.obj wifi_if.obj crc.obj nvocmp_cc35xx.obj \
    adaptation_osi_dpl.obj adaptation_osi_filesystem.obj adaptation_uart_term.obj adaptation_syslog.obj \
    freertos_main_freertos.obj
# TI ble_wifi_provisioning: the GATT provisioning service (unmodified)
OBJ_PROV = nimble_host_provisioning.obj
# TI ota_example: server query, download, PSA FWU slot handling, mbedTLS threading (unmodified)
OBJ_OTA  = ota_server.obj ota_fwu.obj https_client.obj network_mbedtls.obj
# Ours
OBJ_FRL  = chained_app.obj prov_glue.obj ota_glue.obj cred_nv.obj cred_record.obj cmd_parse.obj

OBJECTS = $(OBJ_MQTT) $(OBJ_PROV) $(OBJ_OTA) $(OBJ_FRL) $(patsubst %.c,%.obj,$(notdir $(SYSCFG_C_FILES)))

NAME = chained_demo

DFLAGS += -gdwarf-3

# Include order: this folder (changed copies, glue), the config header, the core layer, then TI's three demo
# folders. A quoted #include looks in the including file's own folder first, so each TI source still sees its own
# demo's headers.
CFLAGS += -I$(HERE) \
    -I$(CONFIG_DIR) \
    -I$(CORE) \
    -I$(MQTT) \
    -I$(OTA) \
    -I$(PROV) \
    -I. \
    $(addprefix @,$(SYSCFG_OPT_FILES)) \
    -DMBEDTLS_CONFIG_FILE=\"config-hsm.h\" \
    -DNVOCMP_SPS_USE_CBC \
    -DMBEDTLS_PSA_CRYPTO_CONFIG_FILE=\"config-psa-crypto-hsm.h\" \
    -Ofast \
    -DCC35XX \
    -DDeviceFamily_CC35XX \
    -DNVOCMP_POSIX_MUTEX \
    -DTHREADSTACKSIZE=10752 \
    $(if $(CHAINED_FW_VERSION),-DCHAINED_FW_VERSION=\"$(CHAINED_FW_VERSION)\") \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/utils/FWU" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/utils/FWU/headers" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/utils/FWU/headers/fwu/1.0/psa" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/mbedtls/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/mbedtls/ti/configs" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/mbedtls/ti/port" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/kernel/freertos" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/posix/ticlang" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/freertos/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/freertos/portable/GCC/ARM_CM33_NTZ/non_secure" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/net/ble_interface/inc_adapt" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/net/ble_interface/inc_common" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/drivers/net/wifi/wifi_platform/cc35xx/inc_common" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/net/wifi_stack/inc_common" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/drivers/net/wifi/wifi_host_driver/inc_common" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/drivers/net/wifi/wifi_host_driver/inc_adapt" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/tools/TI_Containers" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/lwip/lwip-stack/src/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/lwip/ti_config/lwip-port/osi/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/lwip/lwip-contrib" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/ti_config/nimble-port/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/ti_config/nimble-port/porting/npl/osi/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/host/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/host/services/gap/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/host/services/gatt/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/host/services/dis/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/host/util/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/nimble/transport/include" \
    "-I$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/nimble-src/porting/nimble/include" \
    -mcpu=cortex-m33 \
    -mfloat-abi=hard \
    -mfpu=fpv5-sp-d16 \
    -Wall

ifneq ($(CONFIG),RELEASE)
CFLAGS += $(DFLAGS)
endif

LFLAGS += "-L$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source" \
    -Wl,--reread_libs \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/lwip/lib/ticlang/lwip.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/hostap/lib/ticlang/hostap.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/third_party/nimble/lib/ticlang/nimble.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/net/wifi_stack/lib/ticlang/wifi_stack.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/net/ble_interface/lib/ticlang/ble_interface.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/drivers/net/wifi/wifi_host_driver/lib/ticlang/wifi_host_driver.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/drivers/net/wifi/wifi_platform/cc35xx/lib/ticlang/wifi_platform_cc35xx.a" \
    "-Wl,-l$(SIMPLELINK_WIFI_SDK_INSTALL_DIR)/source/ti/utils/FWU/lib/ticlang/FWU.a" \
    ti_utils_build_linker.cmd.genlibs \
    linker.cmd \
    "-Wl,-m,$(NAME).map" \
    -Wl,--rom_model \
    -Wl,--warn_sections \
    "-L$(TICLANG_ARMCOMPILER)/lib" \
    -llibc.a

all: $(NAME).out

.INTERMEDIATE: syscfg
$(SYSCFG_FILES): syscfg
	@ echo generation complete

syscfg: $(SYSCFG_SRC)
	@ echo Generating configuration files...
	$(V) $(SYSCFG_CMD_STUB) --output $(@D) $<

define C_RULE
$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))

# name:source pairs; one compile rule each
SRCS = button_if:$(MQTT)/button_if.c dns_if:$(MQTT)/dns_if.c led_if:$(MQTT)/led_if.c mqtt_if:$(MQTT)/mqtt_if.c \
    tcpip_if:$(MQTT)/tcpip_if.c wifi_if:$(HERE)/wifi_if.c crc:$(MQTT)/crc.c nvocmp_cc35xx:$(MQTT)/nvocmp_cc35xx.c \
    adaptation_osi_dpl:$(MQTT)/adaptation/osi_dpl.c adaptation_osi_filesystem:$(MQTT)/adaptation/osi_filesystem.c \
    adaptation_uart_term:$(MQTT)/adaptation/uart_term.c adaptation_syslog:$(MQTT)/adaptation/syslog.c \
    freertos_main_freertos:$(MQTT)/freertos/main_freertos.c \
    nimble_host_provisioning:$(PROV)/nimble_host_provisioning.c \
    ota_server:$(OTA)/ota_server.c ota_fwu:$(OTA)/ota_fwu.c https_client:$(OTA)/https_client.c \
    network_mbedtls:$(OTA)/network_mbedtls.c \
    chained_app:$(HERE)/chained_app.c prov_glue:$(HERE)/prov_glue.c ota_glue:$(HERE)/ota_glue.c \
    cred_nv:$(HERE)/cred_nv.c cred_record:$(CORE)/cred_record.c cmd_parse:$(CORE)/cmd_parse.c

define SRC_RULE
$(word 1,$(subst :, ,$(1))).obj: $(word 2,$(subst :, ,$(1))) $(SYSCFG_H_FILES)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach s,$(SRCS),$(eval $(call SRC_RULE,$(s))))

chained_app.obj: $(CONFIG_DIR)/chained_config.h

# TI's linker.cmd, unmodified, copied next to the generated ti_build_linker.cmd.toolbox it #includes (in TI's
# layout the build folder is the folder linker.cmd lives in)
linker.cmd: $(MQTT)/freertos/ticlang/linker.cmd
	cp $< $@

$(NAME).out: $(OBJECTS) linker.cmd
	@ echo linking $@
	$(V) $(LNK) -Wl,-u,_c_int00 $(OBJECTS)  $(LFLAGS) -o $(NAME).out

clean:
	@ echo Cleaning...
	$(V) $(RM) $(OBJECTS) > $(DEVNULL) 2>&1
	$(V) $(RM) $(NAME).out > $(DEVNULL) 2>&1
	$(V) $(RM) $(NAME).map > $(DEVNULL) 2>&1
