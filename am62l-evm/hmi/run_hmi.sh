#!/bin/sh
# run_hmi.sh : three 30 s measured runs per config; waits for load < 0.3 and no containers before each run
cd /root/hmi
settle() { i=0; while :; do l=$(cut -d' ' -f1 /proc/loadavg); n=$(docker ps -q | wc -l)
  if [ "$n" = 0 ] && awk "BEGIN{exit !($l<0.3)}"; then return; fi; i=$((i+1)); [ $i -gt 40 ] && { echo "UNSETTLED load=$l containers=$n"; return; }; sleep 6; done; }
run() { name=$1; shift; for r in 1 2 3; do settle; echo "free_mb=$(free -m | awk '/Mem/{print $7}')"; python3 meas.py ${name}_$r 30 "$@"; sleep 3; done; }
which=$1
case $which in
 lvgl-fbdev) run lvgl-fbdev ./scene fbdev 45 ;;
 lvgl-drm)   run lvgl-drm ./scene drm 45 ;;
 slint)      export SLINT_BACKEND=linuxkms-software; export PROC=slint-viewer
   for r in 1 2 3; do settle; python3 meas.py slint_$r 30 strace -f --seccomp-bpf -tt -e trace=ioctl -o st_slint_$r.log slint-viewer scene.slint; sleep 3; done ;;
 lvgl-drm-strace) export PROC=scene
   for r in 1 2 3; do settle; python3 meas.py lvgldrmst_$r 30 strace -f --seccomp-bpf -tt -e trace=ioctl -o st_lvgldrm_$r.log ./scene drm 45; sleep 3; done ;;
esac
