#!/usr/bin/env python3
"""Replay a recorded IWRL6432 UART capture through the LP-AM13E230 backchannel and compare what the board prints with the
Mac parser (iwrl6432boost/tools/tlv_parse.py) run on the same bytes.
   replay.py <capture.bin> [--port /dev/cu.usbmodemAL1300011] [--baud 115200] [--idle 2.0]
Start it after the board is flashed (the board prints RADARPARSE READY until the first byte arrives). Exit 0 when every
frame line matches. The board's line is: F<frame> L<len> O<objs> T<ntlv> P<byte0>,<zone0> N<points> R<nearest mm>."""
import argparse, math, os, select, struct, sys, termios, time, tty
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "iwrl6432boost", "tools"))
import tlv_parse

def reference(buf):
    lines = []
    for hd, tlvs in tlv_parse.frames(buf):
        b0 = z0 = -1; npts = 0; r2min = None
        for t, n, p in tlvs:
            if t == 315:
                if len(p) >= 1: b0 = p[0]
                if len(p) >= 2: z0 = p[1]
            if t == 1:
                k = len(p) // 16; npts += k
                for i in range(k):
                    x, y, z, _ = struct.unpack_from("<4f", p, 16 * i)
                    a = np.array([x, y, z], dtype=np.float32)
                    r2 = a[0] * a[0] + a[1] * a[1] + a[2] * a[2]
                    r2min = r2 if r2min is None or r2 < r2min else r2min
        rm = int(np.sqrt(r2min) * np.float32(1000) + np.float32(0.5)) if npts else -1
        lines.append("F%d L%d O%d T%d P%d,%d N%d R%d" % (hd["frame"], hd["length"], hd["objs"], hd["ntlv"], b0, z0, npts, rm))
    return lines

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("capture"); ap.add_argument("--port", default="/dev/cu.usbmodemAL1300011")
    ap.add_argument("--baud", type=int, default=115200); ap.add_argument("--idle", type=float, default=2.0)
    ap.add_argument("--truth", help="synthetic.json: also check the board lines against what the generator put in each frame")
    a = ap.parse_args()
    data = open(a.capture, "rb").read(); want = reference(data)
    fd = os.open(a.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd); t = termios.tcgetattr(fd); t[4] = t[5] = getattr(termios, "B%d" % a.baud); termios.tcsetattr(fd, termios.TCSANOW, t)
    rx = b""; t0 = time.time()
    while b"READY" not in rx and time.time() - t0 < 10:
        if select.select([fd], [], [], 0.1)[0]: rx += os.read(fd, 256)
    if b"READY" not in rx: print("no RADARPARSE READY banner in 10 s"); sys.exit(2)
    rx = b""; sent = 0; t1 = time.time(); last = time.time()
    while True:
        r, w, _ = select.select([fd], [fd] if sent < len(data) else [], [], 0.05)
        if r:
            c = os.read(fd, 1024); rx += c; last = time.time()
        if w and sent < len(data):
            sent += os.write(fd, data[sent:sent + 64])
        if sent >= len(data) and time.time() - last > a.idle: break
    t2 = time.time()
    got = [l.strip() for l in rx.decode("latin1").split("\n") if l.strip().startswith("F")]
    ok = got == want
    print("sent %d bytes in %.1f s; board printed %d frame lines, Mac parser finds %d; %s" % (len(data), t2 - t1 - a.idle, len(got), len(want), "ALL MATCH" if ok else "MISMATCH"))
    if not ok:
        for i, (g, w_) in enumerate(zip(got, want)):
            if g != w_: print(" line", i, "board:", g, "| mac:", w_); break
        if len(got) != len(want): print(" counts differ")
    if a.truth:
        import json
        tr = json.load(open(a.truth)); bad = 0
        for l, t in zip(got, tr):
            p = l.split()
            bad += not (p[0] == "F%d" % t["frame"] and p[2] == "O%d" % t["objs"] and p[5] == "N%d" % t["points"] and p[6] == "R%d" % t["nearest_mm"])
        print("against the generator's own record: %d of %d frames match" % (len(tr) - bad + (0 if len(got) == len(tr) else 0), len(tr)))
        ok = ok and bad == 0 and len(got) == len(tr)
    for l in got[:3] + got[-2:]: print(" ", l)
    sys.exit(0 if ok else 1)
if __name__ == "__main__":
    main()
