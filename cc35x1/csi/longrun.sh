#!/usr/bin/env bash
# Long-run Wi-Fi presence logger for the LP-EM-CC35X1 on TI's CSI presence image (roadmap E187).
# Run this in your own terminal: the first pass reads the Wi-Fi password from the macOS keychain.
#   cc35x1/csi/longrun.sh            # join once, then log; restarts the logger with --resume if it exits
# Rows land in cc35x1/csi/results/<date>-freerun.csv; the log in cc35x1/csi/results/wifi-logger-longrun.log. Ctrl-C stops it.
set -u
cd "$(dirname "$0")/../.."
PY=~/.venvs/csi/bin/python; PORT=/dev/cu.usbmodemE10000FQ1; LOG=cc35x1/csi/results/wifi-logger-longrun.log
trap 'echo "stopped $(date +%F\ %T)" | tee -a "$LOG"; exit 0' INT TERM
echo "start $(date +%F\ %T)" | tee -a "$LOG"
$PY cc35x1/csi/csi_run.py --port $PORT --freerun --keychain 2>&1 | tee -a "$LOG"
while [ -e $PORT ]; do
  echo "restart $(date +%F\ %T)" | tee -a "$LOG"; sleep 3
  $PY cc35x1/csi/csi_run.py --port $PORT --freerun --resume 2>&1 | tee -a "$LOG"
done
echo "port gone $(date +%F\ %T)" | tee -a "$LOG"
