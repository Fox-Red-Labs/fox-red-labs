# Bare-metal benchmark for the LP-EM-CC2755P10, TI Arm Clang. Not run yet (written 2026-10-07).
# Copied from cc2340r5/baremetal/makefile; Cortex-M33 flags, TF-M include paths and the linker file
# come from the SDK's examples/nortos/LP_EM_CC2755P10/drivers/empty/ticlang/makefile.
# SRC is this folder (prebuild.sh runs it from a build folder outside the repo):
#   make -C <build dir> -f <this folder>/makefile SRC=<this folder> OPT=-O3
# Differences from the SDK makefile: more source files (benchmark + EEMBC CoreMark, unmodified, from
# third-party/coremark), the optimization level comes from OPT (default -Oz, the SDK's own setting).
# ITERATIONS 5000 (the CC2340R5 used 2000; 4000 ran 9.38 s at 426 per second, under CoreMark's 10 s minimum).
# Clock, power and CCFG all come from SysConfig defaults (bench.syscfg = empty.syscfg + UART2).
SRC ?= ..
REPO ?= $(SRC)/../..

SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR ?= /Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS

include $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/imports.mak

OPT ?= -Oz
ITERATIONS ?= 5000

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"

SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output . $(SRC)/bench.syscfg)

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

ARCHFLAGS = -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -mthumb

CFLAGS += -I$(SRC) \
    -I$(SRC)/coremark \
    -I$(REPO)/third-party/coremark \
    "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/tfm_s/build/cc27xxx10/production_full/Release/export/tfm/inc" \
    "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/third_party/tfm/interface/include" \
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
    "-L$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/tfm_s" \
    "-L$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/kernel/nortos" \
    "-Wl,--search_path=." \
    ti_utils_build_linker.cmd.genlibs \
    $(SRC)/../bench/board/lpf3_nortos.cmd \
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

syscfg: $(SRC)/bench.syscfg
	@ echo Generating configuration files...
	$(V) $(SYSCFG_CMD_STUB) --output $(@D) $<

define C_RULE
$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))

bench.obj: $(SRC)/bench.c $(SRC)/bench_timer.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

main_nortos.obj: $(SRC)/main_nortos.c $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

core_main.obj: $(REPO)/third-party/coremark/core_main.c $(SRC)/coremark/core_portme.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -Dmain=coremark_main -c $< -o $@

core_portme.obj: $(SRC)/coremark/core_portme.c $(SRC)/coremark/core_portme.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

%.obj: $(REPO)/third-party/coremark/%.c $(SRC)/coremark/core_portme.h $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(NAME).out: $(OBJECTS)
	@ echo linking $@
	$(V) $(LNK) -Wl,-u,_c_int00 $(OBJECTS)  $(LFLAGS) -o $(NAME).out

clean:
	$(V) $(RM) *.obj *.d $(NAME).out $(NAME).map $(NAME).hex $(SYSCFG_FILES)
