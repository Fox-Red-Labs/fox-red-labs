#!/usr/bin/env python3
"""Mac bridge: IWRL6432BOOST presence flag to Home Assistant over REST (route A in the plan).
Reads the radar's TLV stream (shipped Motion and Presence demo, 115200 baud, TLV 315), and posts
binary_sensor.iwrl6432_presence to Home Assistant on every change, plus a heartbeat every 30 s so the
entity reappears after a Home Assistant restart. Read only toward the radar: nothing is written to the port.

  ha_bridge.py run [--port P] [--entity E] [--samples N] [--log out.csv]   live
  ha_bridge.py replay capture.bin                                       parse a file, print the transitions, post nothing
  ha_bridge.py selftest                                                 no board, no Home Assistant

Presence rule: TLV 315 byte 0 is the number of zones, the next bytes hold 2 bits per zone from the least
significant bit (0 = nothing; the nonzero values are motion or presence, read from the toolbox source).
Presence is on when any zone state is nonzero.
Latency row: for each change, ms from the end of the frame's last byte on the Mac to the REST post returning,
then a GET confirms the state (the confirm time is logged apart). The token comes from tools/ha.py (the home-assistant
MCP entry in ~/.claude.json) at run time; it is never printed or stored here."""
import argparse, os, struct, sys, termios, time, statistics
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tlv_parse as T

def presence_of(tlvs):
    for t, n, b in tlvs:
        if t == 315 and n >= 2:
            nz = b[0]
            bits = int.from_bytes(b[1:], "little")
            states = [(bits >> (2 * i)) & 3 for i in range(nz)]
            return any(states), states
    return None, None

class Stream:
    """Incremental frame extractor that stamps each frame with the time its last byte arrived."""
    def __init__(self): self.buf = b""
    def feed(self, data, t):
        self.buf += data
        out = []
        while True:
            i = self.buf.find(T.MAGIC)
            if i < 0:
                self.buf = self.buf[-7:]; break
            if len(self.buf) < i + T.HDR.size: self.buf = self.buf[i:]; break
            plen = T.HDR.unpack_from(self.buf, i)[2]
            if plen < T.HDR.size or plen > 65536: self.buf = self.buf[i + 8:]; continue
            if len(self.buf) < i + plen: self.buf = self.buf[i:]; break
            pkt = self.buf[i:i + plen]; self.buf = self.buf[i + plen:]
            fr = list(T.frames(pkt))
            if fr: out.append((t, fr[0][0], fr[0][1]))
        return out

def open_port(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0; a[1] = 0; a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = termios.B115200
    a[6][termios.VMIN] = 0; a[6][termios.VTIME] = 1
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd

def run(a):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
    import ha
    fd = open_port(a.port); s = Stream(); cur = None; lat = []; last_beat = 0; frames = 0
    log = open(a.log, "a") if a.log else None
    print(f"bridge up: {a.port} -> {a.entity}", flush=True)
    while True:
        try: d = os.read(fd, 4096)
        except BlockingIOError: d = b""
        if not d: time.sleep(0.005); continue
        for t, hd, tl in s.feed(d, time.monotonic()):
            frames += 1
            p, states = presence_of(tl)
            if p is None: continue
            now = time.monotonic()
            if p != cur or now - last_beat > 30:
                changed = p != cur
                body = {"state": "on" if p else "off", "attributes": {"device_class": "occupancy",
                        "friendly_name": "IWRL6432 presence", "zone_states": states, "frame": hd["frame"]}}
                t1 = time.monotonic(); ha.req("POST", f"/api/states/{a.entity}", body); t2 = time.monotonic()
                _, got = ha.req("GET", f"/api/states/{a.entity}"); t3 = time.monotonic()
                ok = got["state"] == body["state"]
                if changed and cur is not None:
                    ms = round((t2 - t) * 1000); lat.append(ms)
                    line = f"{time.strftime('%H:%M:%S')},{hd['frame']},{body['state']},{ms},{round((t3 - t) * 1000)},{ok}"
                    print("change", line, flush=True)
                    if log: log.write(line + "\n"); log.flush()
                    if a.samples and len(lat) >= a.samples:
                        print("samples", len(lat), "median ms", statistics.median(lat), "min", min(lat), "max", max(lat)); return
                cur = p; last_beat = now

def replay(path):
    s = Stream(); cur = None; n = 0
    for t, hd, tl in s.feed(open(path, "rb").read(), 0.0):
        n += 1; p, st = presence_of(tl)
        if p is not None and p != cur:
            print("frame", hd["frame"], "presence", p, "zone states", st); cur = p
    print(n, "frames parsed")

def selftest():
    def pkt(frame, z):
        body = struct.pack("<II", 315, 2) + bytes([1, z])
        return T.MAGIC + struct.pack("<8I", 0x05030300, 40 + len(body), 0xA6432, frame, 0, 0, 1, 0xFFFFFFFF) + body
    data = b"junk" + pkt(1, 0) + pkt(2, 1) + pkt(3, 0)
    s = Stream(); out = []
    for i in range(0, len(data), 7): out += s.feed(data[i:i + 7], 0.0)   # split mid-frame
    assert [presence_of(o[2])[0] for o in out] == [False, True, False], out
    print("selftest ok")

if __name__ == "__main__":
    ap = argparse.ArgumentParser(); sp = ap.add_subparsers(dest="cmd", required=True)
    r = sp.add_parser("run"); r.add_argument("--port", default="/dev/cu.usbmodemRI32RI321")
    r.add_argument("--entity", default="binary_sensor.iwrl6432_presence"); r.add_argument("--samples", type=int, default=0)
    r.add_argument("--log"); sp.add_parser("replay").add_argument("file"); sp.add_parser("selftest")
    a = ap.parse_args()
    {"run": lambda: run(a), "replay": lambda: replay(a.file), "selftest": selftest}[a.cmd]()
