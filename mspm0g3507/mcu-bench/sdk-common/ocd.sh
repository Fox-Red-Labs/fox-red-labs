#!/bin/bash
# Run OpenOCD commands against one probe only. Usage: PROBE=<serial> ocd.sh "cmd1" "cmd2" ...
# Optional, set in the environment: PROBE (the XDS110 serial to use), OCD (xPack OpenOCD folder), OCD_CFG (connection file).
# PROBE has no built-in default (2026-10-14): this bench's serial comes from the git-ignored
# tools/benchmark/settings.local.json ("lab_probes"), so a copied command on another machine stops and asks.
OCD="${OCD:-$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7}"
OCD_CFG="${OCD_CFG:-$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PROBE="${PROBE:-$(python3 "$ROOT/tools/frl/labprobe.py" mspm0g3507)}"
[ -n "$PROBE" ] || { echo "Set PROBE to your board's XDS110 serial (python3 tools/frl/frl.py detect lists them)"; exit 3; }
args=(); for c in "$@"; do args+=(-c "$c"); done
"$OCD/bin/openocd" -s "${OCD_SCRIPTS:-$OCD/openocd/scripts}" -f "$OCD_CFG" -c "adapter serial $PROBE" -c init "${args[@]}" -c shutdown 2>&1
