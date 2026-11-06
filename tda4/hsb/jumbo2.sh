#!/bin/sh
# Rerun of the sensor-bridge sender's cost with the achieved frame rate recorded over the same window as perf (2026-10-10).
# The first run (jumbo_test.sh) did not print the rate during its perf window, so its per-packet figure assumed 60 fps.
cd /root/hsb; L=/root/night/jumbo2.log; : > $L
maxt(){ sort -n /sys/class/thermal/thermal_zone*/temp | tail -1; }; while [ $(maxt) -gt 80000 ]; do sleep 5; done
pkill -9 serve_frames; sleep 1
ip link show hsb0 >/dev/null 2>&1 || ip link add hsb0 type dummy
ip link set hsb0 up
addrs(){ ip -4 addr show hsb0 | grep -q "inet 192\.0\.2\.2/24 " || ip addr add 192.0.2.2/24 brd + dev hsb0; ip -4 addr show hsb0 | grep -q "inet 192\.0\.2\.1/24 " || ip addr add 192.0.2.1/24 brd + dev hsb0;
  [ "$(ip -4 addr show hsb0 | grep -c "inet 192\.0\.2\.[12]/24 ")" -eq 2 ] || { echo "hsb0 addresses missing; stopping" >> $L; pkill -9 serve_frames; exit 1; }; }
sysctl -w net.core.rmem_max=134217728 >/dev/null
for mtu in 1500 9000; do
  pkill -9 serve_frames; sleep 1; ip link set hsb0 mtu $mtu; sleep 1; addrs; echo "MTU $mtu addresses: $(ip -4 addr show hsb0 | grep -c inet)" >> $L
  setsid nohup ./serve_frames 192.0.2.2 frames1080.raw 2073600 --fps 60 > serve2_mtu$mtu.log 2>&1 &
  sleep 2
  timeout 40 python3 hsb_host_check.py --host-ip 192.0.2.1 --port 50000 --frame-bytes 2073600 --frames 3 --mtu $mtu --source frames1080.raw 2>&1 | grep -E 'RESULT' | cut -c1-200 >> $L
  sleep 3; P=$(pgrep -n serve_frames)
  ( while kill -0 $P 2>/dev/null; do [ $(maxt) -gt 90000 ] && { echo "THERMAL STOP $(maxt)" >> $L; kill -9 $P; }; sleep 1; done ) &
  echo "MTU $mtu: serve log before perf: $(grep -a 'frames sent' serve2_mtu$mtu.log | tail -1)" >> $L
  perf stat -e task-clock,cycles,instructions -p $P -- sleep 10 >> $L 2>&1
  echo "MTU $mtu: serve log after perf: $(grep -a 'frames sent' serve2_mtu$mtu.log | tail -1)" >> $L
  echo "therm $(maxt)" >> $L
done
pkill -9 serve_frames; ip link set hsb0 mtu 1500; echo JUMBO2_DONE >> $L
