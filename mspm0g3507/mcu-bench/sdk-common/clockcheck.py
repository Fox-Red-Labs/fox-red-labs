#!/usr/bin/env python3
"""Check the CPU clock of a running bench build against the Mac's clock.

  clockcheck.py <hex address of bench_wraps> [seconds]

Reads the SysTick wrap counter and SysTick VAL twice, N seconds apart, inside one
OpenOCD session (the target keeps running), and prints cycles per second. Also
decodes SYSCTL CLKSTATUS (0x400B0204). Accuracy is limited by OpenOCD command
latency, roughly 1 percent over 10 seconds: enough to tell 32 MHz from 80 MHz.
"""
import re, subprocess, sys, os
addr = sys.argv[1]; secs = float(sys.argv[2]) if len(sys.argv) > 2 else 10
ocd = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ocd.sh")
out = subprocess.run([ocd, "sleep 2000", f"mdw {addr}", "mdw 0xE000E018", f"sleep {int(secs*1000)}",
                      f"mdw {addr}", "mdw 0xE000E018", "mdw 0x400B0204"], capture_output=True, text=True).stdout
vals = [int(v, 16) for v in re.findall(r"^0x[0-9a-f]+: ([0-9a-f]+)", out, re.M)]
w1, v1, w2, v2, cs = vals
c1 = (w1 << 24) + (0xFFFFFF - v1); c2 = (w2 << 24) + (0xFFFFFF - v2)
print(f"wraps {w1}->{w2}, cycles {c1}->{c2}, {secs:.0f} s host sleep: {(c2 - c1) / secs / 1e6:.2f} MHz (upper bound; OpenOCD latency adds to the interval)")
print(f"CLKSTATUS 0x{cs:08x}: MCLK from {'HSCLK' if cs & 0x10 else 'SYSOSC'}; SYSOSC freq code {cs & 3} (0 = 32 MHz); "
      f"SYSPLL {'good' if cs & 0x200 else 'off' if cs & 0x4000 else 'not good'}; HSCLK {'good' if cs & 0x200000 else 'off' if cs & 0x1000 else 'not good'}; "
      f"CURHSCLKSEL {'HFCLK' if cs & 0x10000 else 'SYSPLL'}")
