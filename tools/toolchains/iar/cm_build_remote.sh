#!/bin/bash
# MacBook side. Builds one IAR CoreMark image on the build host (ssh $IAR_HOST) and copies
# coremark.out and coremark.map back.   cm_build_remote.sh <m33|g3519|l1306> <opt> <mhz> <dest dir> [ITER]
# Env knobs reach coremark_iar.sh: TAG XCC LISTS NOSC MFC.
set -e
B=$1; OPT=$2; MHZ=$3; DEST=$4; ITER=${5:-}; H=${IAR_HOST:?set IAR_HOST to the IAR build machine (ssh host name)}; TIR=${FRL_TI_ROOT:-/opt/ti}; HERE="$(cd "$(dirname "$0")" && pwd)"
ssh "$H" 'mkdir -p ~/iar-build/harness3'; rsync -a "$HERE/coremark_iar.sh" "$H:iar-build/harness3/"
ssh "$H" "bash -ls" <<REMOTE
set -e
docker run --rm --network host --platform linux/amd64 -v $TIR:$TIR -e FRL_TI_ROOT=$TIR -v \$HOME/iar-build/harness3:/harness:ro -v frl-iar-home:/root -v $TIR/iar-devsup:/devsup:ro \
  -e TAG='${TAG:-}' -e XCC='${XCC:-}' -e LISTS='${LISTS:-0}' -e NOSC='${NOSC:-0}' -e MFC='${MFC:-0}' -e CM_LOG='${CM_LOG:-}' \
  frl-iar:10.10.2 bash /harness/coremark_iar.sh $B $OPT $MHZ $ITER > /tmp/cm-build-$B-$OPT-$MHZ${TAG:+-$TAG}.log 2>&1 || { tail -20 /tmp/cm-build-$B-$OPT-$MHZ${TAG:+-$TAG}.log; exit 1; }
tail -4 /tmp/cm-build-$B-$OPT-$MHZ${TAG:+-$TAG}.log
REMOTE
mkdir -p "$DEST"; rsync -a "$H:$TIR/frl-out/$B-$OPT-$MHZ${TAG:+-$TAG}/" "$DEST/" --include='coremark.out' --include='coremark.hex' --include='coremark.map' --include='size.txt' --include='lists/' --include='lists/*' --exclude='*'
