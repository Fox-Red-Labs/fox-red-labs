#!/usr/bin/env python3
"""Radar CLI + stream helpers for the IWRL6432BOOST Motion and Presence 2D demo (shared by radar_eval.py).
Port is opened by explicit path; nothing here resets, flashes or replugs anything."""
import re, sys, time, struct, os
import serial
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
from tlv_parse import frames  # noqa: E402

PORT = "/dev/cu.usbmodemRI32RI321"
CFG = os.path.expanduser("out/iwrl6432boost/toolbox/source/ti/examples/"
      "Industrial_and_Personal_Electronics/Motion_and_Presence_Detection/chirp_configs/"
      "motion_and_presence/xwrl6432boost/PresenceDetect.cfg")
# name -> (rx mask, tx mask). The name is TX x RX antennas.
CONFIGS = {"2x3": (7, 3), "2x2": (3, 3), "1x1": (1, 1), "1x3": (7, 1), "2x1": (1, 3), "1x2": (3, 1)}

def txt(b): return " | ".join(x.decode() for x in re.findall(rb"[\x20-\x7e]{3,}", b))

def cfg_lines(path=CFG, name="2x3", overrides=None):
    """PresenceDetect.cfg lines (comments dropped, baudRate dropped) with channelCfg replaced for config `name`."""
    rx, tx = CONFIGS[name]
    out = []
    for raw in open(path):
        l = raw.strip()
        if not l or l.startswith("%") or l.startswith("baudRate"): continue
        if l.startswith("channelCfg"): l = f"channelCfg {rx} {tx} 0"
        if overrides:
            for k, v in overrides.items():
                if l.split()[0] == k: l = f"{k} {v}"
        out.append(l)
    return out

class Radar:
    def __init__(self, port=PORT):
        self.s = serial.Serial(port, 115200, timeout=0.05)
        self.buf = b""
        self.fwd = None; self.wfile = None; self._wt = 0.0
    def enable_forward(self, port, wfile):
        """Forward every whole radar frame, unchanged, to the G3519's USB serial, and once a second a line W,<cls>,<p> from the last row of the Wi-Fi log (read only; the CC35X1 port is never opened). Runs in its own thread so a slow G3519 can never delay reading the radar; frames are dropped when the queue is full."""
        import queue, threading
        self.fwd = serial.Serial(port, 115200, timeout=0, write_timeout=0.5); self.wfile = wfile
        self._q = queue.Queue(maxsize=8)
        def work():
            wt = 0.0
            while True:
                fr = self._q.get()
                if fr is None: return
                try:
                    self.fwd.write(fr)
                    now = time.time()
                    if self.wfile and now - wt >= 1.0:
                        wt = now
                        with open(self.wfile, "rb") as f:
                            f.seek(0, 2); n = f.tell(); f.seek(max(0, n - 512)); last = f.read().strip().split(b"\n")[-1].decode("latin1").split(",")
                        if len(last) >= 4 and last[0] != "t": self.fwd.write(f"W,{last[2]},{last[3]}\n".encode())
                except Exception:
                    pass
        self._th = threading.Thread(target=work, daemon=True); self._th.start()
    def _forward(self, frame):
        if not self.fwd: return
        try: self._q.put_nowait(frame)
        except Exception: pass
    def close(self):
        self.s.close()
        if self.fwd:
            try: self._q.put_nowait(None)
            except Exception: pass
    def cmd(self, line, wait=2.0):
        """Send one CLI line; return (accepted, reply text). Stream bytes mixed in are filtered by txt()."""
        self.s.write((line + "\n").encode())
        t, b = time.time(), b""
        while time.time() - t < wait:
            b += self.s.read(4096)
            if b"Done" in b or b"rror" in b or b"gnored" in b:
                time.sleep(0.05); b += self.s.read(4096); break
        r = txt(b)
        return ("Done" in r and "rror" not in r), r[:240]
    def apply(self, lines, log=print, stop_on_error=False):
        """sensorStop first, then each line. Returns list of (line, ok, reply)."""
        res = []
        self.s.reset_input_buffer()
        for l in lines:
            ok, r = self.cmd(l, 3.0 if l.startswith("sensorStart") else 2.0)
            res.append((l, ok, r)); log(f"  {l} -> {r}")
            if not ok and stop_on_error: break
        return res
    def flush(self, drain=True):
        """Drop what is buffered. The macOS serial driver keeps more than tcflush clears (a whole earlier minute can arrive at once),
        so keep reading and discarding until frames arrive one at a time, i.e. the reading is live again."""
        self.s.reset_input_buffer(); self.buf = b""
        if not drain: return
        t0 = time.time(); calm = 0
        while time.time() - t0 < 90 and calm < 3:
            n = len(self.poll()); calm = calm + 1 if n <= 1 else 0
            if n == 0: time.sleep(0.05)
    def poll(self):
        """Read bytes, return list of (t_mac, header, tlvs) for each whole frame (t = when read)."""
        d = self.s.read(8192)
        now = time.time()
        if d: self.buf += d
        out = []
        while True:
            i = self.buf.find(bytes([2, 1, 4, 3, 6, 5, 8, 7]))
            if i < 0: self.buf = self.buf[-7:]; break
            if i + 40 > len(self.buf): self.buf = self.buf[i:]; break
            plen = struct.unpack_from("<I", self.buf, i + 12)[0]
            if plen < 40 or plen > 65536: self.buf = self.buf[i + 8:]; continue
            if i + plen > len(self.buf): self.buf = self.buf[i:]; break
            self._forward(self.buf[i:i + plen])
            for hd, tl in frames(self.buf[i:i + plen]): out.append((now, hd, tl))
            self.buf = self.buf[i + plen:]
        return out

def decode(tl):
    """-> dict: zones (list of 0..3 per zone), nz, pres (any), prof (303 minor profile list or None), prof_major, pts (obj count)"""
    r = {"nz": 0, "zones": [], "pres": None, "prof": None, "profmaj": None}
    for t, n, b in tl:
        if t == 315 and n >= 2:
            nz = b[0]; zs = []
            for z in range(min(nz, 4 * (n - 1))):
                zs.append((b[1 + z // 4] >> (2 * (z % 4))) & 3)
            r.update(nz=nz, zones=zs, pres=int(any(zs)))
        elif t == 303: r["prof"] = list(struct.unpack_from(f"<{n // 4}I", b))
        elif t == 302: r["profmaj"] = list(struct.unpack_from(f"<{n // 4}I", b))
    return r
