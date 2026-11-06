#!/usr/bin/env python3
"""Empty-room baseline: configure, then record frames until stopped or --mins elapse.
   empty_soak.py <cfg> <out_prefix> [--mins 30]
Writes <out>.bin (raw) and <out>.json every minute: frames, frames with points, frames with tracks,
track ids seen, per-0.5 m cell point counts. Needs pyserial."""
import sys, time, json, math, serial, radar_live as rl
cfg, out = sys.argv[1], sys.argv[2]
mins = float(sys.argv[sys.argv.index("--mins") + 1]) if "--mins" in sys.argv else 30
bad = rl.configure("/dev/cu.usbmodemRI321", cfg, "RI32"); print("config errors", bad, flush=True)
s = serial.Serial("/dev/cu.usbmodemRI321", 115200, timeout=0.05)
raw = open(out + ".bin", "wb"); buf = b""; t0 = time.time(); nxt = t0 + 60
st = {"start": time.strftime("%Y-%m-%d %H:%M:%S"), "frames": 0, "with_points": 0, "with_tracks": 0,
      "track_ids": {}, "cells": {}, "minutes": []}
m = {"frames": 0, "with_points": 0, "with_tracks": 0}
while time.time() - t0 < mins * 60:
    c = s.read(4096); raw.write(c)
    fr, buf = rl.split_frames(buf + c)
    for f in fr:
        d = rl.decode(f); st["frames"] += 1; m["frames"] += 1
        if d["points"]:
            st["with_points"] += 1; m["with_points"] += 1
            for p in d["points"]:
                k = f"{math.floor(p[0]/0.5)*0.5:.1f},{math.floor(p[1]/0.5)*0.5:.1f}"
                st["cells"][k] = st["cells"].get(k, 0) + 1
        if d["tracks"]:
            st["with_tracks"] += 1; m["with_tracks"] += 1
            for t in d["tracks"]:
                st["track_ids"].setdefault(str(t["id"]), []).append([round(time.time() - t0, 1), t["x"], t["y"]])
    if time.time() >= nxt:
        st["minutes"].append(m); m = {"frames": 0, "with_points": 0, "with_tracks": 0}; nxt += 60
        st["elapsed_s"] = round(time.time() - t0); raw.flush()
        json.dump(st, open(out + ".json", "w"))
        print(f"min {len(st['minutes'])}: {st['minutes'][-1]}", flush=True)
st["elapsed_s"] = round(time.time() - t0); json.dump(st, open(out + ".json", "w")); print("done", flush=True)
