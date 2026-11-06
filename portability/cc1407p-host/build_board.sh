#!/bin/bash
# Build the CC1407P host driver bench (TI's driver unmodified, this repo's bare HAL, the fake-radio hooks and
# bench_main.c) as a normal SDK image for one LaunchPad, so the same bytes the driver puts on the SPI wire can be
# read back from RAM and compared with the PC reference (portability/cc1407p-host/c28x/README.md). No radio needed.
#   build_board.sh <g3519|m33> <work dir>     prints the image path and sizes
set -e
BOARD=${1:?g3519 or m33}; W=${2:?work dir}
R=$(cd "$(dirname "$0")/../.." && pwd); T=$R/third-party/ti-cc140xp-host; P=$R/portability/cc1407p-host
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
case $BOARD in
  g3519) SDK=/Applications/ti/mspm0_sdk_2_11_00_07; VAR=MSPM0_SDK_INSTALL_DIR; EX=$SDK/examples/nortos/LP_MSPM0G3519/driverlib/empty;;
  m33)   SDK=/Applications/ti/mspm33_sdk_1_04_00_00; VAR=MSPM33_SDK_INSTALL_DIR; EX=$SDK/examples/nortos/LP_MSPM33C321A/driverlib/empty;;
  *) echo "board?"; exit 2;;
esac
rm -rf "$W"; mkdir -p "$W"; cp -R "$EX" "$W/empty"; D=$W/empty
cp "$P/c28x/bench_main.c" "$D/empty.c"; rm -f "$D/ti_msp_dl_config.c" "$D/ti_msp_dl_config.h"
EXTRA="$T/rcl_settings_wisun.c $T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c $P/hal_bare.c $P/c28x/hooks_fake.c"
python3 - "$D/ticlang/makefile" "$EXTRA" "$T" "$P" <<'PY'
import sys
m, extra, t, p = sys.argv[1:]
s = open(m).read()
block = f"""
CFLAGS += -I{t}/host -I{t} -I{p} -I{p}/shim -w
EXTRA_SRCS = {extra}
OBJECTS += $(patsubst %.c,%.obj,$(notdir $(EXTRA_SRCS)))
$(foreach c_file,$(EXTRA_SRCS),$(eval $(call C_RULE,$(c_file))))

$(NAME).out:"""
assert "\n$(NAME).out:" in s
open(m, "w").write(s.replace("\n$(NAME).out:", block, 1))
PY
make -C "$D/ticlang" $VAR=$SDK SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT GCC_ARMCOMPILER= > "$W/build.log" 2>&1 || { tail -20 "$W/build.log"; exit 1; }
echo "$D/ticlang/empty.out"; $CGT/bin/tiarmsize "$D/ticlang/empty.out"
