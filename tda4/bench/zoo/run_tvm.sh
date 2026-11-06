#!/bin/sh
R=/root/night; cd /root/push/zoo
echo "=== tvm start $(date +%T) therm $(cat /sys/class/thermal/thermal_zone*/temp | tr '\n' ' ') load $(cut -d' ' -f1-3 /proc/loadavg)" > $R/zoo_tvm.log
python3 tvm_bench.py 30 >> $R/zoo_tvm.log 2>&1
echo "=== tvm end $(date +%T) rc=$?" >> $R/zoo_tvm.log
