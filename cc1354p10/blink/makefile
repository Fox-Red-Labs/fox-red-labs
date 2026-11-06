# Blink for the LP-CC1354P10-1, TI Arm Clang. Adapted from the SDK's examples/nortos/LP_EM_CC1354P10_1/drivers/empty/ticlang/makefile (F2 SDK 8.33).
# Run from a build folder one level below this file:  make -C build -f ../makefile

SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR ?= $(HOME)/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16

include $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/imports.mak

OPT ?= -Oz
ITERATIONS ?= 2000

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"

SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output . ../blink.syscfg)

SYSCFG_C_FILES = $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES = $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

V := @
ifeq ($(VERBOSE), 1)
  V :=
endif

OBJECTS = blink.obj main_nortos.obj $(patsubst %.c,%.obj,$(notdir $(SYSCFG_C_FILES)))

NAME = blink

DFLAGS += -gdwarf-3

ARCHFLAGS = -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -mthumb

CFLAGS += -I.. \
    -I. \
    $(addprefix @,$(SYSCFG_OPT_FILES)) \
    $(OPT) \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/kernel/nortos" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/kernel/nortos/posix" \
    $(ARCHFLAGS) \
    -Wall

ifneq ($(CONFIG),RELEASE)
CFLAGS += $(DFLAGS)
endif

LFLAGS += "-L$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source" \
    "-L$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/kernel/nortos" \
    "-Wl,--search_path=." \
    ti_utils_build_linker.cmd.genlibs \
    ../cc13x4_cc26x4_nortos.cmd \
    "-Wl,-m,$(NAME).map" \
    -Wl,--rom_model \
    -Wl,--warn_sections \
    "-L$(TICLANG_ARMCOMPILER)/lib" \
    -llibc.a

all: postbuild

.PHONY: postbuild
postbuild: $(NAME).out
	"$(TICLANG_ARMCOMPILER)/bin/tiarmobjcopy" -O ihex "./$(NAME).out" "./$(NAME).hex"

.INTERMEDIATE: syscfg
$(SYSCFG_FILES): syscfg
	@ echo generation complete

syscfg: ../blink.syscfg
	@ echo Generating configuration files...
	$(V) $(SYSCFG_CMD_STUB) --output $(@D) $<

define C_RULE
$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))

blink.obj: ../blink.c $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

main_nortos.obj: ../main_nortos.c $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(NAME).out: $(OBJECTS)
	@ echo linking $@
	$(V) $(LNK) -Wl,-u,_c_int00 $(OBJECTS)  $(LFLAGS) -o $(NAME).out

clean:
	$(V) $(RM) *.obj *.d $(NAME).out $(NAME).map $(NAME).hex $(SYSCFG_FILES)
