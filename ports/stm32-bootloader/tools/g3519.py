#!/usr/bin/env python3
"""Board helpers for the stm32-bootloader check on the LP-MSPM0G3519 (probe MG000001), all under the probe lock.

  g3519.py erase          erase the application area 0x00010000..0x0007FFFF (main flash only; never the bootloader, data flash or NONMAIN)
  g3519.py dump N OUT     read N bytes at 0x00010000 into OUT
  g3519.py reset          pulse nRST from the probe (restart the program)
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "tools", "ai_runs"))
try:
    import portboard as p
except ImportError:
    sys.exit("skipped: needs the lab bench helper portboard.py (not in this copy)")
APP, END = 0x10000, 0x80000
def main(a):
    if a[0] == "erase":
        r = p.ocd_cmds("g3519", ["reset halt", f"flash erase_address 0x{APP:x} 0x{END - APP:x}", "reset run"])
        return 0 if "erased address" in r.stdout + r.stderr else 1
    if a[0] == "dump":
        r = p.ocd_cmds("g3519", ["reset halt", f"dump_image {a[2]} 0x{APP:x} {int(a[1])}", "reset run"])
        return 0 if os.path.exists(a[2]) else 1
    if a[0] == "reset":
        return 0 if p.run("g3519") else 1
    return 2
sys.exit(main(sys.argv[1:]))
