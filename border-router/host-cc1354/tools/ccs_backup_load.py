"""Back up the LP-CC1354P10-1's flash and CCFG, then load an image and run it, in one CCStudio scripting session.
A DSLite flash does not start a new program on this board until a power cycle; a CCS load does (cc1354p10/tools/
ccs_load_run.py). Backups go outside the repo (they can hold network keys).

Run with CCStudio's Python:
  run.sh ccs_backup_load.py <ccxml> <out dir> backup                 back up only, leave the program running
  run.sh ccs_backup_load.py <ccxml> <out dir> load <image.out>       back up, load, run
  run.sh ccs_backup_load.py <ccxml> <out dir> restore <restore.hex>  load a backup back, verify 4 KB per range, run
Writes <out dir>/restore.hex and backup_<addr>.bin; refuses to load anything if the backup looks blank."""
import os
import re
import struct
import sys
from pathlib import Path

from scripting import initScripting

RANGES = [(0x0, 1024 * 1024), (0x50000000, 1024)]  # flash, CCFG (tools/board_sweep/boards.json)


def ihex(chunks):
    def rec(a16, typ, data):
        b = bytes([len(data), a16 >> 8, a16 & 0xFF, typ]) + data
        return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()
    lines, upper = [], None
    for base, data in chunks:
        for off in range(0, len(data), 16):
            a = base + off
            if a >> 16 != upper:
                upper = a >> 16
                lines.append(rec(0, 4, upper.to_bytes(2, "big")))
            lines.append(rec(a & 0xFFFF, 0, data[off:off + 16]))
    return "\n".join(lines + [":00000001FF"]) + "\n"


def read_bytes(s, start, length):
    out = bytearray()
    for a in range(start, start + length, 4096):
        words = s.memory.read(a, min(1024, (start + length - a) // 4))
        out += b"".join(struct.pack("<I", w & 0xFFFFFFFF) for w in words)
    return bytes(out)


ccxml, out, mode = sys.argv[1], Path(sys.argv[2]), sys.argv[3]
image = sys.argv[4] if len(sys.argv) > 4 else None
out.mkdir(parents=True, exist_ok=True)

ds = initScripting()
ds.setScriptingTimeout(300000)
ds.configure(ccxml)
s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
# A sleeping low-power program (BLE or sleepy Zigbee) can lock the debugger out ("Operation was aborted",
# error -242). Pulsing the reset line right before the connect catches the chip awake in its first second.
if os.environ.get("FRL_RESET_PROBE"):
    import subprocess
    import threading
    XR = "/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset"
    probe = os.environ["FRL_RESET_PROBE"]
    delay = float(os.environ.get("FRL_RESET_RELEASE_S", "0.4"))
    # Hold the chip in reset, start the connect, release the reset while the connect is already under way.
    subprocess.run([XR, "-s", probe, "-a", "assert"])
    threading.Timer(delay, lambda: subprocess.run([XR, "-s", probe, "-a", "deassert"])).start()
# Catch the chip awake: retry the connect for a while (the user power-cycles or resets the board meanwhile).
tries = int(os.environ.get("FRL_CONNECT_TRIES", "1"))
for attempt in range(tries):
    try:
        s.target.connect()
        break
    except Exception as e:
        if attempt + 1 == tries:
            raise
        print("connect failed (%d/%d): %s" % (attempt + 1, tries, str(e)[-60:]), flush=True)
s.target.halt()

if mode in ("backup", "load"):
    backup = [(start, read_bytes(s, start, length)) for start, length in RANGES]
    if backup[0][1][:8] in (b"\xff" * 8, b"\x00" * 8):
        s.target.run(False)
        sys.exit("backup looks blank: refusing to load anything")
    (out / "restore.hex").write_text(ihex(backup))
    for start, data in backup:
        (out / f"backup_{start:#x}.bin").write_bytes(data)
    print("backup ok", out, flush=True)

if mode == "load":
    s.memory.loadProgram(image)
    print("loaded", image, flush=True)
elif mode == "restore":
    s.memory.loadProgram(image)
    ok = True
    for start, length in RANGES:
        want = (out / f"backup_{start:#x}.bin").read_bytes()[:4096]
        ok = ok and read_bytes(s, start, min(len(want), 4096)) == want
    print("restored and verified" if ok else "RESTORE DID NOT VERIFY", flush=True)

s.target.run(False)
print("running", flush=True)
s.target.disconnect()
ds.shutdown()
