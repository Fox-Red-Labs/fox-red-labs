#!/bin/bash
# build.sh <work dir>: the bare CC1407P host on the LP-EM-CC2755P10 (F3 SDK NoRTOS). Needs the repo root in REPO.
set -e
W="${1:?work dir}"; REPO="${REPO:?repo root}"; HERE="$REPO/cc2755p10/host-cc1407p"
F3=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$W"; mkdir -p "$W"; cp -R "$F3/examples/nortos/LP_EM_CC2755P10/drivers/empty/." "$W"; rm -rf "$W/iar" "$W/gcc" "$W"/README.*
cp "$HERE"/{hb_p10.c,empty.c,empty.syscfg,app_tx_nostart.c} "$W/"
python3 - "$W/ticlang/makefile" "$REPO" <<'PY'
import sys
p, R = sys.argv[1:3]; s = open(p).read()
T = R + "/third-party/ti-cc140xp-host"; P = R + "/portability/cc1407p-host"
src = [f"{T}/host/trx_host.c", f"{T}/host/handlers/trx_host_handlers.c", f"{T}/host/private/trx_host_private.c",
       f"{T}/host/transport/spi_transport.c", f"{T}/host/utility/utils.c", f"{T}/host/utility/list/list.c",
       f"{T}/host/utility/buffer_manager/buffer_manager.c", f"{T}/rcl_settings_wisun.c", f"{P}/hal_bare.c", "../app_tx_nostart.c", "../hb_p10.c"]
s = s.replace("OBJECTS = empty.obj", "EXTRA_SRC = " + " ".join(src) + "\nOBJECTS = $(patsubst %.c,%.obj,$(notdir $(EXTRA_SRC))) empty.obj")
s = s.replace("CFLAGS += -I.. \\", f"CFLAGS += -Wno-everything -I{T}/host -I{T} -I{P} -I{P}/shim -I.. \\")
s = s.replace("$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))", "$(foreach c_file,$(SYSCFG_C_FILES) $(EXTRA_SRC),$(eval $(call C_RULE,$(c_file))))")
open(p, "w").write(s)
PY
make -C "$W/ticlang" SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=$F3 SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT
python3 "$REPO/cc2755p10/bench/hexcheck.py" "$W/ticlang/empty.hex"
