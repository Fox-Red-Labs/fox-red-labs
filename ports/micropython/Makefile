# MicroPython for the TI MSPM0G3519 and MSPM0G3507 (Cortex-M0+).
#   make BOARD=g3519 VARIANT=full      the full build (512 kB flash, 128 kB SRAM)
#   make BOARD=g3519 VARIANT=min       minimal build, no float
#   make BOARD=g3507 VARIANT=min       the same minimal build on the 128 kB flash, 32 kB SRAM device
#   make BOARD=g5187 VARIANT=min       the same on the MSPM0G5187 (UART block named UC0)
# Tools: arm-none-eabi-gcc (any recent GNU Arm Embedded; newlib-nano), the MSPM0 SDK for
# DriverLib and the startup file, Python 3 for the MicroPython build scripts.
# Version text on the REPL banner (otherwise the build reads the git hash of this repository).
export MICROPY_GIT_TAG ?= v1.29.0-frl1
export MICROPY_GIT_HASH ?= 0fd6c573e
BOARD ?= g3519
VARIANT ?= full
MSPM0_SDK ?= /Applications/ti/mspm0_sdk_2_11_00_07
TOP = ../../third-party/micropython
BUILD = build-$(BOARD)-$(VARIANT)

ifeq ($(BOARD),g3519)
  DEVICE := __MSPM0G3519__
  DRVLIB_DIR := $(MSPM0_SDK)/source/ti/driverlib/lib/gcc/m0p/mspm0gx51x
  STARTUP := $(MSPM0_SDK)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g351x_gcc.c
  FLASH_LEN ?= 0x80000
  RAM_LEN ?= 0x20000
  BOARD_DEFS := -DFRL_G3519 -DFRL_BOARD_NAME='"LP-MSPM0G3519"' -DFRL_MCU_NAME='"MSPM0G3519"'
else ifeq ($(BOARD),g3507)
  DEVICE := __MSPM0G3507__
  DRVLIB_DIR := $(MSPM0_SDK)/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x
  STARTUP := $(MSPM0_SDK)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c
  FLASH_LEN ?= 0x20000
  RAM_LEN ?= 0x8000
  BOARD_DEFS := -DFRL_G3507 -DFRL_BOARD_NAME='"LP-MSPM0G3507"' -DFRL_MCU_NAME='"MSPM0G3507"'
else ifeq ($(BOARD),g5187)
  DEVICE := __MSPM0G5187__
  DRVLIB_DIR := $(MSPM0_SDK)/source/ti/driverlib/lib/gcc/m0p/mspm0g518x
  STARTUP := $(MSPM0_SDK)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g518x_gcc.c
  FLASH_LEN ?= 0x20000
  RAM_LEN ?= 0x8000
  BOARD_DEFS := -DFRL_G5187 -DFRL_BOARD_NAME='"LP-MSPM0G5187"' -DFRL_MCU_NAME='"MSPM0G5187"'
else ifeq ($(BOARD),l1306)
  DEVICE := __MSPM0L1306__
  DRVLIB_DIR := $(MSPM0_SDK)/source/ti/driverlib/lib/gcc/m0p/mspm0l11xx_l13xx
  STARTUP := $(MSPM0_SDK)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0l130x_gcc.c
  FLASH_LEN ?= 0x10000
  RAM_LEN ?= 0x1000
  BOARD_DEFS := -DFRL_L1306 -DFRL_NO_PLL -DFRL_BOARD_NAME='"LP-MSPM0L1306"' -DFRL_MCU_NAME='"MSPM0L1306"'
else
  $(error BOARD must be g3519, g3507, g5187 or l1306)
endif

ifeq ($(VARIANT),min)
  STACK_LEN ?= 0x600
  VARIANT_DEFS := -DFRL_MIN
else
  STACK_LEN ?= 0x1000
  VARIANT_DEFS :=
endif

# HEAP_MAX=<bytes> caps the garbage-collected heap (default: all the RAM left over).
ifdef HEAP_MAX
  BOARD_DEFS += -DFRL_HEAP_MAX=$(HEAP_MAX)
endif

# AUTORUN='<python text with \n escapes>' runs that text once after start-up (LED check; off by default).
ifdef AUTORUN
  BOARD_DEFS += -DFRL_AUTORUN='"$(AUTORUN)"'
endif

QSTR_DEFS = qstrdefsport.h
MICROPY_ROM_TEXT_COMPRESSION ?= 1
CROSS_COMPILE ?= arm-none-eabi-

include $(TOP)/py/mkenv.mk
include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

INC += -I. -I$(TOP) -I$(BUILD) -I$(MSPM0_SDK)/source -I$(MSPM0_SDK)/source/third_party/CMSIS/Core/Include
CFLAGS_CORTEX_M0 := -mthumb -mcpu=cortex-m0plus -msoft-float -fsingle-precision-constant -Wdouble-promotion
CFLAGS += $(INC) -Wall -Werror -Wno-error=unused-function -std=c99 $(CFLAGS_CORTEX_M0) -D$(DEVICE) \
  $(BOARD_DEFS) $(VARIANT_DEFS) -Os -DNDEBUG -fdata-sections -ffunction-sections -g
LDFLAGS += $(CFLAGS_CORTEX_M0) --specs=nano.specs --specs=nosys.specs -nostartfiles \
  -Wl,-T,mspm0.ld -Wl,--gc-sections -Wl,-Map=$@.map,--cref \
  -Wl,--defsym=FRL_FLASH_LEN=$(FLASH_LEN) -Wl,--defsym=FRL_RAM_LEN=$(RAM_LEN) \
  -Wl,--defsym=FRL_STACK_LEN=$(STACK_LEN) -L$(DRVLIB_DIR)
LIBS := -l:driverlib.a -lm -lc -lgcc

SRC_C = \
	main.c \
	mphalport.c \
	machine_pin.c \
	machine_spi.c \
	shared/libc/printf.c \
	shared/readline/readline.c \
	shared/runtime/gchelper_native.c \
	shared/runtime/interrupt_char.c \
	shared/runtime/pyexec.c \
	shared/runtime/stdout_helpers.c \
	shared/runtime/sys_stdio_mphal.c \
	drivers/bus/softspi.c

SRC_S = shared/runtime/gchelper_thumb1.s

SRC_QSTR += $(SRC_C)

OBJ += $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_S:.s=.o))
OBJ += $(BUILD)/startup.o

all: $(BUILD)/firmware.elf $(BUILD)/firmware.bin

$(BUILD)/startup.o: $(STARTUP)
	$(ECHO) "CC $<"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/firmware.elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O binary $< $@

include $(TOP)/py/mkrules.mk
