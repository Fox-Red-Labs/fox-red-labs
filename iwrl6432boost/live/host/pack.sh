#!/usr/bin/env bash
# Pack the room sensors (radar people counting, Wi-Fi presence) for a Linux host such as the BeagleY-AI.
#   pack.sh [out.tgz]      run from anywhere in the repo; copy the file to the host and run install.sh there
set -euo pipefail
ROOT=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
OUT=${1:-room-host.tgz}
cd "$ROOT"
COPYFILE_DISABLE=1 tar --no-xattrs -czf "$OLDPWD/$OUT" --exclude __pycache__ --exclude 'runs/*' --exclude room-learned.json --exclude 'calibrated.cfg' \
  iwrl6432boost/live tools/ha.py cc35x1/csi/csi_run.py \
  cc35x1/csi/lab/server.py cc35x1/csi/lab/lab.py cc35x1/csi/lab/detectors.py \
  cc35x1/csi/lab/presence_out.py cc35x1/csi/lab/page.html
echo "wrote $OUT ($(du -h "$OLDPWD/$OUT" | cut -f1))"
