#!/usr/bin/env python3
"""Flash an image to the LP-MSPM0C1104 (probe MC110001) under the probe lock, run it, read a result struct.

    c1104_run.py <image.out> <image.map> <symbol> <words> [seconds]     e.g. hello.out hello.map hello_res 16 8

Steps inside the lock (one OpenOCD session; attaching again resets the chip): first time only, back the whole 16 kB flash up twice and compare; program and verify (no mass
erase, the boot configuration area is never written); reset; wait; read <words> 32-bit words at <symbol> live; run on.
Backups go to out/boosterpack-speedrun/sharp128/c1104-backup/.
"""
import os, re, subprocess, sys, time, hashlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools", "frl"))
from lock import ProbeLock  # noqa: E402

OCD = os.path.expanduser("~/zephyr-toolchain/xpack-openocd-0.12.0-7")
SER = "MC110001"
BK = os.path.abspath("out/boosterpack-speedrun/sharp128/c1104-backup")


def ocd(*cmds, speed=2500):
    a = [OCD + "/bin/openocd", "-s", OCD + "/openocd/scripts", "-c", "gdb port disabled", "-c", "tcl port disabled",
         "-c", "telnet port disabled", "-f", "interface/xds110.cfg", "-c", f"adapter serial {SER}", "-c", f"adapter speed {speed}",
         "-f", "target/ti_mspm0.cfg", "-c", "init"]
    for c in cmds:
        a += ["-c", c]
    a += ["-c", "shutdown"]
    p = subprocess.run(a, capture_output=True, text=True, timeout=60)
    return p.stdout + p.stderr


def main():
    img, mp, sym, words = sys.argv[1:5]
    secs = float(sys.argv[5]) if len(sys.argv) > 5 else 8
    m = re.search(r"^([0-9a-f]{8})\s+" + re.escape(sym) + r"\s*$", open(mp).read(), re.M)
    if not m:
        sys.exit(f"{sym} not in {mp}")
    addr = m.group(1)
    os.makedirs(BK, exist_ok=True)
    with ProbeLock(SER, "sharp128-c1104", "flash and read") as lk:
        if not os.path.exists(BK + "/a.bin"):
            ocd("reset halt", f"dump_image {BK}/a.bin 0x0 0x4000")
            ocd("reset halt", f"dump_image {BK}/b.bin 0x0 0x4000")
            a, b = open(BK + "/a.bin", "rb").read(), open(BK + "/b.bin", "rb").read()
            if a != b or len(a) != 0x4000:
                sys.exit("backup reads differ or are short; stopping before any write")
            os.replace(BK + "/b.bin", BK + "/flash-before-sharp-" + hashlib.sha256(a).hexdigest()[:8] + ".bin")
            print("backed up 16 kB, sha256", hashlib.sha256(a).hexdigest()[:16])
        # One OpenOCD session for flash, run and read: attaching again would reset the chip (it did, once).
        t0 = time.time()
        out = ocd("reset halt", f"program {img} verify", "reset run", f"sleep {int(secs * 1000)}", f"mdw 0x{addr} {words}")
        print(f"flash+run+read {time.time() - t0:.1f} s;", "verified" if "Verified OK" in out else "NOT VERIFIED")
        print(f"{sym} at 0x{addr}:")
        vals = []
        for line in out.splitlines():
            mm = re.match(r"0x([0-9a-f]+):((?:\s+[0-9a-f]{8})+)", line)
            if mm:
                vals += [int(x, 16) for x in mm.group(2).split()]
        print(" ".join(str(v) for v in vals))
        print(out[-300:] if not vals else "")


main()
