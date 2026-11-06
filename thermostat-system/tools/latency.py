#!/usr/bin/env python3
"""Join the replay CSV (seq, t_write, t_shown optional) with hub.py's CSV (seq, t_mqtt_seq_arrived, t_ha_done, ha_rest_ms)
by seq and print per-stage median / p95 / max in ms. All t_* columns are time.time() seconds on the same Mac.
  latency.py replay.csv hub.csv [--out summary.json]
Stages: radar_to_broker = t_mqtt - t_write; radar_to_ha = t_ha_done - t_write; radar_to_shown = t_shown - t_write (if present);
        ha_rest = ha_rest_ms from the hub. If hub.csv has a seq more than once (heartbeats), the first row counts."""
import csv, json, math, sys, argparse

def pct(v, p):
    s = sorted(v); k = (len(s) - 1) * p / 100.0; lo, hi = math.floor(k), math.ceil(k)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)
def num(x):
    try: return float(x)
    except (TypeError, ValueError): return None
def stats(v):
    return {"n": len(v), "median_ms": round(pct(v, 50), 2), "p95_ms": round(pct(v, 95), 2), "max_ms": round(max(v), 2)} if v else {"n": 0}

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("replay"); ap.add_argument("hub"); ap.add_argument("--out")
    a = ap.parse_args()
    rep = {}
    for r in csv.DictReader(open(a.replay)):
        s = num(r.get("seq"))
        if s is not None and num(r.get("t_write")) is not None: rep.setdefault(int(s), r)
    hub = {}
    for r in csv.DictReader(open(a.hub)):
        s = num(r.get("seq"))
        if s is not None: hub.setdefault(int(s), r)
    both = sorted(set(rep) & set(hub))
    st = {"radar_to_broker": [], "radar_to_ha": [], "radar_to_shown": [], "ha_rest": []}
    for s in both:
        w = num(rep[s]["t_write"]); m = num(hub[s]["t_mqtt_seq_arrived"]); h = num(hub[s]["t_ha_done"]); sh = num(rep[s].get("t_shown"))
        if m is not None: st["radar_to_broker"].append((m - w) * 1000)
        if h is not None: st["radar_to_ha"].append((h - w) * 1000)
        if sh is not None: st["radar_to_shown"].append((sh - w) * 1000)
        if num(hub[s].get("ha_rest_ms")) is not None: st["ha_rest"].append(num(hub[s]["ha_rest_ms"]))
    out = {"replay_rows": len(rep), "hub_rows": len(hub), "joined": len(both),
           "replay_not_in_hub": len(set(rep) - set(hub)), "hub_not_in_replay": len(set(hub) - set(rep)),
           "stages": {k: stats(v) for k, v in st.items()}}
    print("joined %d of %d replay seqs (%d hub rows; %d replay seqs never reached the hub)" % (len(both), len(rep), len(hub), out["replay_not_in_hub"]))
    print("%-18s %6s %10s %10s %10s" % ("stage", "n", "median ms", "p95 ms", "max ms"))
    for k, v in out["stages"].items():
        if v["n"]: print("%-18s %6d %10.1f %10.1f %10.1f" % (k, v["n"], v["median_ms"], v["p95_ms"], v["max_ms"]))
        else: print("%-18s %6d %10s %10s %10s" % (k, 0, "-", "-", "-"))
    if a.out: json.dump(out, open(a.out, "w"), indent=2); print("summary:", a.out)

if __name__ == "__main__": main()
