#!/usr/bin/env python3
"""Live bridge for the thermostat: radar USB port in, G3519 USB serial port out (the Mac stands in for the radar's wire).
  bridge.py <radar port> <g3519 port> <out.csv> [--seconds N] [--cfg file.cfg]
Needs pyserial (~/.venvs/csi/bin/python). With --cfg it first sends the cfg with iwrl6432boost/tools/send_cfg.py (the prebuilt demo needs it at every boot).
Then it reads radar bytes, cuts them into frames at the magic word and the header length, forwards each whole frame to the G3519
unchanged, and logs per frame: seq (the radar's frame number, which the G3519 echoes in SHOWN), t_frame (Mac time when the last byte of the
frame arrived), presence (TLV 315 zone bits), t_shown and dev_lat_us (from the G3519's SHOWN line). Frames the radar sent that the G3519 never
showed are the drops. Writes <out.csv>.log with every G3519 line."""
import sys, os, struct, time, threading, subprocess
import serial
MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
def presence(f):
    n = struct.unpack_from("<I", f, 32)[0]; p = 40
    for _ in range(min(n, 8)):
        if p + 8 > len(f): return -1
        t, l = struct.unpack_from("<II", f, p)
        if t == 315 and p + 10 <= len(f):
            z, bits = f[p + 8], f[p + 9]; zz = min(z, 4)
            return 1 if zz and (bits & ((1 << (2 * zz)) - 1)) else 0
        p += 8 + l
    return -1
def main():
    a = sys.argv[1:]; rp, gp, out = a[0], a[1], a[2]
    secs = float(a[a.index("--seconds") + 1]) if "--seconds" in a else 600
    if "--cfg" in a:
        here = os.path.dirname(os.path.abspath(__file__))
        subprocess.run([sys.executable, os.path.join(here, "..", "..", "iwrl6432boost", "tools", "send_cfg.py"), rp, a[a.index("--cfg") + 1]], check=True)
    r = serial.Serial(rp, 115200, timeout=0.05); g = serial.Serial(gp, 115200, timeout=0)
    rows = {}; log = open(out + ".log", "w"); stop = [False]
    def greader():
        buf = b""
        while not stop[0]:
            d = g.read(4096)
            if not d: time.sleep(0.002); continue
            now = time.time(); buf += d
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1); s = line.decode("latin1").strip(); log.write("%.6f %s\n" % (now, s))
                if s.startswith("SHOWN,"):
                    p = s.split(",")
                    try:
                        q = int(p[1])
                        if q in rows: rows[q]["t_shown"] = now; rows[q]["dev"] = int(p[3])
                    except (ValueError, IndexError): pass
    threading.Thread(target=greader, daemon=True).start()
    buf = b""; t_end = time.time() + secs; n = 0
    while time.time() < t_end:
        buf += r.read(4096)
        while True:
            i = buf.find(MAGIC)
            if i < 0: buf = buf[-7:]; break
            if len(buf) < i + 40: buf = buf[i:]; break
            ln = struct.unpack_from("<I", buf, i + 12)[0]
            if ln < 40 or ln > 65536: buf = buf[i + 8:]; continue
            if len(buf) < i + ln: buf = buf[i:]; break
            f = buf[i:i + ln]; buf = buf[i + ln:]; t = time.time()
            seq = struct.unpack_from("<I", f, 20)[0]
            g.write(f); n += 1
            rows[seq] = {"t_frame": t, "pres": presence(f), "t_shown": None, "dev": None}
    time.sleep(1.0); stop[0] = True
    with open(out, "w") as o:
        o.write("seq,t_frame,presence,t_shown,dev_lat_us\n")
        for q in sorted(rows):
            x = rows[q]; o.write("%d,%.6f,%d,%s,%s\n" % (q, x["t_frame"], x["pres"], "%.6f" % x["t_shown"] if x["t_shown"] else "", x["dev"] if x["dev"] is not None else ""))
    shown = sum(1 for x in rows.values() if x["t_shown"])
    print("forwarded %d frames in %.0f s (%.2f per s), %d shown, %d not shown" % (n, secs, n / secs, shown, n - shown))
main()
