#!/bin/sh
# Second stage inside frl-steam: skip steam.sh (a native shell script cannot exec the x86 helpers it calls, and the
# kernel has no binfmt_misc), make the data links steam.sh would make, and run the 32-bit bootstrap under Box64's
# BOX32 mode so it downloads and starts the client. Stops at a time limit; screenshots every 60 s.
set -u
W=/s/work; export HOME=/s/home DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 BOX64_LOG=1
pgrep Xvfb >/dev/null || { Xvfb :1 -screen 0 1280x720x24 >/dev/null 2>&1 & sleep 2; }
mkdir -p $HOME/.steam
ln -sfn $W/boot $HOME/.steam/steam; ln -sfn $W/boot $HOME/.steam/root
ln -sfn $W/boot/ubuntu12_32 $HOME/.steam/bin32
T=${1:-1500}; s=$(date +%s)
cd $W/boot && box64 ./ubuntu12_32/steam -no-cef-sandbox > $W/client.log 2>&1 &
P=$!
i=0
while kill -0 $P 2>/dev/null && [ $(( $(date +%s)-s )) -lt $T ]; do
  sleep 60; i=$((i+1)); import -window root $W/client-$i.png 2>/dev/null
  echo "t=$(( $(date +%s)-s )) s used=$(free -m | awk '/Mem/{print $3}') MB steam-dir=$(du -sm $W/boot | cut -f1) MB procs=$(pgrep -c box64) last: $(tail -1 $W/client.log | cut -c1-120)"
done
kill -0 $P 2>/dev/null && { echo "time limit"; pkill -f box64; } || { wait $P; echo "exited rc=$?"; }
grep -iE "error|unsupported|failed|assert|login|abort|signal|segfault" $W/client.log | sort | uniq -c | sort -rn | head -15
