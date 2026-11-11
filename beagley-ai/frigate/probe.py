#!/usr/bin/env python3
"""Post JPEG files to the detection server the way Frigate's deepstack detector does and time the round trips.
Usage: probe.py <url> <repeat> <jpeg> [<jpeg> ...]     (prints the first reply per file, then median and p95 round trip in ms)"""
import sys, time, json, urllib.request, uuid
url, rep, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
def post(b):
    bd = uuid.uuid4().hex
    body = (f'--{bd}\r\nContent-Disposition: form-data; name="api_key"\r\n\r\n\r\n--{bd}\r\nContent-Disposition: form-data; name="image"; filename="image"\r\n'
            f'Content-Type: application/octet-stream\r\n\r\n').encode() + b + f'\r\n--{bd}--\r\n'.encode()
    r = urllib.request.Request(url, body, {"Content-Type": f"multipart/form-data; boundary={bd}"})
    return json.load(urllib.request.urlopen(r, timeout=10))
ts = []
for f in files:
    b = open(f, "rb").read()
    out = post(b); print(f, json.dumps([(p["label"], p["confidence"]) for p in out["predictions"][:6]]), "inferenceMs", out["inferenceMs"], "processMs", out["processMs"])
    for _ in range(rep):
        t = time.perf_counter(); post(b); ts.append((time.perf_counter() - t) * 1e3)
ts.sort(); print(f"round trip ms: n={len(ts)} median={ts[len(ts)//2]:.2f} p95={ts[int(len(ts)*.95)-1]:.2f}")
