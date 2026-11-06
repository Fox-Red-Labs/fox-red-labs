#!/bin/sh
# Inside frl-steam-user, as uid 1000: start Valve's native arm64 client on a virtual X screen and time it to the
# "Sign in to Steam" window. No account is entered; that window is the stop point.
# Usage: sh /s/run_user.sh TAG [seconds_to_wait=300] [idle_seconds=60] [extra steam flags...]
# Env: WINRE (window-title regex to wait for, default the sign-in window), CEF_SANDBOX=1 (default; 0 adds -no-cef-sandbox), LSE_PATCH=1 (hot-patch LSE sites), KEEP=1 (leave it
#      running after the idle sample), DISPLAY_NUM (default 1), XVFB_SIZE (default 1280x800).
# Prints: seconds to the window, container CPU seconds used to get there, CPU percent while idle at the window,
# RAM (container memory.current, which includes page cache; modes_host.sh logs whole-board used), trapped LSE instructions per second while idle.
set -u
TAG=${1:?tag}; T=${2:-300}; IDLE=${3:-60}; shift 3 2>/dev/null || shift $#
export HOME=/data/home DISPLAY=:${DISPLAY_NUM:-1} LIBGL_ALWAYS_SOFTWARE=1
R=$HOME/.local/share/Steam; O=/data/runs/$TAG; rm -rf $O; mkdir -p $O/lse
pgrep -u "$(id -u)" Xvfb >/dev/null || { Xvfb $DISPLAY -screen 0 ${XVFB_SIZE:-1280x800}x24 >/dev/null 2>&1 & sleep 2; }
export LD_LIBRARY_PATH=$R/steamrtarm64:$R/steamrtarm64/libs
export LD_PRELOAD=/opt/frl/lse_trap.so:/opt/frl/avshim.so LSE_TRAP_DIR=$O/lse
SB=""; [ "${CEF_SANDBOX:-1}" = 1 ] || SB="-no-cef-sandbox"
cpu() { awk '/usage_usec/{print $2}' /sys/fs/cgroup/cpu.stat; }
mem() { echo $(( $(cat /sys/fs/cgroup/memory.current) / 1048576 )); }
lse() { cat $O/lse/* 2>/dev/null | od -An -v -tu8 -w8 | awk 'BEGIN{n=0} {v[n++]=$1} END{
  # struct ctr: magic pid comm[16] = 4 words, then ldop swp cas casp ldapr unknown unaligned bus patched patch_fail stub_bus inplace
  s=0; p=0; for (b=0; b+16<=n; b+=512) { if (v[b]!=1280525652) continue; for (k=4;k<=11;k++) if (k!=10) s+=v[b+k]; p+=v[b+12]+v[b+15] } print s, p }'; }
echo "$TAG: flags=[$SB $*] sandbox=${CEF_SANDBOX:-1} patch=${LSE_PATCH:-0}" | tee $O/summary.txt
# steamwebhelper.sh pins the browser to CPUs 2 to 6 (taskset 0x7c); the AM62L has CPUs 0 and 1. A client update
# rewrites the file, so drop the pin before every start.
sed -i 's/taskset 0x7c //' $R/steamrtarm64/steamwebhelper.sh 2>/dev/null; grep -c taskset $R/steamrtarm64/steamwebhelper.sh 2>/dev/null | sed 's/^/taskset lines left: /'
c0=$(cpu); s0=$(date +%s.%N)
cd $R && ./steamrtarm64/steam -noverifyfiles $SB "$@" > $O/client.log 2>&1 &
P=$!; win=""; gone=0
# The client relaunches itself after an update (new pid), so wait on any steam process, with a 20 s grace.
while [ $gone -lt 10 ]; do
  if kill -0 $P 2>/dev/null || pgrep -u "$(id -u)" -f '[s]teamrtarm64/steam' >/dev/null; then gone=0; else gone=$((gone+1)); fi
  sleep 2; t=$(awk -v a="$(date +%s.%N)" -v b="$s0" 'BEGIN{printf "%.0f", a-b}')
  if xwininfo -root -tree 2>/dev/null | grep -qE "${WINRE:-\"Sign in to Steam\"}"; then win=$t; break; fi
  [ "$t" -gt "$T" ] && break
done
c1=$(cpu)
if [ -n "$win" ]; then
  # The window exists before its page is drawn; "painted" is the first screenshot with over 1,000 colors
  # (an empty frame has a few dozen; the drawn sign-in page with its QR code has thousands).
  paint=""
  while [ "$(awk -v a="$(date +%s.%N)" -v b="$s0" 'BEGIN{printf "%.0f", a-b}')" -le "$T" ]; do
    import -window root $O/window.png 2>/dev/null
    k=$(identify -format %k $O/window.png 2>/dev/null || echo 0)
    if [ "${k:-0}" -gt 1000 ]; then paint=$(awk -v a="$(date +%s.%N)" -v b="$s0" 'BEGIN{printf "%.0f", a-b}'); break; fi
    sleep 1
  done
  c2=$(cpu); sleep 3
  set -- $(lse); l0=$1; i0=$(cpu); sleep $IDLE; i1=$(cpu); set -- $(lse); l1=$1; pat=$2
  echo "window_s=$win painted_s=${paint:-none} cpu_to_paint_s=$(( (c2-c0)/1000000 )) colors=$k" \
       "cpu_to_window_s=$(( (c1-c0)/1000000 )) idle_cpu_pct=$(( (i1-i0)/(IDLE*10000) )) of one core" \
       "container_mem_mb=$(mem)" \
       "lse_traps_total=$l1 lse_traps_per_s_idle=$(( (l1-l0)/IDLE )) patched_sites=$pat" | tee -a $O/summary.txt
else
  echo "no sign-in window within ${T} s; last: $(tail -2 $O/client.log | tr '\n' ' ' | cut -c1-200)" | tee -a $O/summary.txt
fi
ps -eo stat,rss,args | awk '$1 !~ /^Z/ && /steam|webhelper/ && !/awk/ {s+=$2; n++} END{print "steam processes:", n, "rss_sum_mb:", int(s/1024)}' | tee -a $O/summary.txt
grep -ciE "sandbox" $O/client.log | sed 's/^/client.log lines mentioning sandbox: /' | tee -a $O/summary.txt
[ "${KEEP:-0}" = 1 ] && { echo "left running (pid $P)"; exit 0; }
kill $P 2>/dev/null; pkill -u "$(id -u)" -f '[s]teamrtarm64/steam '; sleep 4; pkill -u "$(id -u)" -f steamwebhelper; pkill -u "$(id -u)" -f steamrtarm64; sleep 2
pkill -9 -u "$(id -u)" -f 'steamwebhelper|steamrtarm64' 2>/dev/null; true
