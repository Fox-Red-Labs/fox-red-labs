#!/bin/bash
# Build the CC1407P host for the LP-MSPM0L1306: TI's SDK SPI example folder with our .c, .syscfg, TI's CC140xP host driver
# (third-party/ti-cc140xp-host, buffer pool cut from 24 to 4 so it fits 4 kB of RAM) and the bare HAL (portability/cc1407p-host).
#   build.sh [extra CFLAGS]     e.g. build.sh -DHB_TRACE     image: mspm0l1306/radio-host/out/radio_host.out
set -e
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."; E=spi_controller_echo_interrupts
NB=${NBUF:-4}
T0=$REPO/third-party/ti-cc140xp-host; P=$REPO/portability/cc1407p-host
W=${WORK:-$PWD/out/bench-wave/l1306/build}; rm -rf "${W:?}"; mkdir -p "$W"
# TI hard-codes 24 buffers; a smaller pool is a copy of the driver with only that number changed (as portability/cc1407p-host/build_fit.sh)
T=$W/t; mkdir -p "$T"; cp -R "$T0/host" "$T0/settings" "$T0"/*.h "$T0/rcl_settings_2gfsk50kbps.c" "$T/"
sed -i.bak "s/BUFFER_MANAGER_NBUFFERS    (24U)/BUFFER_MANAGER_NBUFFERS    (${NB}U)/" "$T/host/utility/buffer_manager/buffer_manager.h"
grep -q "(${NB}U)" "$T/host/utility/buffer_manager/buffer_manager.h"
cp -R $SDK/examples/nortos/LP_MSPM0L1306/driverlib/$E "$W/"
cp "$REPO/mspm0g5187/radio/radio_host.c" "$W/$E/$E.c"; cp "$HERE/radio.syscfg" "$W/$E/$E.syscfg"
rm -f "${W:?}/${E:?}/ti_msp_dl_config.c" "${W:?}/${E:?}/ti_msp_dl_config.h"
EXTRA="$T/rcl_settings_2gfsk50kbps.c $T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c $P/hal_bare.c $HERE/hooks_mspm0l1306.c"
M="$W/$E/ticlang/makefile"
python3 - "$M" "$EXTRA" "$T" "$P" "$*" <<'PY'
import sys
m, extra, t, p, cf = sys.argv[1:]
s = open(m).read()
block = f"""
CFLAGS += {cf} -I{t}/host -I{t}/settings -I{t} -I{p} -I{p}/shim -w
EXTRA_SRCS = {extra}
OBJECTS += $(patsubst %.c,%.obj,$(notdir $(EXTRA_SRCS)))
$(foreach c_file,$(EXTRA_SRCS),$(eval $(call C_RULE,$(c_file))))

$(NAME).out:"""
s = s.replace("\n$(NAME).out:", block, 1)
open(m, "w").write(s)
PY
"$REPO/mspm0g3507/mcu-bench/sdk-common/sdkmake.sh" "$W/$E/ticlang" 2>&1 | tail -25
ls -la "$W/$E/ticlang/$E.out" && $CC/bin/tiarmsize "$W/$E/ticlang/$E.out"
mkdir -p "$HERE/out" && cp "$W/$E/ticlang/$E.out" "$HERE/out/radio_host.out" && echo "image: mspm0l1306/radio-host/out/radio_host.out"
