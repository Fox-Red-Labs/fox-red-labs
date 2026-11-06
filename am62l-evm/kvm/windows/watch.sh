#!/bin/sh
# watch.sh SECONDS INTERVAL: on the board host, screendump every INTERVAL s (named by seconds since t0)
# and log QEMU CPU time, RSS, disk bytes read/written and host free RAM to watch.log
D=/root/frl/kvm/win; T0=$(cut -d. -f1 $D/t0); END=$(( $(date +%s) + $1 ))
while [ $(date +%s) -lt $END ]; do
  P=$(pgrep -f qemu-system-aarch64 | head -1); [ -z "$P" ] && { echo "$(( $(date +%s)-T0 )) qemu gone" >> $D/watch.log; break; }
  S=$(( $(date +%s)-T0 ))
  docker exec frl-kvm /w/win/shot.sh t$(printf %05d $S) 2>/dev/null
  echo "$S cpu=$(ps -o times= -p $P) rss=$(ps -o rss= -p $P) $(grep -E '^(read_bytes|write_bytes)' /proc/$P/io | tr '\n' ' ') memavail=$(awk '/MemAvailable/{print $2}' /proc/meminfo) swap=$(awk '/SwapFree/{print $2}' /proc/meminfo)" >> $D/watch.log
  sleep $2
done
