# Test application at 0x00010000. make [APP_VER=1|2]; env: MSPM0_SDK_INSTALL_DIR SYSCONFIG_TOOL TICLANG_ARMCOMPILER
MSPM0_SDK_INSTALL_DIR ?= /Applications/ti/mspm0_sdk_2_11_00_07
SYSCONFIG_TOOL ?= /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
TICLANG_ARMCOMPILER ?= /Applications/ti/ti-cgt-armllvm_release
CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(MSPM0_SDK_INSTALL_DIR)/.metadata/product.json
FILES := $(shell $(STUB) --listGeneratedFiles --listReferencedFiles --output . testapp.syscfg)
CFILES = $(filter %.c,$(FILES)) 
OPTS = $(filter %.opt,$(FILES))
APP_VER ?= 1
CFLAGS = -I. $(addprefix @,$(OPTS)) -O2 @device.opt -DAPP_VER=$(APP_VER) "-I$(MSPM0_SDK_INSTALL_DIR)/source/third_party/CMSIS/Core/Include" "-I$(MSPM0_SDK_INSTALL_DIR)/source" -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall
OBJS = main.obj ti_msp_dl_config.obj startup_mspm0g351x_ticlang.obj
all: app.bin
gen: testapp.syscfg
	$(STUB) --output . testapp.syscfg
%.obj: %.c gen
	$(CC) $(CFLAGS) -c $< -o $@
ti_msp_dl_config.obj: gen
startup_mspm0g351x_ticlang.obj: $(MSPM0_SDK_INSTALL_DIR)/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c
	$(CC) $(CFLAGS) -c $< -o $@
app.out: $(OBJS)
	$(CC) -Wl,-u,_c_int00 $(OBJS) -ldevice.cmd.genlibs "-L$(MSPM0_SDK_INSTALL_DIR)/source" app_linker.cmd -Wl,-m,app.map -Wl,--rom_model -Wl,--warn_sections "-L$(TICLANG_ARMCOMPILER)/lib" -llibc.a -o app.out
app.bin: app.out
	"$(TICLANG_ARMCOMPILER)/bin/tiarmobjcopy" -O binary app.out app.bin
clean:
	rm -f *.obj app.out app.bin app.map ti_msp_dl_config.* startup_*.c Event.dot device_linker.cmd Event.dot
