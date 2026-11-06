#!/bin/bash
# Runs INSIDE the frl-iar container. Reads a list of "<ewp path><TAB><config>" lines and builds each
# with iarbuild. One log per line in $LOGDIR, one result line per build in $LOGDIR/results.tsv:
#   <ewp><TAB><config><TAB><exit code><TAB><seconds><TAB><log file>
# Usage (the harness calls this): batch.sh <list file> <log dir> [varfile] [timeout seconds]
LIST=$1; LOGDIR=$2; TMO=${4:-600}
. /etc/profile.d/iar.sh
# TI device files are not in the .deb: they come from IAR's public device-support tarball, extracted on the
# mini at $FRL_TI_ROOT/iar-devsup and mounted at /devsup. Link them into the install (symlinks only, no copy).
[ -d /devsup/cxarm-10.10.2/arm ] && cp -rsn /devsup/cxarm-10.10.2/arm/. /opt/iar/cxarm/arm/
# some SDK pre-build steps (FreeRTOS kernel library) call iarbuild.exe, the Windows name
VARS=${3:-}
printf '#!/bin/sh\nexec iarbuild "$@" ${IAR_VARFILE:+-varfile "$IAR_VARFILE"}\n' > /usr/local/bin/iarbuild.exe; chmod +x /usr/local/bin/iarbuild.exe; export IAR_VARFILE=$VARS
mkdir -p "$LOGDIR"; : > "$LOGDIR/results.tsv"
n=0
while IFS=$'\t' read -r ewp cfg; do
  [ -n "$ewp" ] || continue
  n=$((n+1)); log="$LOGDIR/$(printf %05d $n).log"
  t0=$(date +%s)
  ( cd "$(dirname "$ewp")" && timeout "$TMO" iarbuild "$ewp" -build "$cfg" -log all ${VARS:+-varfile "$VARS"} ) > "$log" 2>&1
  rc=$?
  printf '%s\t%s\t%s\t%s\t%s\n' "$ewp" "$cfg" "$rc" "$(( $(date +%s) - t0 ))" "$log" >> "$LOGDIR/results.tsv"
done < "$LIST"
