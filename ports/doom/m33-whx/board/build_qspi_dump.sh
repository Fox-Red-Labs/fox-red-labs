#!/bin/bash
# Build the read-only QSPI dump firmware (E27 steps 1 and 3) from TI's qspi_flash_readback_DMA project files (SysConfig and linker script).
#   build_qspi_dump.sh [out dir]      output <out>/ticlang/qspi_flash_readback_DMA.out and .map. Nothing here flashes a board.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"
O="${1:-$PWD/out/boosterpack-speedrun/m33-qspi-dump/fw}"; EX=qspi_flash_readback_DMA
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"
rm -rf "$O"; mkdir -p "$(dirname "$O")"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/demos/$EX" "$O"
rm -rf "$O/gcc" "$O/iar" "$O/keil" "$O/README".*
cp "$HERE/qspi_dump.c" "$O/$EX.c"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo built || { tail -20 "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
