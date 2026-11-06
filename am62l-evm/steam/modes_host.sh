#!/bin/sh
# On the board (host): run bench_modes.sh one mode at a time, killing every leftover Steam process as root in
# between (the CEF sandbox's zygotes outlive the client's own shutdown), and log whole-board RAM in use.
# Usage: sh modes_host.sh mode...   -> /root/frl/steam/logs/modes/<mode>.txt
# Host root never writes into, or reads through, the container's writable mount (/root/frl/steam/data, owned by
# uid 1000): a process in the container could plant a symlink there. Host logs go to a root-only directory, and
# the container's own summary is copied out with docker cp. (Answers the security review of 2026-10-25.)
N=frl-steam-user; L=/root/frl/steam/logs/modes
mkdir -p $L; chmod 700 /root/frl/steam/logs $L
# Also empty /dev/shm: a killed CEF leaves its shared-memory files there, and a full 512 MB /dev/shm makes the next
# client die with SIGBUS (seen as crash dumps after the first run).
clean() { docker exec $N sh -c 'pkill -9 -f "[s]teamwebhelper|[s]teamrtarm64|[s]teamerrorrepor|[x]11vnc"; sleep 2; rm -rf /dev/shm/* /dev/shm/.[!.]* 2>/dev/null; true'; sleep 2; }
for m in "$@"; do
  clean; echo "$m: board used before $(free -m | awk '/Mem/{print $3}') MB"
  docker exec -u 1000 $N sh /s/bench_modes.sh $m > $L/$m.console.log 2>&1 &
  P=$!; peak=0
  while kill -0 $P 2>/dev/null; do u=$(free -m | awk '/Mem/{print $3}'); [ $u -gt $peak ] && peak=$u; sleep 2; done
  rm -f $L/$m.txt; docker cp $N:/data/runs/$m/summary.txt $L/$m.txt 2>/dev/null
  [ -L $L/$m.txt ] && rm -f $L/$m.txt                       # never follow a link the container made
  echo "board_used_peak_mb=$peak" >> $L/$m.txt
  grep -E "window_s|no sign-in|board_used" $L/$m.txt | tr '\n' ' '; echo
done
clean
