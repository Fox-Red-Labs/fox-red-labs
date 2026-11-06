#!/bin/bash
# Image E (DDR) in Renode at EL1 (default) or EL2 (EL=2): runs run-ring-ddr.resc and decodes the ring. Usage: [EL=2] run-ring-el.sh [ELF]. Needs renode on PATH.
set -u
cd "$(dirname "$0")"
ELF="${1:-${ZP:-$HOME/zephyr-am62l}/build-ddr-core1/zephyr/zephyr.elf}"
EL2=false; [ "${EL:-1}" = 2 ] && EL2=true
renode --console --disable-xwt -e "\$el2=$EL2; set elf @$ELF; include @run-ring-ddr.resc" </dev/null 2>&1 | perl -ne 'print if /RING|0x/ || /Error|No such/' | python3 -c '
import sys,re
t=sys.stdin.read()
m=t.split("RING bytes:")
print(m[0].strip()[-200:])
b=bytes(int(x,16) for x in re.findall(r"0x([0-9A-Fa-f]{2}),",m[1])) if len(m)>1 else b""
print(b.split(b"\0\0")[0].decode(errors="replace"))'
