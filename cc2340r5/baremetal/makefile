# Bare-metal benchmark for the LP-EM-CC2340R5, TI Arm Clang.
# Adapted from the SDK's examples/nortos/LP_EM_CC2340R5/drivers/empty/ticlang/makefile.
# Differences from that makefile: more source files (benchmark + EEMBC CoreMark), the
# optimization level comes from OPT (default -Oz, the SDK's own setting), and it is run from a
# build folder one level below this file:  make -C build-O3 -f ../makefile OPT=-O3
# Clock, power and CCFG all come from SysConfig defaults (bench.syscfg = empty.syscfg + UART2).

SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR ?= /Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS

include $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/imports.mak

OPT ?= -Oz
ITERATIONS ?= 2000

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"

SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output . ../bench.syscfg)

SYSCFG_C_FILES = $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES = $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

V := @
ifeq ($(VERBOSE), 1)
  V :=
endif

CM_OBJECTS = core_list_join.obj core_main.obj core_matrix.obj core_state.obj core_util.obj core_portme.obj
OBJECTS = bench.obj main_nortos.obj $(CM_OBJECTS) $(patsubst %.c,%.obj,$(notdir $(SYSCFG_C_FILES)))

NAME = bench

DFLAGS += -gdwarf-3

ARCHFLAGS = -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb

CFLAGS += -I.. \
    -I../coremark \
    -I../../../third-party/coremark \
    -I. \
    $(addprefix @,$(SYSCFG_OPT_FILES)) \
    $(OPT) \
    "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source" \
    "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/kernel/nortos" \
    "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/kernel/nortos/posix" \
    $(ARCHFLAGS) \
    -Wall \
    -DBENCH_OPT='"$(OPT)"' \
    -DPERFORMANCE_RUN=1 \
    -DITERATIONS=$(ITERATIONS) \
    -DCOMPILER_FLAGS='"$(OPT) $(ARCHFLAGS)"'

ifneq ($(CONFIG),RELEASE)
CFLAGS += $(DFLAGS)
endif

LFLAGS += "-L$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source" \
    "-L$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/kernel/nortos" \
    "-Wl,--search_path=." \
    ti_utils_build_linker.cmd.genlibs \
    ../lpf3_nortos.cmd \
    "-Wl,-m,$(NAME).map" \
    -Wl,--rom_model \
    -Wl,--warn_sections \
    "-L$(TICLANG_ARMCOMPILER)/lib" \
    -llibc.a

all: postbuild

.PHONY: postbuild
postbuild: $(NAME).out
	"$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/tools/common/crc_tool/crc_tool" patch-image --elf "./$(NAME).out" --symbol-prefix ti_utils_build_GenMap_sym_CRC --output "./$(NAME).out"
	"$(TICLANG_ARMCOMPILER)/bin/tiarmobjcopy" -O ihex "./$(NAME).out" "./$(NAME).hex"

.INTERMEDIATE: syscfg
$(SYSCFG_FILES): syscfg
	@ echo generation complete

syscfg: ../bench.syscfg
	@ echo Generating configuration files...
	$(V) $(SYSCFG_CMD_STUB) --output $(@D) $<

define C_RULE
$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))

bench.obj: ../bench.c ../bench_timer.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

main_nortos.obj: ../main_nortos.c $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

core_main.obj: ../../../third-party/coremark/core_main.c ../coremark/core_portme.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -Dmain=coremark_main -c $< -o $@

core_portme.obj: ../coremark/core_portme.c ../coremark/core_portme.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

%.obj: ../../../third-party/coremark/%.c ../coremark/core_portme.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(NAME).out: $(OBJECTS)
	@ echo linking $@
	$(V) $(LNK) -Wl,-u,_c_int00 $(OBJECTS)  $(LFLAGS) -o $(NAME).out

clean:
	$(V) $(RM) *.obj *.d $(NAME).out $(NAME).map $(NAME).hex $(SYSCFG_FILES)
