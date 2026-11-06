# Build for LP-MSPM0G3519 with TI Arm Clang, SysConfig CLI and gmake from CCStudio.
#   PATH=/Applications/ti/ccs2101/ccs/utils/bin:$PATH gmake            (USART_BAUD defaults to 115200)
#   gmake USART_BAUD=230400
SDK      ?= /Applications/ti/mspm0_sdk_2_11_00_07
TICLANG  ?= /Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG   ?= /Applications/ti/ccs2101/ccs/utils/sysconfig_1.28.1/sysconfig_cli.sh
LWRB     ?= $(abspath ../../common/lwrb/src)
USART_BAUD ?= 115200

CC  = "$(TICLANG)/bin/tiarmclang"
BLD = build
SYSCFG_STUB = $(SYSCFG) --compiler ticlang --product $(SDK)/.metadata/product.json

SYSCFG_OUT = $(BLD)/ti_msp_dl_config.c $(BLD)/ti_msp_dl_config.h $(BLD)/device.opt \
             $(BLD)/device.cmd.genlibs $(BLD)/device_linker.cmd

STARTUP = $(SDK)/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c
OBJECTS = $(BLD)/main.obj $(BLD)/ti_msp_dl_config.obj $(BLD)/startup.obj $(BLD)/lwrb.obj

CFLAGS = -I. -I$(BLD) -I$(LWRB)/include \
    @$(BLD)/device.opt \
    "-I$(SDK)/source/third_party/CMSIS/Core/Include" "-I$(SDK)/source" \
    -DUSART_BAUD=$(USART_BAUD)U \
    -O2 -g -gdwarf-3 -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall

LFLAGS = -ldevice.cmd.genlibs "-L$(SDK)/source" -L$(BLD) $(BLD)/device_linker.cmd \
    "-Wl,-m,$(BLD)/usart_dma.map" -Wl,--rom_model -Wl,--warn_sections \
    "-L$(TICLANG)/lib" -llibc.a

all: $(BLD)/usart_dma.out
	@ "$(TICLANG)/bin/tiarmsize" $<

$(SYSCFG_OUT) &: usart_dma.syscfg
	@ mkdir -p $(BLD)
	$(SYSCFG_STUB) --output $(BLD) $<

$(BLD)/main.obj: main.c $(SYSCFG_OUT)
	$(CC) $(CFLAGS) -c $< -o $@
$(BLD)/ti_msp_dl_config.obj: $(BLD)/ti_msp_dl_config.c
	$(CC) $(CFLAGS) -c $< -o $@
$(BLD)/startup.obj: $(STARTUP) $(SYSCFG_OUT)
	$(CC) $(CFLAGS) -c $< -o $@
$(BLD)/lwrb.obj: $(LWRB)/lwrb/lwrb.c $(SYSCFG_OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD)/usart_dma.out: $(OBJECTS)
	$(CC) -Wl,-u,_c_int00 $(OBJECTS) $(LFLAGS) -o $@

clean:
	rm -rf $(BLD)
.PHONY: all clean
