#!/usr/bin/env python3
"""Replay a recorded IWRL6432BOOST capture into a serial port, with the frame number renumbered from 1 and the presence bits
of TLV 315 rewritten on a schedule, and record when each frame was written and when the board said it drew it.
  replay.py <capture.bin> <port> <out.csv> [--frames N] [--period S] [--toggle K]
Writes <out.csv> (seq, t_write, t_shown, dev_lat_us) and <out.csv>.log (every line the board printed). Uses termios, no pyserial.
t_write is time.time() just before the first byte of the frame goes out; t_shown is time.time() when the SHOWN line arrives.
The capture is real radar data; only the frame number and the zone-bit byte of TLV 315 are changed (the shipped capture has no
one in front of the radar, so presence is always 0 in it). Every rewritten frame is still a valid TI TLV frame."""
import os, sys, struct, termios, threading, time, argparse
MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
def frames(buf):
    i = 0
    while True:
        i = buf.find(MAGIC, i)
        if i < 0 or i + 40 > len(buf): return
        plen = struct.unpack_from("<I", buf, i + 12)[0]
        if plen < 40 or plen > 65536 or i + plen > len(buf): i += 8; continue
        yield bytearray(buf[i:i + plen]); i += plen
def find_315(f):
    n = struct.unpack_from("<I", f, 32)[0]; p = 40
    for _ in range(min(n, 8)):
        if p + 8 > len(f): return -1
        t, l = struct.unpack_from("<II", f, p)
        if t == 315: return p + 8
        p += 8 + l
    return -1
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cap"); ap.add_argument("port"); ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=200); ap.add_argument("--period", type=float, default=0.25)
    ap.add_argument("--toggle", type=int, default=8, help="presence flips every K frames"); a = ap.parse_args()
    fr = [f for f in frames(open(a.cap, "rb").read()) if find_315(f) >= 0]
    if not fr: sys.exit("no frame with TLV 315 in the capture")
    fd = os.open(a.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    t = termios.tcgetattr(fd); t[0] = 0; t[1] = 0; t[3] = 0; t[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    t[4] = t[5] = termios.B115200; t[6][termios.VMIN] = 0; t[6][termios.VTIME] = 0; termios.tcsetattr(fd, termios.TCSANOW, t)
    shown = {}; log = open(a.out + ".log", "w"); stop = [False]
    def reader():
        buf = b""
        while not stop[0]:
            try: d = os.read(fd, 4096)
            except BlockingIOError: d = b""
            if not d: time.sleep(0.002); continue
            now = time.time(); buf += d
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1); s = line.decode("latin1").strip(); log.write("%.6f %s\n" % (now, s))
                if s.startswith("SHOWN,"):
                    p = s.split(","); 
                    try: shown[int(p[1])] = (now, int(p[3]))
                    except (ValueError, IndexError): pass
    th = threading.Thread(target=reader, daemon=True); th.start()
    rows = []; t0 = time.time()
    for k in range(a.frames):
        f = bytearray(fr[k % len(fr)]); seq = k + 1
        struct.pack_into("<I", f, 20, seq)
        p = find_315(f); on = (k // a.toggle) % 2 == 1
        f[p] = 1; f[p + 1] = 1 if on else 0           # one zone; bit 0 set means someone is in it
        tw = time.time(); data = bytes(f); n = 0
        while n < len(data):
            try: n += os.write(fd, data[n:])
            except BlockingIOError: time.sleep(0.001)
        rows.append((seq, tw))
        nxt = t0 + (k + 1) * a.period
        time.sleep(max(0, nxt - time.time()))
    time.sleep(1.0); stop[0] = True; th.join(2)
    with open(a.out, "w") as o:
        o.write("seq,t_write,t_shown,dev_lat_us\n")
        for seq, tw in rows:
            s = shown.get(seq)
            o.write("%d,%.6f,%s,%s\n" % (seq, tw, "%.6f" % s[0] if s else "", s[1] if s else ""))
    got = sum(1 for seq, _ in rows if seq in shown)
    print("sent %d frames, %d SHOWN lines matched" % (len(rows), got))
main()
