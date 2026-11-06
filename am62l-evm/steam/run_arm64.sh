#!/bin/sh
# Inside frl-steam-arm64: start Valve's native arm64 client (steamrtarm64/steam) on a virtual X screen with
# Mesa llvmpipe. No account is entered; a drawn login window is the stop point. Every 20 s: RAM, the top-level
# windows, a screenshot. Usage: sh /s/run_arm64.sh [seconds] [extra steam flags...]
set -u
export HOME=/s/arm64/home DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1
R=$HOME/.local/share/Steam; O=/s/arm64/out; mkdir -p $O
T=${1:-1200}; shift 2>/dev/null || true
pgrep Xvfb >/dev/null || { Xvfb :1 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2; }
glxinfo -B 2>/dev/null | grep -E "renderer string" | head -1
export LD_LIBRARY_PATH=$R/steamrtarm64:$R/steamrtarm64/libs
# The client is built with ARMv8.1 LSE atomics; on the A53 they need the trap library (lse_trap/).
[ "${LSE_TRAP:-0}" = 1 ] && export LD_PRELOAD=/s/lse_trap/lse_trap.so LSE_TRAP_STATS=1
# libvideo.so wants Valve's FFmpeg extras (av_malloc_tracked...); avshim/ supplies them on stock FFmpeg 8.
[ "${AVSHIM:-0}" = 1 ] && export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}/s/avshim/avshim.so"
s=$(date +%s); first=""
cd $R && ./steamrtarm64/steam -noverifyfiles -no-cef-sandbox "$@" > $O/client.log 2>&1 &
P=$!; i=0
while kill -0 $P 2>/dev/null && [ $(( $(date +%s)-s )) -lt $T ]; do
  sleep 20; i=$((i+1)); t=$(( $(date +%s)-s ))
  w=$(xwininfo -root -children 2>/dev/null | grep -E '^ +0x' | grep -v 'has no name' | sed 's/^ *0x[0-9a-f]* //' | cut -c1-60 | tr '\n' ';')
  [ -n "$w" ] && [ -z "$first" ] && first=$t
  import -window root $O/shot-$i.png 2>/dev/null
  echo "t=$t s used=$(free -m | awk '/Mem/{print $3}') MB rss_steam=$(ps -eo rss,comm | awk '/steam|cef|webhelper/{s+=$1} END{print int(s/1024)}') MB windows=[$w] last: $(tail -1 $O/client.log | cut -c1-110)"
done
echo "first named window at: ${first:-none} s"
kill -0 $P 2>/dev/null && { echo "time limit, stopping"; kill $P; sleep 3; pkill -f steamwebhelper; pkill -f steamrtarm64; } || { wait $P; echo "exited rc=$?"; }
grep -iE "error|fail|abort|signal|not found|cannot|login|update" $O/client.log | sort | uniq -c | sort -rn | head -20
