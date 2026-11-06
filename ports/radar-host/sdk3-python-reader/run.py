#!/usr/bin/env python3
"""Run readAndParseData18xx() from ibaiGorordo/AWR1843-Read-Data-Python-MMWAVE-SDK-3- unchanged on an
IWRL6432BOOST stream, from a file or from the radar's data port.

  run.py <clone dir> --bin <stream.bin> [--walk <walk.json>]   replay; with --walk, compare every point
  run.py <clone dir> --port <device> --seconds N [--mac-glue]   live; the radar must already be running

Glue only: the script runs its own main loop at import (serial ports COM8/COM9, a Qt window), so this
loads the file's text up to its "MAIN" line and calls the parser with a port object. Not edited.
"""
import argparse, json, pathlib, sys, time
import numpy as np

class FilePort:
    """Releases the stream at the radar's pace: one frame per 250 ms, read every 50 ms as the script
    does (time.sleep(0.05)), so each read sees what a serial port would hold at that moment."""
    MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
    def __init__(self, data):
        self.data, self.i, self.calls = data, 0, 0
        starts = [k for k in range(len(data)) if data.startswith(self.MAGIC, k)]
        self.ends = starts[1:] + [len(data)]
    @property
    def in_waiting(self):
        self.calls += 1
        k = min(len(self.ends), self.calls // 5)
        return max(0, (self.ends[k - 1] if k else 0) - self.i)
    def read(self, n):
        b = self.data[self.i:self.i + n]; self.i += n; return b

class MacPort:
    """macOS: the XDS110 data port reports in_waiting 0 until a read is pending, so the script's
    read(in_waiting) never gets a byte. This wrapper reads first and reports what it holds."""
    def __init__(self, ser): self.ser, self.buf = ser, b""
    @property
    def in_waiting(self):
        self.buf += self.ser.read(4096); return len(self.buf)
    def read(self, n):
        b, self.buf = self.buf[:n], self.buf[n:]; return b

def load(clone):
    src = (pathlib.Path(clone) / "readData_AWR1843.py").read_text()
    head = src.split("# -------------------------    MAIN")[0]
    head = head.replace("import pyqtgraph as pg\n", "").replace("from pyqtgraph.Qt import QtGui\n", "")
    ns = {}
    exec(compile(head, "readData_AWR1843.py", "exec"), ns)   # the repo's code, as published
    return ns

def main():
    a = argparse.ArgumentParser(); a.add_argument("clone"); a.add_argument("--bin"); a.add_argument("--walk")
    a.add_argument("--port"); a.add_argument("--seconds", type=float, default=30)
    a.add_argument("--mac-glue", action="store_true", help="wrap the port so in_waiting works on macOS"); o = a.parse_args()
    ns = load(o.clone); parse = ns["readAndParseData18xx"]
    frames, headers = [], set()
    if o.bin:
        port = FilePort(open(o.bin, "rb").read())
        idle = 0
        while idle < 20:   # stop once 20 reads in a row change nothing (4 frame times)
            before = (port.i, ns["byteBufferLength"])
            ok, n, obj = parse(port, {})
            if int(n): headers.add(int(n))
            if ok: frames.append((int(n), obj))
            idle = idle + 1 if (port.i, ns["byteBufferLength"]) == before else 0
    else:
        import serial
        port = serial.Serial(o.port, 115200, timeout=0.05)
        if o.mac_glue: port = MacPort(port)
        t0 = time.time()
        while time.time() - t0 < o.seconds:
            ok, n, obj = parse(port, {})
            if int(n): headers.add(int(n))
            if ok: frames.append((int(n), obj))
            time.sleep(0.05)
    pts = sum(int(f[1]["numObj"]) for f in frames)
    print(f"headers decoded {len(headers)}, frames with points {len(frames)}, points {pts}")
    if o.walk:
        want = {f.get("frame", i): f.get("points") or [] for i, f in enumerate(json.load(open(o.walk))["frames"])}
        got = {n: obj for n, obj in frames}
        missing = [n for n, p in want.items() if p and n not in got]
        worst = 0.0
        for n, obj in got.items():
            w = np.array([p[:4] for p in want.get(n, [])], dtype=np.float32).reshape(-1, 4)
            g = np.stack([obj["x"], obj["y"], obj["z"], obj["velocity"]], 1) if int(obj["numObj"]) else np.zeros((0, 4))
            if g.shape != w.shape: print(f"frame {n}: {g.shape[0]} points, expected {w.shape[0]}"); continue
            worst = max(worst, float(np.abs(g - w).max()) if len(w) else 0.0)
        frames_with = sum(1 for p in want.values() if p)
        print(f"recording: {frames_with} frames with points, {sum(len(p) for p in want.values())} points; "
              f"missing frames {len(missing)}; largest difference {worst:.2e}")

if __name__ == "__main__":
    main()
