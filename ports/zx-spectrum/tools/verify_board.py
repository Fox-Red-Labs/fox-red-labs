#!/usr/bin/python3
"""Compare the board's 6,912 screen bytes with the host machine's after the same number of frames (deterministic: no input).
  verify_board.py ROM.bin FRAMES            ROM.bin is the image tools/mkrom.py wrote ($OUT/rom.bin); the board image was built with STOP_AFTER=FRAMES
Under the probe lock: reset, wait until the board logs 'zx stopped' (it spins at a frame boundary), halt, dump, resume."""
import os, subprocess, sys, tempfile, time
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "touch-games" / "tools")); sys.path.insert(0, str(HERE.parents[2] / "tools" / "frl"))
import g3519_lock_run as g
from lock import ProbeLock
rom, frames = sys.argv[1], sys.argv[2]
elf = Path(sys.argv[3]) if len(sys.argv) > 3 else Path.cwd() / "out/zx-spectrum/g3519/zx-g3519.out"
tmp = Path(tempfile.mkdtemp()); host = tmp / "dump"
subprocess.run(["cc", "-O2", "-I" + str(HERE.parents[2] / "third-party/chips"), "-o", str(host), str(HERE.parent / "host/dump.c"), str(HERE.parent / "src/zx.c")], check=True)
r = subprocess.run([str(host), rom, frames, str(tmp / "host.bin")], capture_output=True, text=True).stdout
off = int(r.split("ram_offset=")[1].split()[0])
nm = subprocess.run([os.environ.get("NM", "tiarmnm"), str(elf)], capture_output=True, text=True).stdout
addr = int([l for l in nm.splitlines() if l.endswith(" zx")][0].split()[0], 16)
secs = float(sys.argv[4]) if len(sys.argv) > 4 else 60
with ProbeLock(g.SERIAL, "zx-spectrum", "verify screen bytes") as lk:
    g.ocd(["reset run"]); time.sleep(secs)
    out = g.ocd(["halt", f"dump_image {tmp}/board.bin 0x{addr + off:08X} 6912", "resume"])
a, b = (tmp / "host.bin").read_bytes(), (tmp / "board.bin").read_bytes()
d = sum(x != y for x, y in zip(a, b))
print("MATCH (6912 bytes equal)" if d == 0 and len(a) == len(b) else f"DIFF {d} bytes", "|", r.strip())
