#!/bin/sh
# On the board (host), kernel 6.18.38-ti-kvm (binderfs): start Redroid 12 (Android in a container, arm64-only
# image) and time it to sys.boot_completed=1. adb is published on the board's loopback only.
# Usage: sh redroid_boot.sh [seconds=600]   -> /root/frl/redroid/out/
T=${1:-600}; O=/root/frl/redroid/out; mkdir -p $O /root/frl/redroid/data
docker rm -f frl-redroid >/dev/null 2>&1
s=$(date +%s)
docker run -d --privileged --name frl-redroid -v /root/frl/redroid/data:/data -p 127.0.0.1:5555:5555 \
  redroid/redroid:12.0.0_64only-latest androidboot.redroid_gpu_mode=guest > $O/run.id 2>&1
peak=0; done=""
while [ $(( $(date +%s)-s )) -lt $T ]; do
  sleep 5; u=$(free -m | awk '/Mem/{print $3}'); [ $u -gt $peak ] && peak=$u
  docker ps -q -f name=frl-redroid | grep -q . || { echo "container exited"; break; }
  b=$(docker exec frl-redroid getprop sys.boot_completed 2>/dev/null)
  [ "$b" = 1 ] && { done=$(( $(date +%s)-s )); break; }
done
echo "boot_completed_s=${done:-none} board_used_peak_mb=$peak"
docker exec frl-redroid getprop ro.build.fingerprint 2>/dev/null
docker exec frl-redroid sh -c 'getprop ro.product.cpu.abilist; dumpsys window 2>/dev/null | grep -m1 mCurrentFocus' 2>/dev/null
docker logs frl-redroid 2>&1 | tail -5
dmesg | grep -iE "binder|redroid|ashmem" | tail -5
