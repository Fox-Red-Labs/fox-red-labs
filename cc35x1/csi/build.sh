#!/bin/bash
# Build TI's two Wi-Fi CSI occupancy examples for the LP-EM-CC35X1 from the command line (Mac). Written 2026-10-09.
# Touches no board and no probe. Needs about 250 MB free (plugin zip 63 MB, unpacked 168 MB).
# The Wi-Fi network name and password are NOT compiled in: they are typed to the board at run time (csi_run.py).
#   cc35x1/csi/build.sh
# Output: $RUN/x/simplelink_sdk_edgeai_plugin_1_20_00_00/examples/rtos/LP_EM_CC35X1/edgeai/<example>/freertos/ticlang/
#   <example>.out and toolbox/tool_settings.json (the file `programmer ... programming --tool_settings` takes).
set -eu
RUN="$PWD/out/cc35x1/csi-dl"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39
TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
URL=https://dr-download.ti.com/software-development/plug-in/MD-DqX4NYJAzq/1.20.00.00/simplelink_sdk_edgeai_plugin_1_20_00_00__all.zip
SHA=a4ba1aea3cd6fd472709ae18bd8a952c6974cd67eee1f3fb4477a32c598d0856   # 66,163,438 bytes, no sign-in needed
P="$RUN/x/simplelink_sdk_edgeai_plugin_1_20_00_00"
mkdir -p "$RUN"
if [ ! -d "$P" ]; then
	curl -fL --max-time 600 -o "$RUN/plugin.zip" "$URL"
	[ "$(shasum -a 256 "$RUN/plugin.zip" | cut -d' ' -f1)" = "$SHA" ] || { echo "checksum differs from the one recorded 2026-10-09"; exit 1; }
	mkdir -p "$RUN/x"; unzip -q "$RUN/plugin.zip" -d "$RUN/x"; rm "$RUN/plugin.zip"
fi
for e in live_preview data_capture; do
	d="$P/examples/rtos/LP_EM_CC35X1/edgeai/edgeai_wifi_csi_occupancy_detection_$e/freertos/ticlang"
	# The zip ships TI's own build products with old generated files. Remove them and touch the .syscfg so SysConfig 1.28 regenerates
	# (without this the Toolbox step fails: action_request_extra.txt is missing).
	( cd "$d"; rm -f *.obj *.out *.map action_req.txt action_params.json; touch ../*.syscfg
	  make SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/build-$e.log" 2>&1 )
	make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$d" BUILD_DIR="$d" \
		BUILD_ARTIFACT="$d/edgeai_wifi_csi_occupancy_detection_$e.out" TOOLBOX_DIR="$TB" > "$RUN/img-$e.log" 2>&1
	echo "$e: built and signed; $("$CGT/bin/tiarmsize" "$d/edgeai_wifi_csi_occupancy_detection_$e.out" | tail -1 | awk '{print $1" B code+const, "$3" B bss"}')"
done
