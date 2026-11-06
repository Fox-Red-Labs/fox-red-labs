#!/usr/bin/env python3
"""Board-to-board BLE run: back up the CC2340R5, flash the central, capture its UART while it finds
"Fox Red 1354" (the CC1354P10) and toggles its LED, then restore the CC2340R5's previous image.
Usage: run_b2b.py <central .hex> <backup dir> <log file> [seconds]"""
import sys, time, pathlib, threading
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "portability/crossbuild"))
import run_cc23 as r

hexfile, bdir, log = sys.argv[1], pathlib.Path(sys.argv[2]), sys.argv[3]
secs = float(sys.argv[4]) if len(sys.argv) > 4 else 45
bdir.mkdir(parents=True, exist_ok=True)
main_bin, ccfg_bin, restore = bdir / "main_flash.bin", bdir / "ccfg.bin", bdir / "restore.hex"
r.read_mem(0x0, 0x80000, main_bin); r.read_mem(0x4E020000, 0x800, ccfg_bin)
assert main_bin.read_bytes()[:8] not in (b"\xff" * 8, b"\x00" * 8), "backup looks blank"
restore.write_text(r.ihex([(0x0, main_bin.read_bytes()), (0x4E020000, ccfg_bin.read_bytes())]))
print("backup ok")
try:
    (ok, fsecs, flog), text = r.uart_during(lambda: r.flash(hexfile), secs)
    open(log, "w").write(text)
    print("flashed" if ok else "FLASH FAILED " + flog, fsecs, "s")
    print("\n".join(l for l in text.replace("\r", "\n").split("\n") if "FOXRED" in l))
finally:
    ok, fsecs, flog = r.flash(restore)
    print("restore:", "ok" if ok else "FAILED " + flog, fsecs, "s")
