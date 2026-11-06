#!/usr/bin/env python3
"""Feeder for the thermostat demo: the Mac stands in for the radar's wire and for the CC35X1 Wi-Fi result.
  feeder.py [--radar PORT] [--g3519 PORT] [--wifi CSV] [--fast-cfg FILE] [--log FILE] [--frames-csv FILE]
Every whole radar frame is forwarded unchanged to the G3519's USB serial port. Once a second the last row of the Wi-Fi log is sent as
`W,<cls>,<p>,<age_s>\\n`. It never sends sensorStop or a cfg while the radar streams (the demo takes one start per boot). When the radar
stream stops for 3 s and then the demo's banner comes back (someone pressed S2), it sends the fast-clear cfg once, line by line, and
carries on forwarding. Needs pyserial (~/.venvs/csi/bin/python). Logs events, a status line every 10 s, and per-frame
rows (seq, Mac time, held presence, points, SHOWN time and on-board latency) to the frames csv."""
import sys, os, time, struct, argparse, re, datetime
import serial
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "iwrl6432boost", "tools"))
from tlv_parse import frames
MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
day = datetime.date.today().isoformat()
ap = argparse.ArgumentParser()
ap.add_argument("--radar", default="/dev/cu.usbmodemRI32RI321"); ap.add_argument("--g3519", default="/dev/cu.usbmodemMG0000011")
ap.add_argument("--wifi", default=os.path.join(ROOT, "cc35x1", "csi", "results", "2026-10-10-freerun.csv"))
ap.add_argument("--fast-cfg", default=os.path.join(ROOT, "iwrl6432boost", "eval", "cfg", "PresenceDetect-fastclear.cfg"))
ap.add_argument("--log", default=os.path.join(HERE, "..", "results", f"feeder-{day}.log"))
ap.add_argument("--frames-csv", default=os.path.join(HERE, "..", "results", f"feeder-frames-{day}.csv"))
ap.add_argument("--no-apply", action="store_true")
a = ap.parse_args()
log = open(a.log, "a", buffering=1); fc = open(a.frames_csv, "a", buffering=1)
if fc.tell() == 0: fc.write("seq,t_frame,presence,pts301,t_shown,dev_lat_us\n")
def L(*x): log.write("%s %s\n" % (datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], " ".join(str(i) for i in x)))
L("feeder start radar", a.radar, "g3519", a.g3519, "wifi", a.wifi, "fast_cfg", a.fast_cfg, "apply", not a.no_apply)
r = serial.Serial(a.radar, 115200, timeout=0.02)
g = None
def open_g():
    global g
    try: g = serial.Serial(a.g3519, 115200, timeout=0, write_timeout=0.3)
    except Exception as e: g = None; L("g3519 port open failed", repr(e)[:80])
open_g()
buf = b""; gbuf = b""; last_rx = time.time(); had_frames = False; state = "stream"; applied = False
nfr = ndrop = nw = 0; last_w = 0.0; last_stat = time.time(); rows = {}; pres_prev = None
def wline():
    try:
        with open(a.wifi, "rb") as f:
            f.seek(0, 2); n = f.tell(); f.seek(max(0, n - 512)); last = f.read().strip().split(b"\n")[-1].decode("latin1").split(",")
        if len(last) >= 4 and last[0] != "t": return f"W,{last[2]},{last[3]},{max(0.0, time.time() - float(last[0])):.1f}\n".encode()
    except Exception: pass
    return None
def gwrite(b):
    global g, ndrop
    if g is None: open_g()
    if g is None: ndrop += 1; return
    try: g.write(b)
    except Exception as e: ndrop += 1; L("g3519 write failed", repr(e)[:60]); 
def apply_cfg():
    global applied
    lines = [l.strip() for l in open(a.fast_cfg) if l.strip() and not l.startswith("%") and not l.startswith("baudRate")]
    L("applying", os.path.basename(a.fast_cfg), len(lines), "lines")
    r.reset_input_buffer()
    for ln in lines:
        r.write((ln + "\n").encode()); t = time.time(); b = b""
        while time.time() - t < (3.0 if ln.startswith("sensorStart") else 2.0):
            b += r.read(4096)
            if b"Done" in b or b"rror" in b: time.sleep(0.05); b += r.read(4096); break
        s = " | ".join(x.decode() for x in re.findall(rb"[\x20-\x7e]{3,}", b))[:100]
        L("cfg", ln, "->", s)
    applied = True; L("cfg applied")
try:
    while True:
        d = r.read(8192); now = time.time()
        if d:
            last_rx = now
            if state == "silent": state = "banner"; L("bytes after silence: banner phase"); banner = b""
            if state == "banner": banner += d
            buf += d
        if state == "banner" and b"mmwDemo" in banner and (now - last_rx) > 0.4 and MAGIC not in banner:
            if not applied and not a.no_apply: apply_cfg(); buf = b""
            else: L("prompt seen, not applying (already applied or --no-apply)")
            state = "stream"; had_frames = False; last_rx = time.time()
        if state == "stream":
            while True:
                i = buf.find(MAGIC)
                if i < 0: buf = buf[-7:]; break
                if len(buf) < i + 40: buf = buf[i:]; break
                ln = struct.unpack_from("<I", buf, i + 12)[0]
                if ln < 40 or ln > 65536: buf = buf[i + 8:]; continue
                if len(buf) < i + ln: buf = buf[i:]; break
                f = buf[i:i + ln]; buf = buf[i + ln:]; t = time.time(); had_frames = True; nfr += 1
                gwrite(f)
                pres = -1; pts = 0
                for hd, tl in frames(f):
                    for ty, n, bb in tl:
                        if ty == 315 and n >= 2:
                            z = min(bb[0], 4); pres = int(z > 0 and (bb[1] & ((1 << (2 * z)) - 1)) != 0)
                        elif ty == 301 and n >= 20: pts = (n - 20) // 10
                    seq = hd["frame"]
                    rows[seq] = [t, pres, pts, None, None]
                    if pres_prev is not None and pres != pres_prev: L("held presence", pres_prev, "->", pres, "at frame", seq)
                    pres_prev = pres
        if had_frames and state == "stream" and time.time() - last_rx > 3.0:
            state = "silent"; L("radar stream stopped (no bytes for 3 s)")
        if now - last_w >= 1.0:
            last_w = now; w = wline()
            if w: gwrite(w); nw += 1
        if g is not None:
            try: gd = g.read(4096)
            except Exception: gd = b""
            if gd:
                gbuf += gd; tn = time.time()
                while b"\n" in gbuf:
                    ln_, gbuf = gbuf.split(b"\n", 1); s = ln_.decode("latin1").strip()
                    if s.startswith("SHOWN,"):
                        p = s.split(",")
                        try:
                            q = int(p[1])
                            if q in rows: rows[q][3] = tn; rows[q][4] = int(p[3])
                        except (ValueError, IndexError): pass
                    elif s.startswith("STAT,"): L(s)
        for q in [k for k, v in rows.items() if v[3] is not None or time.time() - v[0] > 5]:
            v = rows.pop(q); fc.write("%d,%.6f,%d,%d,%s,%s\n" % (q, v[0], v[1], v[2], "%.6f" % v[3] if v[3] else "", v[4] if v[4] is not None else ""))
        if now - last_stat >= 10: last_stat = now; L("status frames", nfr, "w_lines", nw, "g3519_write_drops", ndrop, "state", state)
except KeyboardInterrupt:
    L("feeder stopped")
