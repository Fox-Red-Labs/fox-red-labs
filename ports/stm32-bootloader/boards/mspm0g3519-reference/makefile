# Bootloader for the LP-MSPM0G3519. env: MSPM0_SDK_INSTALL_DIR SYSCONFIG_TOOL TICLANG_ARMCOMPILER (defaults below)
MSPM0_SDK_INSTALL_DIR ?= /Applications/ti/mspm0_sdk_2_11_00_07
SYSCONFIG_TOOL ?= /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
TICLANG_ARMCOMPILER ?= /Applications/ti/ti-cgt-armllvm_release
CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(MSPM0_SDK_INSTALL_DIR)/.metadata/product.json
FILES := $(shell $(STUB) --listGeneratedFiles --listReferencedFiles --output . bootloader.syscfg)
OPTS = $(filter %.opt,$(FILES))
CFLAGS = -I. -I../../common $(addprefix @,$(OPTS)) -O2 @device.opt "-I$(MSPM0_SDK_INSTALL_DIR)/source/third_party/CMSIS/Core/Include" "-I$(MSPM0_SDK_INSTALL_DIR)/source" -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall
STARTUP = $(MSPM0_SDK_INSTALL_DIR)/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c
OBJS = main.obj uart.obj flash.obj xmodem.obj ti_msp_dl_config.obj startup.obj
all: bootloader.out
gen: bootloader.syscfg
	$(STUB) --output . bootloader.syscfg
ti_msp_dl_config.obj main.obj uart.obj flash.obj: gen
%.obj: %.c
	$(CC) $(CFLAGS) -c $< -o $@
xmodem.obj: ../../common/xmodem.c
	$(CC) $(CFLAGS) -c $< -o $@
startup.obj: $(STARTUP)
	$(CC) $(CFLAGS) -c $< -o $@
bootloader.out: $(OBJS)
	$(CC) -Wl,-u,_c_int00 $(OBJS) -ldevice.cmd.genlibs "-L$(MSPM0_SDK_INSTALL_DIR)/source" boot_linker.cmd -Wl,-m,bootloader.map -Wl,--rom_model -Wl,--warn_sections "-L$(TICLANG_ARMCOMPILER)/lib" -llibc.a -o bootloader.out
clean:
	rm -f *.obj bootloader.out bootloader.map ti_msp_dl_config.* Event.dot device_linker.cmd
