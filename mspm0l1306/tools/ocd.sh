#!/bin/bash
# Run OpenOCD commands against the LP-MSPM0L1306 on one probe only. Usage: PROBE=<serial> ocd.sh "cmd1" "cmd2" ...
# Server ports are off so this runs while another OpenOCD (another board) holds port 3333.
# PROBE is required: the XDS110 serial to use (python3 tools/frl/frl.py detect lists them). There is no built-in
# default; this bench's own serial comes from the git-ignored tools/benchmark/settings.local.json ("lab_probes"),
# so a command copied to another machine stops instead of reaching for a board that is not there.
# Optional: OCD (xPack OpenOCD folder, else openocd on PATH), OCD_CFG (a connection file that replaces the interface
# and target lines below), XDSDFU (TI's xdsdfu; when found, the probe must be on USB before OpenOCD runs).
# The probe is held through tools/frl/lock.py for the length of the command, so two runs cannot share it.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PROBE="${PROBE:-$(python3 "$ROOT/tools/frl/labprobe.py" mspm0l1306)}"
[ -n "$PROBE" ] || { echo "Set PROBE to your board's XDS110 serial: PROBE=<serial> $0 ... (python3 tools/frl/frl.py detect lists them)"; exit 3; }
case "$PROBE" in *[!A-Za-z0-9]*) echo "PROBE '$PROBE' is not a probe serial"; exit 3;; esac
if [ "${FRL_LOCK_HELD:-}" != "$PROBE" ]; then
  exec python3 "$ROOT/tools/frl/lock.py" exec --serial "$PROBE" --op "ocd.sh" -- "$0" "$@"
fi
OCD="${OCD:-$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7}"
if [ -x "$OCD/bin/openocd" ]; then BIN="$OCD/bin/openocd"; SCRIPTS=(-s "${OCD_SCRIPTS:-$OCD/openocd/scripts}")
elif command -v openocd >/dev/null; then BIN="$(command -v openocd)"; SCRIPTS=(); [ -n "${OCD_SCRIPTS:-}" ] && SCRIPTS=(-s "$OCD_SCRIPTS")
else echo "OpenOCD not found: set OCD to the xPack OpenOCD folder or put openocd on PATH"; exit 3; fi
XDSDFU="${XDSDFU:-$(ls -d /Applications/ti/ccs*/ccs/ccs_base/common/uscif/xds110/xdsdfu "$HOME"/ti/ccs*/ccs/ccs_base/common/uscif/xds110/xdsdfu 2>/dev/null | tail -1)}"
if [ -n "$XDSDFU" ] && [ -x "$XDSDFU" ]; then
  "$XDSDFU" -e 2>/dev/null | grep -q "Serial Num: *$PROBE\$" || { echo "Probe $PROBE missing"; exit 3; }
fi
args=(); for c in "$@"; do args+=(-c "$c"); done
if [ -n "${OCD_CFG:-}" ]; then cfg=(-f "$OCD_CFG" -c "adapter serial $PROBE" -c "adapter speed 2500")
else cfg=(-f interface/xds110.cfg -c "adapter serial $PROBE" -c "adapter speed 2500" -f target/ti_mspm0.cfg); fi
"$BIN" "${SCRIPTS[@]+"${SCRIPTS[@]}"}" -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" "${cfg[@]}" -c init "${args[@]}" -c shutdown 2>&1
