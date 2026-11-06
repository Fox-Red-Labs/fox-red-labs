#!/usr/bin/env python3
"""Build demo-data.json for the web dashboard from the recorded bench runs in ../results/.
    python3 -I make_demo.py
Each tag in the demo is one recorded run (events.csv: when the tool sent the event, its type, its value, the delay until
the hub's log showed it). The runs were made one after another and are played side by side. The two runs made with
the current simulator (seed 7) are replayed through the same simulator to read the battery line the machine reported at
each event; the replay must reproduce every recorded event exactly or the script stops. The older run has no battery."""
import csv
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "results")
sys.path.insert(0, os.path.join(HERE, "..", "sim"))
from toolsim import Tool  # noqa: E402

RUNS = [  # (results folder, tag name, equipment name, replay the simulator for the battery)
    ("e2e-7", "Tag 01", "Equipment 01", True),
    ("e2e-5", "Tag 02", "Equipment 02", False),
    ("e2e-zephyr", "Tag 03", "Equipment 03", True),
]


def battery(rec):
    """Battery percent at each recorded event, from the seeded simulator; None where it cannot be reproduced."""
    t = Tool("X", 7, 5)
    out, now = [], 0.0
    while len(out) < len(rec) and now < 2000:
        for e in t.step(0.1 * 3):
            if "ev=trig" in e:
                out.append(("1", int(e.split(",v=")[1].split("*")[0]), t.soc))
            elif "ev=oc" in e:
                out.append(("2", int(e.split(",v=")[1].split("*")[0]), t.soc))
        now += 0.1
    bad = [i for i, (a, b) in enumerate(zip(rec, out)) if a != (b[0], b[1])]
    if len(out) != len(rec) or bad:
        sys.exit("simulator replay does not match the recording (%d events, %d off)" % (len(rec), len(bad)))
    return [o[2] for o in out]


def main():
    tags, frames = [], {"legacy": 0, "ext": 0, "adv": 0}
    summary = None
    for folder, name, tool, replay in RUNS:
        rows = list(csv.DictReader(open(os.path.join(RES, folder, "events.csv"))))
        s = json.load(open(os.path.join(RES, folder, "summary.json")))
        rec = [(r["type"], int(r["v"])) for r in rows]
        soc = battery(rec) if replay else [None] * len(rows)
        ev = [[round(float(r["t_sent"]), 2), int(r["type"]), int(r["v"]), round(float(r["latency_s"]), 2), c]
              for r, c in zip(rows, soc)]
        frames["legacy"] += s["frames"]["legacy"]
        frames["ext"] += s["frames"]["ext"]
        frames["adv"] += s["frames"]["all_adv"]
        if folder == "e2e-7":
            summary = {"sent": s["events_sent"], "seen": s["events_seen"], "median": s["radio_path"]["median"],
                       "p95": s["radio_path"]["p95"], "totals_match": s["totals_match"],
                       "good_lines": s["tag_rx"]["ok"], "bad_lines": s["tag_rx"]["bad"]}
        tags.append({"id": "demo-%d" % (len(tags) + 1), "name": name, "tool": tool, "run": folder,
                     "recorded": s["when"][:10], "minutes": s["minutes"], "ev": ev})
        runsec = sum(e[2] for e in ev if e[1] == 1) / 1000
        print("%s: %d events, %d trips, run %.0f s (summary %s), battery %s -> %s" % (
            folder, len(ev), sum(1 for e in ev if e[1] == 2), runsec, s["tool_totals"]["run"], ev[0][4], ev[-1][4]))
    json.dump({"note": "Recorded bench runs of 2026-10-17, played side by side. ev rows: seconds since the run began, "
                       "type (1 pull ended, 2 overcurrent trip), value (pull length in ms; peak current in mA), seconds until the "
                       "hub's log showed it, battery percent or null.",
               "hub": {"up0": 45, "recorded": summary}, "frames": frames, "tags": tags},
              open(os.path.join(HERE, "demo-data.json"), "w"), separators=(",", ":"))
    print("demo-data.json written")


if __name__ == "__main__":
    main()
