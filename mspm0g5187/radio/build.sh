#!/bin/bash
# Build the CC1407P host for the LP-MSPM0G5187: TI's SDK SPI example folder with our .c, .syscfg,
# TI's CC140xP host driver (third-party/ti-cc140xp-host) and the bare HAL (portability/cc1407p-host).
set -e
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."; E=spi_controller_echo_interrupts
T=$REPO/third-party/ti-cc140xp-host; P=$REPO/portability/cc1407p-host
W=$PWD/out/bp-cc1407p-9-fe/g5187; rm -rf "$W"; mkdir -p "$W"
cp -R $SDK/examples/nortos/LP_MSPM0G5187/driverlib/$E "$W/"
cp "$HERE/${RADIO_APP:-radio_host}.c" "$W/$E/$E.c"; cp "$HERE/radio.syscfg" "$W/$E/$E.syscfg"
if [ "${RADIO_APP:-radio_host}" = radio_lab ]; then cp "$HERE/phy_matrix.c" "$W/$E/phy_matrix.c"; fi
rm -f "$W/$E/ti_msp_dl_config.c" "$W/$E/ti_msp_dl_config.h"
case "${RADIO_APP:-radio_host}" in phy_matrix|radio_lab) ALLSET=1;; *) ALLSET=0;; esac; if [ $ALLSET = 1 ]; then SET="$(ls $T/settings/*.c | tr '\n' ' ')"; else SET="$T/rcl_settings_2gfsk50kbps.c"; fi
EXTRA="$SET $T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c $P/hal_bare.c $HERE/hooks_mspm0g5187.c"
M="$W/$E/ticlang/makefile"
python3 - "$M" "$EXTRA" "$T" "$P" <<'PY'
import sys
m, extra, t, p = sys.argv[1:]
s = open(m).read()
block = f"""
CFLAGS += ${{RADIO_CFLAGS}} -I{t}/host -I{t}/settings -I{t} -I{p} -I{p}/shim -w
EXTRA_SRCS = {extra}
OBJECTS += $(patsubst %.c,%.obj,$(notdir $(EXTRA_SRCS)))
$(foreach c_file,$(EXTRA_SRCS),$(eval $(call C_RULE,$(c_file))))

$(NAME).out:"""
s = s.replace("\n$(NAME).out:", block, 1)
open(m, "w").write(s)
PY
"$REPO/mspm0g3507/mcu-bench/sdk-common/sdkmake.sh" "$W/$E/ticlang" 2>&1 | tail -25
ls -la "$W/$E/ticlang/$E.out" && $CC/bin/tiarmsize "$W/$E/ticlang/$E.out"
mkdir -p "$HERE/out" && cp "$W/$E/ticlang/$E.out" "$HERE/out/${RADIO_APP:-radio_host}.out" && echo "image: mspm0g5187/radio/out/${RADIO_APP:-radio_host}.out"
