#!/usr/bin/env python3
"""Flash an image to the LP-MSPM0G5187 with DSLite and capture the backchannel UART to a file.
Take the probe lock around it:
  python3 tools/frl/lock.py exec --serial MG020001 -- python3 portability/boosterpacks/bassensorsmkii/tools/flash_capture.py <image.out> <seconds> <out.txt>
No mass erase, no boot configuration write. The capture opens the port right after the flash returns, so the first boot text is kept."""
import subprocess, sys, time, os
import serial
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../.."))
DSLITE = os.environ.get("BENCH_DSLITE", "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite")
PORT = os.environ.get("BS_PORT", "/dev/cu.usbmodemMG0200011")
img, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
t0 = time.time()
r = subprocess.run([DSLITE, "flash", "-c", os.path.join(ROOT, "mspm0g5187/MSPM0G5187_MG020001.ccxml"), "-f", "-v", "-u", img],
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=240)
print("flash exit", r.returncode, "%.1f s" % (time.time() - t0), r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "")
if r.returncode != 0:
    print(r.stdout[-600:]); sys.exit(1)
s = serial.Serial(PORT, 115200, timeout=0.2)
buf = bytearray(); t1 = time.time()
while time.time() - t1 < secs:
    buf += s.read(4096)
open(out, "wb").write(buf)
sys.stdout.write(buf.decode(errors="replace")[:6000])
