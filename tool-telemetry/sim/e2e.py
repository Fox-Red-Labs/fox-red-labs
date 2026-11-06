#!/usr/bin/env python3
"""End-to-end check: drive a tag with the pretend tool and watch the gateway's API at the same time.
    python e2e.py --tag-port /dev/cu.usbmodemLS470GTU1 --hub http://<hub address> [--minutes 5] [--speed 3] [--out DIR]
For every event the tool sent (trigger pull ended, overcurrent trip) it finds the matching gateway log item and records
the delay from the UART line to the item showing up in /api/log. At the end it compares the tool's totals with what the
gateway reports for that tag. Writes events.csv and summary.json to --out and prints the summary.
Needs pyserial (~/.venvs/ble/bin/python)."""
import argparse
import csv
import json
import os
import statistics
import sys
import threading
import time
import urllib.request

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "proto"))
sys.path.insert(0, os.path.join(HERE, "..", "ai"))
from toolsim import Tool  # noqa: E402

TYPE = {"trig": 1, "oc": 2}


def get(url, timeout=4, tries=4):
    for k in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=timeout) as r:
                return json.loads(r.read())
        except Exception:
            if k == tries - 1:
                raise
            time.sleep(2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag-port", required=True)
    ap.add_argument("--hub", required=True)
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("--speed", type=float, default=3)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--jam-every", type=float, default=5)
    ap.add_argument("--id", default="UNIT-07")
    ap.add_argument("--hub-port", help="the hub's UART: also time each event to its @L line (radio path, no HTTP)")
    ap.add_argument("--out", default=os.path.abspath("out/tool-telemetry/e2e"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    info0 = get(a.hub + "/api/info")
    cursor = info0["recs"]
    sent = []            # (t_sent, type, v)
    seen = []            # (t_seen, type, v, id, addr)
    stop = threading.Event()
    errors = []

    def watch():
        nonlocal cursor
        while not stop.is_set():
            try:
                page = get(a.hub + "/api/log?since=%d" % cursor, tries=1)
                t = time.time()
                for it in page["items"]:
                    if it["k"] == "p":
                        pulls_seen.append(it)
                    if it["k"] == "e" and it["type"] in (1, 2):
                        seen.append((t, it["type"], it["v"], it["id"], it["a"]))
                cursor = page["next"]
            except Exception as e:  # keep watching through a dropped request
                errors.append("%.1f %s" % (time.time(), e))
            time.sleep(0.25)

    th = threading.Thread(target=watch, daemon=True)
    th.start()
    uart_seen = []       # (t_seen, type, v) from the hub's @L lines
    pulls_seen = []      # hub 'p' items from the API

    def watch_uart():
        s = serial.Serial(a.hub_port, 115200, timeout=0.1)
        buf = b""
        while not stop.is_set():
            buf += s.read(4096)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace")
                if "@L {" in line:
                    try:
                        it = json.loads(line[line.index("@L {") + 3:])
                    except ValueError:
                        continue
                    if it.get("k") == "e" and it.get("type") in (1, 2):
                        uart_seen.append((time.time(), it["type"], it["v"]))

    if a.hub_port:
        threading.Thread(target=watch_uart, daemon=True).start()

    port = serial.Serial(a.tag_port, 115200, timeout=0)
    taglog = open(os.path.join(a.out, "tag.log"), "w")
    tool = Tool(a.id, a.seed, a.jam_every)
    t0 = time.time()
    last_ts = last_tb = -99.0
    nlines = 0

    def emit(s):
        nonlocal nlines
        port.write(s.encode())
        nlines += 1
        if s.startswith("$TE"):
            kv = dict(p.split("=", 1) for p in s.split("*")[0].split(",")[1:])
            if kv.get("ev") in TYPE:
                sent.append((time.time(), TYPE[kv["ev"]], int(kv["v"])))

    emit(tool.line("TE", ev="boot", v=1))
    while time.time() - t0 < a.minutes * 60:
        was = tool.st
        for ev in tool.step(0.1 * a.speed):
            emit(ev)
        now = time.time() - t0
        if tool.st != was or now - last_ts >= (1.0 if tool.st == 1 else 10.0):
            emit(tool.status())
            last_ts = now
            if tool.st == 0:
                tool.ipk = 0
        if now - last_tb >= 10:
            emit(tool.battery())
            last_tb = now
        tagout = port.read(4096)          # the tag's own status lines, kept for diagnosis
        if tagout:
            taglog.write(tagout.decode("utf-8", "replace").replace("\n@", "\n%.1f @" % (time.time() - t0)))
            taglog.flush()
        time.sleep(0.1)
    # stop the motor so the last status is idle, then let the radio catch up
    tool.st, tool.rpm = 0, 0
    emit(tool.status())
    time.sleep(8)
    stop.set()
    th.join(2)

    # match in order: each sent event to the first unmatched seen event of the same type and value
    used = set()
    rows = []
    for ts, ty, v in sent:
        hit = next((k for k, s in enumerate(seen) if k not in used and s[1] == ty and s[2] == v), None)
        if hit is None:
            rows.append(dict(t_sent=round(ts - t0, 3), type=ty, v=v, latency_s="", gw_id=""))
        else:
            used.add(hit)
            rows.append(dict(t_sent=round(ts - t0, 3), type=ty, v=v, latency_s=round(seen[hit][0] - ts, 3),
                             gw_id=seen[hit][3]))
    with open(os.path.join(a.out, "events.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]) if rows else ["t_sent"])
        w.writeheader()
        w.writerows(rows)

    # radio path: the same matching against the hub's UART lines
    used2, lat_uart = set(), []
    for ts, ty, v in sent:
        hit = next((k for k, s in enumerate(uart_seen) if k not in used2 and s[1] == ty and s[2] == v), None)
        if hit is not None:
            used2.add(hit)
            lat_uart.append(uart_seen[hit][0] - ts)
    # pull summaries: match each simulated pull to the hub's record by its speed and current, then score the class
    # the tag's model gave it against the class the simulator drew
    from physics import CLASSES
    by_key = {}
    for it in pulls_seen:
        by_key.setdefault((it["ravg"], it["rmax"], it["iavg"], it["ipk"]), []).append(it)
    cm = [[0] * 4 for _ in range(4)]
    pulls_matched = 0
    truth_rows = []
    for _, cls, f in tool.truth:
        hits = by_key.get((f["rpm_avg"], f["rpm_max"], f["i_avg_ma"], f["i_peak_ma"]))
        if hits:
            hit = hits.pop(0)
            got = hit["cls"]
            truth_rows.append((hit["a"], hit["pid"], cls))
            pulls_matched += 1
            if got < 4:
                cm[cls][got] += 1
    with open(os.path.join(a.out, "truth.csv"), "w", newline="") as tf:
        tw = csv.writer(tf)
        tw.writerow(("tag_addr", "pull_id", "label"))
        tw.writerows(truth_rows)
    tags = get(a.hub + "/api/tags")
    tag = next((t for t in tags if t.get("tool") == a.id), None)
    lat = [r["latency_s"] for r in rows if r["latency_s"] != ""]
    q = statistics.quantiles(lat, n=20) if len(lat) >= 20 else None
    info1 = get(a.hub + "/api/info")
    summary = dict(
        minutes=a.minutes, speed=a.speed, lines_sent=nlines, events_sent=len(sent),
        events_seen=len(lat), events_missed=len(sent) - len(lat), extra_seen=len(seen) - len(used),
        latency_s=dict(median=round(statistics.median(lat), 3) if lat else None,
                       p95=round(q[18], 3) if q else None, max=round(max(lat), 3) if lat else None),
        radio_path=dict(seen=len(lat_uart), median=round(statistics.median(lat_uart), 3) if lat_uart else None,
                        p95=round(statistics.quantiles(lat_uart, n=20)[18], 3) if len(lat_uart) >= 20 else None,
                        max=round(max(lat_uart), 3) if lat_uart else None) if a.hub_port else None,
        pulls=dict(sent=len(tool.truth), seen=len(pulls_seen), matched=pulls_matched,
                   hub_missed=tag.get("pmiss") if tag else None,
                   tag_model_accuracy=round(sum(cm[i][i] for i in range(4)) / max(1, sum(map(sum, cm))), 3),
                   confusion_true_by_predicted=cm, classes=CLASSES),
        tool_totals=dict(trig=tool.trig, oc=tool.oc, run=int(tool.run)),
        gateway_totals=dict(trig=tag["trig"], oc=tag["oc"], run=tag["run"], st=tag["st"]) if tag else None,
        totals_match=bool(tag) and tag["trig"] == tool.trig and tag["oc"] == tool.oc and tag["run"] == int(tool.run),
        tag_rx=dict(ok=tag.get("rxok"), bad=tag.get("rxbad")) if tag else None,
        frames=dict(legacy=info1["scan"]["legacy"] - info0["scan"]["legacy"], ext=info1["scan"]["ext"] - info0["scan"]["ext"],
                    all_adv=info1["scan"]["adv"] - info0["scan"]["adv"]),
        api_errors=len(errors), hub_up_start=info0["up"], hub_up_end=info1["up"],
        hub_restarted=info1["up"] < info0["up"] + (time.time() - t0) - 30, when=time.strftime("%Y-%m-%d %H:%M:%S"))
    open(os.path.join(a.out, "api_errors.txt"), "w").write("\n".join(errors))
    json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=1)
    print(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
