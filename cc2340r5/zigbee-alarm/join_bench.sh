#!/bin/bash
# Join timing: remove the test device from ZHA, open permit-join, flash, then poll until the smoke and CO entities exist.
# Prints seconds from permit-join (t_permit), from the end of the flash (t_flash), to: ZHA lists the device, both entities exist.
cd "$(dirname "$0")"; PY=~/.venvs/findmy/bin/python; IEEE=00:12:4b:xx:xx:xx:xx:xx
python3 ../../tools/ha.py call zha remove "{\"ieee\":\"$IEEE\"}" >/dev/null; sleep 6
T0=$(python3 -c 'import time;print(time.time())')
python3 ../../tools/ha.py call zha permit '{"duration":240}' >/dev/null
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c ../CC2340R5.ccxml -e -f -u freertos/ticlang/alarm.hex > flash.txt 2>&1
T1=$(python3 -c 'import time;print(time.time())')
tail -1 flash.txt
dev=""; ent=""
for i in $(seq 1 240); do
  now=$(python3 -c 'import time;print(time.time())')
  if [ -z "$dev" ] && $PY ha_ws.py zha | grep -q "$IEEE"; then dev=$now; fi
  n=$(python3 ../../tools/ha.py states binary_sensor | grep -c "frl_alarm_smoke\|FRL-ALARM Smoke\|FRL-ALARM Carbon")
  if [ "$n" -ge 2 ]; then ent=$now; break; fi
  sleep 0.3
done
python3 - <<PY
t0,t1,d,e=$T0,$T1,float("${dev:-0}" or 0),float("${ent:-0}" or 0)
print("flash %.1f s; device listed %.1f s after permit (%.1f after flash end); both entities %.1f s after permit (%.1f after flash end)"%(t1-t0,d-t0,d-t1,e-t0,e-t1))
PY
