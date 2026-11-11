#!/bin/sh
# Runs ON the board while Frigate is up. Usage: sample.sh <label> <seconds> <camera name>
# Every 10 s: Frigate's own inference speed, detection fps, camera fps, process fps, then medians, container memory, events and a snapshot.
L=$1; T=${2:-120}; C=${3:-yard}; O=/root/frigate/out; mkdir -p $O
python3 - "$L" "$T" "$C" <<'PY'
import sys, json, time, urllib.request, subprocess, statistics as st
L, T, C = sys.argv[1], int(sys.argv[2]), sys.argv[3]
g = lambda p: json.load(urllib.request.urlopen("http://localhost:5000" + p, timeout=10))
rows = []
for i in range(T // 10):
    time.sleep(10)
    d = g("/api/stats"); c = d["cameras"][C]; det = list(d["detectors"].values())[0]
    rows.append((det["inference_speed"], c["detection_fps"], c["camera_fps"], c["process_fps"], c["skipped_fps"]))
    print(L, "t=%ds inference_ms=%s detection_fps=%s camera_fps=%s process_fps=%s skipped=%s" % ((i + 1) * 10, *rows[-1]), flush=True)
med = lambda k: round(st.median(r[k] for r in rows[1:]), 2)   # drop the first sample: warm-up
print(L, "MEDIAN inference_ms=%s detection_fps=%s camera_fps=%s process_fps=%s" % (med(0), med(1), med(2), med(3)))
print(L, "docker", subprocess.run("docker stats --no-stream --format '{{.CPUPerc}} cpu, {{.MemUsage}}' frigate", shell=True, capture_output=True, text=True).stdout.strip())
print(L, "free", subprocess.run("free -m | sed -n 2p", shell=True, capture_output=True, text=True).stdout.strip())
print(L, "load", open("/proc/loadavg").read().strip())
ev = g("/api/events?limit=100"); from collections import Counter
print(L, "events", dict(Counter(e["label"] for e in ev)), "max_score", max([e.get("data", {}).get("top_score", 0) for e in ev] or [0]))
open("/root/frigate/out/%s.jpg" % L, "wb").write(urllib.request.urlopen("http://localhost:5000/api/%s/latest.jpg?bbox=1" % C, timeout=10).read())
PY
