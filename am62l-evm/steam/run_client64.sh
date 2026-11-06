#!/bin/sh
# Third stage inside frl-steam: start the installed 64-bit client (steamrt64/steam, x86-64) under Box64 directly,
# with its own folder as the x86 library path. Stops at a time limit; screenshots every 60 s.
set -u
W=/s/work; B=$W/boot; export HOME=/s/home DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 BOX64_LOG=1
pgrep Xvfb >/dev/null || { Xvfb :1 -screen 0 1280x720x24 >/dev/null 2>&1 & sleep 2; }
export BOX64_LD_LIBRARY_PATH=$B/steamrt64:$B/linux64:$B/ubuntu12_64
T=${1:-900}; s=$(date +%s)
cd $B && box64 ./steamrt64/steam -no-cef-sandbox > $W/client64.log 2>&1 &
P=$!; i=0
while kill -0 $P 2>/dev/null && [ $(( $(date +%s)-s )) -lt $T ]; do
  sleep 30; i=$((i+1)); import -window root $W/c64-$i.png 2>/dev/null
  echo "t=$(( $(date +%s)-s )) s used=$(free -m | awk '/Mem/{print $3}') MB procs=$(ps -e | wc -l) last: $(tail -1 $W/client64.log | cut -c1-140)"
done
kill -0 $P 2>/dev/null && { echo "time limit"; kill $P; sleep 2; pkill -f steam; } || { wait $P; echo "exited rc=$?"; }
grep -iE "error|unsupported|failed|assert|login|abort|signal|not found|cannot" $W/client64.log | sort | uniq -c | sort -rn | head -15
