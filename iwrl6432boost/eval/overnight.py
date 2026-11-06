#!/usr/bin/env python3
"""False alarms per hour for the radar and the Wi-Fi presence model from an empty-house free run.

  python3 iwrl6432boost/eval/overnight.py --start "2026-10-10 23:30" --end "2026-10-11 05:54" \
      [--radar thermostat-system/results/feeder-frames-2026-10-10.csv] [--wifi cc35x1/csi/results/2026-10-10-freerun.csv]

Reads the feeder's frame log (seq, t_frame, presence, pts301) and the Wi-Fi logger's rows (t, iso, cls, p1).
A false alarm is a run of presence-on frames; runs less than 2 s apart merge (same rule as radar_eval.py).
With zero runs the 95 percent upper bound is 3 divided by the window in hours (rule of three).
Prints a summary and writes it beside the frame log as <log>.overnight.json. Times are Mac local time.
"""
import argparse, csv, datetime as dt, json, pathlib, statistics

def ts(s):
    return dt.datetime.strptime(s, "%Y-%m-%d %H:%M").timestamp()

def runs_of(frames, gap=2.0):
    """frames: list of (t, on). Returns [(t_start, t_end, n)] for on-runs, merging gaps under `gap` s."""
    out, cur = [], None
    for t, on in frames:
        if on:
            if cur and t - cur[1] < gap:
                cur[1] = t; cur[2] += 1
            elif cur is None:
                cur = [t, t, 1]
            else:
                out.append(tuple(cur)); cur = [t, t, 1]
    if cur:
        out.append(tuple(cur))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--start", required=True); ap.add_argument("--end", required=True)
    ap.add_argument("--radar", default="thermostat-system/results/feeder-frames-2026-10-10.csv")
    ap.add_argument("--wifi", default="cc35x1/csi/results/2026-10-10-freerun.csv")
    a = ap.parse_args()
    t0, t1 = ts(a.start), ts(a.end)
    hours = (t1 - t0) / 3600
    res = {"window_start": a.start, "window_end": a.end, "window_hours": round(hours, 2)}
    # radar
    fr = []
    with open(a.radar) as f:
        for x in csv.DictReader(f):
            try:
                t = float(x["t_frame"])
            except ValueError:
                continue
            if t0 <= t < t1:
                fr.append((int(x["seq"]), t, int(x["presence"]), int(x["pts301"] or 0)))
    gaps = [fr[i][0] - fr[i - 1][0] for i in range(1, len(fr))]
    dts = [fr[i][1] - fr[i - 1][1] for i in range(1, len(fr))]
    rr = runs_of([(t, p) for s, t, p, n in fr])
    r = {"frames": len(fr), "frames_per_s": round(len(fr) / (t1 - t0), 3) if fr else None,
         "seq_gaps": sum(1 for g in gaps if g > 1), "largest_seq_gap": max(gaps) if gaps else None,
         "median_frame_period_s": round(statistics.median(dts), 4) if dts else None,
         "presence_frames": sum(p for s, t, p, n in fr), "frames_with_points": sum(1 for s, t, p, n in fr if n > 0),
         "false_alarm_runs": len(rr), "false_alarms_per_h": round(len(rr) / hours, 3),
         "upper_95_per_h_if_zero": round(3 / hours, 3)}
    res["radar"] = r
    # first flag after the window (the return home)
    after = []
    with open(a.radar) as f:
        for x in csv.DictReader(f):
            try:
                t = float(x["t_frame"])
            except ValueError:
                continue
            if t >= t1 and int(x["presence"]) == 1:
                after.append(t); break
    res["radar"]["first_flag_after_window"] = dt.datetime.fromtimestamp(after[0]).strftime("%Y-%m-%d %H:%M:%S") if after else None
    # wifi
    wf = []
    p = pathlib.Path(a.wifi)
    if p.exists():
        with open(p) as f:
            for x in csv.DictReader(f):
                t = float(x["t"])
                if t0 <= t < t1:
                    wf.append((t, int(x["cls"]), float(x["p1"])))
        wr = runs_of([(t, c == 1) for t, c, pr in wf], gap=5.0)
        w = {"results": len(wf), "present_results": sum(1 for t, c, pr in wf if c == 1),
             "last_result": dt.datetime.fromtimestamp(wf[-1][0]).strftime("%Y-%m-%d %H:%M:%S") if wf else None,
             "logged_hours": round((wf[-1][0] - wf[0][0]) / 3600, 2) if wf else 0,
             "present_share": round(sum(1 for t, c, pr in wf if c == 1) / len(wf), 3) if wf else None,
             "false_alarm_runs": len(wr),
             "false_alarms_per_logged_h": round(len(wr) / ((wf[-1][0] - wf[0][0]) / 3600), 3) if len(wf) > 1 else None,
             "median_p1": round(statistics.median([pr for t, c, pr in wf]), 3) if wf else None}
        res["wifi"] = w
    out = pathlib.Path(a.radar).with_suffix(".overnight.json")
    out.write_text(json.dumps(res, indent=1) + "\n")
    print(json.dumps(res, indent=1)); print("wrote", out)

if __name__ == "__main__":
    main()
