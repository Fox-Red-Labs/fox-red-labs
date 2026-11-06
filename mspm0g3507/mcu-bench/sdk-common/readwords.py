#!/usr/bin/env python3
"""Read N 32-bit words from RAM on probe MG350001 after waiting.
  readwords.py <hex addr> <count> [wait_ms]   -> prints the raw OpenOCD lines, then one JSON list
Note: OpenOCD 'init' on this setup restarts the program, so wait_ms must cover the whole run."""
import json, re, subprocess, sys, os
addr, n = sys.argv[1], int(sys.argv[2]); wait = sys.argv[3] if len(sys.argv) > 3 else "3000"
ocd = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ocd.sh")
out = subprocess.run([ocd, f"sleep {wait}", "halt", f"mdw {addr} {n}", "resume"], capture_output=True, text=True).stdout
lines = [l for l in out.splitlines() if re.match(r"^0x[0-9a-f]+:", l)]
print("\n".join(lines))
print(json.dumps([int(w, 16) for l in lines for w in l.split(":")[1].split()]))
