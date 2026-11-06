# Build: make -C port   (tools picked up from the environment, see build.sh)
# UART_BAUD is the one place the console baud rate is set.
UART_BAUD ?= 115200
MSPM0_SDK_INSTALL_DIR ?= /Applications/ti/mspm0_sdk_2_11_00_07
SYSCONFIG_TOOL ?= /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
TICLANG_ARMCOMPILER ?= /Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS

UP = ../../common/lwrb
UPX =
LWRB = $(UP)/src
CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
SYSCFG_CMD = $(SYSCONFIG_TOOL) --compiler ticlang --product $(MSPM0_SDK_INSTALL_DIR)/.metadata/product.json
NAME = usart_dma_g5187

CFLAGS = -I. -Ibuild -I$(LWRB)/include -O2 -DUART_BAUD=$(UART_BAUD) @build/device.opt \
  "-I$(MSPM0_SDK_INSTALL_DIR)/source/third_party/CMSIS/Core/Include" "-I$(MSPM0_SDK_INSTALL_DIR)/source" \
  -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -gdwarf-3
OBJ = build/main.obj build/ti_msp_dl_config.obj build/startup_mspm0g518x_ticlang.obj build/lwrb.obj

all: build/$(NAME).out

build/ti_msp_dl_config.c build/ti_msp_dl_config.h build/device.opt: $(NAME).syscfg
	mkdir -p build && $(SYSCFG_CMD) --output build $<
build/main.obj: main.c build/ti_msp_dl_config.h
	$(CC) $(CFLAGS) -c $< -o $@
build/ti_msp_dl_config.obj: build/ti_msp_dl_config.c
	$(CC) $(CFLAGS) -c $< -o $@
build/startup_mspm0g518x_ticlang.obj: $(MSPM0_SDK_INSTALL_DIR)/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g518x_ticlang.c build/device.opt
	$(CC) $(CFLAGS) -c $< -o $@
build/lwrb.obj: $(LWRB)/lwrb/lwrb.c
	$(CC) $(CFLAGS) -c $< -o $@
build/$(NAME).out: $(OBJ) device_linker.cmd
	$(CC) -Wl,-u,_c_int00 $(OBJ) -Lbuild -ldevice.cmd.genlibs "-L$(MSPM0_SDK_INSTALL_DIR)/source" device_linker.cmd \
	  "-Wl,-m,build/$(NAME).map" -Wl,--rom_model -Wl,--warn_sections "-L$(TICLANG_ARMCOMPILER)/lib" -llibc.a -o $@
clean:
	rm -rf build
