#!/bin/sh
# Fourth stage inside frl-steam: run Valve's steam.sh under an x86-64 bash (Debian bash 5.2.37 amd64, unpacked into
# /s/x86 with `apt-get download`, nothing installed) that Box64 emulates, so every x86 child steam.sh starts goes
# through Box64's own exec path; the kernel has no binfmt_misc. Stops at a time limit; screenshots every 30 s.
set -u
W=/s/work; B=$W/boot; X=/s/x86/root
export HOME=/s/home DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 BOX64_LOG=1 BOX64_BASH=$X/usr/bin/bash
export BOX64_LD_LIBRARY_PATH=$X/usr/lib/x86_64-linux-gnu
pgrep Xvfb >/dev/null || { Xvfb :1 -screen 0 1280x720x24 >/dev/null 2>&1 & sleep 2; }
T=${1:-900}; s=$(date +%s)
cd $B && box64 $X/usr/bin/bash ./steam.sh -no-cef-sandbox < /dev/null > $W/steamsh.log 2>&1 &
P=$!; i=0
while kill -0 $P 2>/dev/null && [ $(( $(date +%s)-s )) -lt $T ]; do
  sleep 30; i=$((i+1)); import -window root $W/sh-$i.png 2>/dev/null
  echo "t=$(( $(date +%s)-s )) s used=$(free -m | awk '/Mem/{print $3}') MB procs=$(ps -e | wc -l) last: $(tail -1 $W/steamsh.log | cut -c1-140)"
done
kill -0 $P 2>/dev/null && { echo "time limit"; ps -eo rss,args --sort=-rss | head -6; pkill -f steam; } || { wait $P; echo "exited rc=$?"; }
grep -iE "error|unsupported|failed|assert|login|abort|signal|not found|cannot" $W/steamsh.log | sort | uniq -c | sort -rn | head -15
