# Build: make   (from this folder). Output: bootloader.out
SDK   ?= /Applications/ti/mspm0_sdk_2_11_00_07
SYSCFG ?= /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
TC    ?= /Applications/ti/ti-cgt-armllvm_release
CC     = $(TC)/bin/tiarmclang
GEN    = gen
CFLAGS = -I Inc -I $(GEN) @$(GEN)/device.opt -Os -g -gdwarf-3 -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall \
         -I $(SDK)/source/third_party/CMSIS/Core/Include -I $(SDK)/source
SRCS   = Src/main.c Src/uart.c Src/flash.c Src/xmodem.c $(GEN)/ti_msp_dl_config.c
STARTUP = $(SDK)/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c

all: bootloader.out

$(GEN)/ti_msp_dl_config.c: bootloader.syscfg
	$(SYSCFG) --compiler ticlang --product $(SDK)/.metadata/product.json --output $(GEN) bootloader.syscfg

bootloader.out: $(SRCS) $(STARTUP) linker.cmd Inc/*.h
	$(CC) $(CFLAGS) $(SRCS) $(STARTUP) -Wl,-u,_c_int00 -ldevice.cmd.genlibs -L$(SDK)/source -L$(GEN) linker.cmd \
	  -Wl,-m,bootloader.map -Wl,--rom_model -Wl,--warn_sections -L$(TC)/lib -llibc.a -o $@
	$(TC)/bin/tiarmsize $@ || size $@

clean:
	rm -rf $(GEN) bootloader.out bootloader.map
