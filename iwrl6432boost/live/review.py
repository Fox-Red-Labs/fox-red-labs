#!/usr/bin/env python3
"""Day summary for the review page, from radar_live's log folder.

  review.py <log folder> [YYYY-MM-DD]          prints the summary as JSON
  review.py <log folder> YYYY-MM-DD --csv      prints the day's events as CSV

Summary: occupied periods (start, end, minutes), people per 5 minutes (most seen in each slot), entries and exits
per hour and per door, seat minutes, frames logged and the hours covered. Everything comes from
events-<day>.jsonl and frames-<day>.jsonl; nothing is estimated."""
import csv, io, json, os, sys, time


def _lines(path):
    if not os.path.exists(path):
        return []
    out = []
    for l in open(path):
        try:
            out.append(json.loads(l))
        except ValueError:
            pass  # a line cut short by a stop
    return out


def summary(log_dir, day=None):
    day = day or time.strftime("%Y-%m-%d")
    ev = _lines(os.path.join(log_dir, f"events-{day}.jsonl"))
    fr = _lines(os.path.join(log_dir, f"frames-{day}.jsonl"))
    hour = lambda t: time.strftime("%H", time.localtime(t))
    hm = lambda t: time.strftime("%H:%M", time.localtime(t))
    periods, start = [], None
    seat_min, seat_start = 0.0, None
    per_hour, per_door = {}, {}
    for e in ev:
        k = e["event"]
        if k == "occupied" and start is None:
            start = e["t"]
        elif k == "empty" and start is not None:
            periods.append({"start": hm(start), "end": hm(e["t"]), "minutes": round((e["t"] - start) / 60, 1)}); start = None
        elif k in ("in", "out"):
            h = per_hour.setdefault(hour(e["t"]), {"in": 0, "out": 0}); h[k] += 1
            d = per_door.setdefault(str(e.get("door", "?")), {"in": 0, "out": 0}); d[k] += 1
        elif k == "seat_on":
            seat_start = e["t"]
        elif k == "seat_off" and seat_start is not None:
            seat_min += (e["t"] - seat_start) / 60; seat_start = None
    now = fr[-1]["t"] if fr else time.time()
    if start is not None:
        periods.append({"start": hm(start), "end": "now", "minutes": round((now - start) / 60, 1)})
    if seat_start is not None:
        seat_min += (now - seat_start) / 60
    slots = {}
    for f in fr:
        k = time.strftime("%H:%M", time.localtime(f["t"] - f["t"] % 300))
        slots[k] = max(slots.get(k, 0), len(f.get("tr", [])))
    # utilization: occupied share of the working day (08:00 to 18:00), so far today or the whole window
    t0 = time.mktime(time.strptime(day + " 08:00", "%Y-%m-%d %H:%M")); t1 = t0 + 10 * 3600
    if fr:
        t0 = max(t0, fr[0]["t"])  # only time the radar was logging counts; earlier is unknown, not empty
    occ_s, start2 = 0.0, None
    for e in ev:
        if e["event"] == "occupied" and start2 is None:
            start2 = e["t"]
        elif e["event"] == "empty" and start2 is not None:
            occ_s += max(0, min(e["t"], t1) - max(start2, t0)); start2 = None
    if start2 is not None:
        occ_s += max(0, min(now, t1) - max(start2, t0))
    window = max(0, min(now, t1) - t0)
    util = round(100 * occ_s / window, 1) if window > 0 else None
    return {"day": day, "frames": len(fr), "utilization_pct": util, "window_hours": round(window / 3600, 1),
            "events": sum(1 for e in ev if e["event"] in ("in", "out", "occupied", "empty")), "first": hm(fr[0]["t"]) if fr else None, "last": hm(fr[-1]["t"]) if fr else None,
            "occupied_periods": periods, "occupied_minutes": round(sum(p["minutes"] for p in periods), 1),
            "entries": sum(h["in"] for h in per_hour.values()), "exits": sum(h["out"] for h in per_hour.values()),
            "per_hour": dict(sorted(per_hour.items())), "per_door": per_door, "seat_minutes": round(seat_min, 1),
            "people_per_5min": dict(sorted(slots.items()))}


def events_csv(log_dir, day):
    buf = io.StringIO(); w = csv.writer(buf)
    w.writerow(["time", "event", "door", "x_m", "y_m"])
    for e in _lines(os.path.join(log_dir, f"events-{day}.jsonl")):
        if e["event"] == "count":
            continue
        w.writerow([time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(e["t"])), e["event"], e.get("door", ""),
                    e.get("x", ""), e.get("y", "")])
    return buf.getvalue()


if __name__ == "__main__":
    d = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else time.strftime("%Y-%m-%d")
    print(events_csv(sys.argv[1], d) if "--csv" in sys.argv else json.dumps(summary(sys.argv[1], d), indent=1))
