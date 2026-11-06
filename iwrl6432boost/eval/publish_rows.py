#!/usr/bin/env python3
"""Write the eval platform's rows into data/results/iwrl6432boost.csv: measured where a summary has the number, otherwise
`waiting` with the reason. Idempotent: rows whose variant ends in -1x1, -2x2 or -2x3 (and the mpd2d frame-rate row) are replaced.
Then run: python3 tools/results/check.py && python3 tools/results/compare.py"""
import csv, glob, json, os, sys, datetime
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from render_eval import load, metrics, ORDER  # noqa: E402
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CSVP = os.path.join(ROOT, "data", "results", "iwrl6432boost.csv")
SRC = "iwrl6432boost/eval/results"
TODAY = datetime.date.today().isoformat()
WAIT = "eval tool built; waits for the antenna test run (a reset of the radar, then people passes)"

def main():
    rows = list(csv.DictReader(open(CSVP))); head = list(rows[0].keys())
    keep = [r for r in rows if not (r["source"].startswith(SRC) or r["variant"].endswith(("-1x1", "-2x2", "-2x3")) and r["test"].startswith("radar_") or r["variant"] in ("mpd2d-shipped-cfg-115200", "mpd2d-2x3-115200"))]
    data = load(); new = []
    def add(test, var, status, value="", runs="", cond="", note="", checked="by-us"):
        new.append(dict(board="iwrl6432boost", test=test, variant=var, status=status, value=value, value_hi="", runs=runs, clock_mhz="",
                        conditions=cond, date=TODAY, source=SRC, checked=checked if status == "measured" else "", note=note))
    for c, _ in ORDER:
        M = metrics(data[c]) if c in data else None
        acc = (data[c][0].get("accept") or {}) if c in data else {}
        boot = f"start {acc.get('start_in_boot', '?')} in {acc.get('boot', 'an unrecorded boot')}"
        base = f"Motion and Presence 2D demo, channelCfg for {c}, lowPowerCfg 0, 250 ms frame period; {boot}"
        if M and M["refused"]:
            msg = "; ".join(f'{x["line"]} -> {x["reply"].strip()}' for x in acc["not_done"])
            st, why = "not_supported", f"demo refused this setting ({boot}): {msg}; the CLI hung until reset"
        elif M and M["nostream"]:
            st, why = "failed", f"{boot}: every config line answered Done but no frame followed sensorStart and the demo went silent; cause unknown (second start in the boot, or the setting)"
        else:
            st, why = "waiting", WAIT
        def put(test, var, val, note="", runs=1):
            if st in ("not_supported", "failed") or val is None: add(test, var, st, cond=why, note=note)
            else: add(test, var, "measured", value=val, runs=runs, cond=base, note=note)
        if M and M["walk_on_start"] and M["walk"] is None: add("radar_detect_range", "presence-walking-" + c, "not_run", cond=base, note="the walk started already flagged at 8 m (person at the start mark was detected), so no arrival range; at least 8 m")
        else: put("radar_detect_range", "presence-walking-" + c, M and M["walk"], "distance of a paced walk-in at the first two flagged frames; not the 20 of 20 rule", runs=(M and M["walk_n"]) or "")
        put("radar_detect_range", "presence-seated-" + c, M and M["sit_far"], "farthest tested distance held in 4 of 5 frames over 40 s; not the 20 of 20 rule")
        put("radar_detect_latency", "presence-" + c, M and (None if M["detect"] is None else round(M["detect"] * 1000)), "from the voice cue, includes the person's reaction and walk")
        put("radar_clear_latency", "presence-" + c, M and M["clear"], "from the leave cue, includes the walk out")
        if st == "waiting" and M and M["empty_s"]:
            add("radar_false_alarm", "presence-" + c, "measured", value=round(M["fa"] / M["empty_s"] * 3600, 1), runs=1,
                cond=base + f"; {M['empty_s']:.0f} s empty room, {M['fa']} flags; 95 percent upper bound {3 / M['empty_s'] * 3600:.0f} per hour if none")
        else: add("radar_false_alarm", "presence-" + c, st, cond=why)
        if M and M["rate"] and st == "waiting" and not c.endswith("-both"):
            sc = data[c][0]["stream_check"]
            add("radar_frame_rate", "mpd2d-eval-" + c + "-115200", "measured", value=M["rate"], runs=1,
                cond=f"{base}; {sc['frames']} frames in the first 8 s after start (short check, not 60 s), {sc['gaps']} gaps", note=f"{sc['bytes_per_frame']:g} bytes a frame")
    st_ = [x for S in data.get("stock", []) for x in S.get("passes", {}).get("rate", [])]
    if st_:
        add("radar_frame_rate", "mpd2d-shipped-cfg-115200", "measured", value=st_[0]["rate_hz"], runs=1,
            cond=f"Motion and Presence 2D demo, shipped PresenceDetect.cfg as already running (channelCfg 7 3 0, lowPowerCfg 1), read-only; {st_[0]['frames']} frames in {st_[0]['seconds']} s, {st_[0]['gaps']} gaps", note="not an eval run; configured period 250 ms")
    with open(CSVP, "w", newline="") as f:
        w = csv.DictWriter(f, head, lineterminator="\n"); w.writeheader(); w.writerows(keep + new)
    print(f"{len(new)} rows written, {len(keep)} kept")
main()
