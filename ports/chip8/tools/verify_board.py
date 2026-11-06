#!/usr/bin/python3
"""Compare the board's CHIP-8 frame buffer with the host interpreter's screen for the same ROM.
  verify_board.py ROM_INDEX ROM_FILE [platform] [settle_secs]
Under the probe lock: reset the LP-MSPM0G3519, wait, halt, dump the 2,048-byte frame buffer (0x20201000) and the run counters, resume.
The ROM index is the one the image booted with (BOOT_ROM). Prints MATCH or the number of differing pixels, and the instruction rate."""
import subprocess, sys, time, tempfile, os
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "touch-games" / "tools")); sys.path.insert(0, str(HERE.parents[2] / "tools" / "frl"))
import g3519_lock_run as g
from lock import ProbeLock
rom = sys.argv[2]; plat = sys.argv[3] if len(sys.argv) > 3 else "0"; settle = float(sys.argv[4]) if len(sys.argv) > 4 else 6
host = os.environ.get("C8HOST", str(HERE.parent / "host" / "c8host"))
subprocess.run(["cc", "-O1", "-o", host, str(HERE.parent / "host/test_host.c"), str(HERE.parent / "src/chip8.c")], check=True)
ref = subprocess.run([host, rom, plat, "900", "12"], capture_output=True, text=True).stdout.splitlines()[:32]
with ProbeLock(g.SERIAL, "chip8", "verify frame buffer") as lk:
    tmp = tempfile.mktemp(suffix=".bin")
    out = g.ocd(["reset run", f"sleep {int(settle * 1000)}", "halt", f"dump_image {tmp} 0x20201000 2048", "mdw 0x202024e0 8", "resume"])
data = open(tmp, "rb").read()
words = [l for l in out.splitlines() if l.startswith("0x202024e0")]
diff = 0
for y in range(32):
    for x in range(64):
        a = data[y * 64 + x]; b = 1 if ref[y][x] == "#" else 0
        diff += a != b
print("MATCH" if diff == 0 else f"DIFF {diff} px", "| counters:", words[0] if words else out[-200:])
