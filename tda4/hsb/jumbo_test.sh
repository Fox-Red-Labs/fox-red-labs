#!/bin/sh
# cost of the HSB emulator at 1080p60 with MTU 1500 and 9000, by /proc ticks and perf; output in /root/hsb/jumbo.log
cd /root/hsb; : > jumbo.log
pkill -9 serve_frames; sleep 1
ip link show hsb0 >/dev/null 2>&1 || ip link add hsb0 type dummy
ip addr flush dev hsb0; ip addr add 192.0.2.2/24 brd + dev hsb0; ip addr add 192.0.2.1/24 brd + dev hsb0; ip link set hsb0 up
sysctl -w net.core.rmem_max=134217728 >/dev/null
for mtu in 1500 9000; do
  pkill -9 serve_frames; sleep 1; ip link set hsb0 mtu $mtu
  setsid nohup ./serve_frames 192.0.2.2 frames1080.raw 2073600 --fps 60 > serve_mtu$mtu.log 2>&1 &
  sleep 2; head -2 serve_mtu$mtu.log >> jumbo.log
  timeout 40 python3 hsb_host_check.py --host-ip 192.0.2.1 --port 50000 --frame-bytes 2073600 --frames 3 --mtu $mtu --source frames1080.raw 2>&1 | grep -E 'RESULT|configured' | cut -c1-220 >> jumbo.log
  sleep 1; P=$(pgrep -n serve_frames); T0=$(awk '{print $14+$15}' /proc/$P/stat); sleep 10; T1=$(awk '{print $14+$15}' /proc/$P/stat)
  echo "MTU $mtu 1080p60: serve_frames $((T1-T0))% of one A72 over 10 s (pid $P)" >> jumbo.log
  perf stat -e task-clock,cycles,instructions -p $P -- sleep 10 >> jumbo.log 2>&1
done
pkill -9 serve_frames; ip link set hsb0 mtu 1500; echo JUMBO_DONE >> jumbo.log
