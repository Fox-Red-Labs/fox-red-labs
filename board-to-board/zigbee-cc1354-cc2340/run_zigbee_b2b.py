#!/usr/bin/env python3
"""Zigbee board to board, no home network: LP-CC1354P10-1 as coordinator (TI zc_light, patched to form and
open its network at boot), LP-EM-CC2340R5 as router (TI onoff_light, all 16 channels). Captures both serial
ports, then writes the CC2340R5's previous image back.
Usage: run_zigbee_b2b.py <coordinator .out, or - to skip> <router .hex> <cc2340r5 backup dir> <out dir> [seconds]"""
import sys, time, pathlib, threading
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "portability/crossbuild"))
import run_cc23 as r

coord, router, bdir, odir = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3]), pathlib.Path(sys.argv[4])
secs = float(sys.argv[5]) if len(sys.argv) > 5 else 120
odir.mkdir(parents=True, exist_ok=True); bdir.mkdir(parents=True, exist_ok=True)
CC23 = r.CCXML
CC13 = pathlib.Path(__file__).resolve().parents[2] / "cc1354p10/CC1354P10.ccxml"

stop = threading.Event()
def listen(port, name):
    import os, select, termios
    fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    with open(odir / f"{name}.uart.log", "w") as f:
        while not stop.is_set():
            rr, _, _ = select.select([fd], [], [], 0.3)
            if rr:
                d = os.read(fd, 4096)
                if d: f.write(f"{time.time():.3f} {d!r}\n"); f.flush()
    os.close(fd)
threads = [threading.Thread(target=listen, args=("/dev/cu.usbmodemLS45013A1", "cc1354_coordinator")),
           threading.Thread(target=listen, args=("/dev/cu.usbmodemLS470GTU1", "cc2340r5_router"))]
for t in threads: t.start()
time.sleep(0.5)
try:
    r.CCXML = CC13
    if coord != "-":   # "-": coordinator already flashed and started by a power cycle (a DSLite flash does not start it on the CC1354)
        ok, s, log = r.flash(coord); print("coordinator flash", ok, s, "s", "" if ok else log)
    time.sleep(20)
    r.CCXML = CC23
    r.read_mem(0x0, 0x80000, bdir / "main_flash.bin"); r.read_mem(0x4E020000, 0x800, bdir / "ccfg.bin")
    assert (bdir / "main_flash.bin").read_bytes()[:8] not in (b"\xff" * 8, b"\x00" * 8)
    (bdir / "restore.hex").write_text(r.ihex([(0x0, (bdir / "main_flash.bin").read_bytes()), (0x4E020000, (bdir / "ccfg.bin").read_bytes())]))
    print("cc2340r5 backup ok")
    ok, s, log = r.flash(router); print("router flash", ok, s, "s", "" if ok else log)
    t_router = time.time(); print("router started", time.strftime("%H:%M:%S"))
    time.sleep(secs)
finally:
    r.CCXML = CC23
    if (bdir / "restore.hex").exists():
        ok, s, log = r.flash(str(bdir / "restore.hex")); print("cc2340r5 restore", ok, s, "s")
    stop.set()
    for t in threads: t.join()
