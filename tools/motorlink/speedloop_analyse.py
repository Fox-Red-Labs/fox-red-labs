#!/usr/bin/env python3
"""Step metrics for campaign.py gstep runs (our code). Speed is taken from the Hall edges: each edge's time comes from
the polled edge counters (about 3 ms resolution at 7 variables), and the speed at an edge is one electrical revolution
(6 edges, all three lines) over the time those 6 edges took, so Hall placement error averages out. TI's speed_Hz is
reported beside it. Band: 0.5 Hz + 5 percent of the target (the live panel's band). Times are measured from the
onset (the first 1-rev speed above lo + 10 percent of the step), because the host's write time is known only to the
serial bridge's burst gap (about 65 ms).

  python3 tools/motorlink/speedloop_analyse.py OUTDIR TAG [TAG ...]
"""
import sys, json, pathlib, statistics as st
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign_analyse as A

PRED = {"rise_ms": 11.2, "overshoot_pct": 14.1, "settle_ms": 86}


def edge_times(ser):
    tot = A.edges_total(ser); out = []; last = 0
    for t, n in tot:
        if n > last:
            out += [t] * (n - last); last = n
    return out


def analyse(outdir, tag):
    meta, ser = A.load(outdir, tag); t0 = meta["t0"]; lo, hi = meta.get("lo", 10.0), meta.get("hi", 15.0)
    et = edge_times(ser)
    sp = [(et[i], 1.0 / (et[i] - et[i - 6])) for i in range(6, len(et)) if et[i] > et[i - 6]]
    rel = [(t - t0, v) for t, v in sp]
    pre = [v for t, v in rel if 12.0 <= t < 13.9]
    d = hi - lo; band = 0.5 + 0.05 * hi
    onset = next((t for t, v in rel if t > 13.9 and v > lo + 0.1 * d), None)
    if onset is None: return {"tag": tag, "error": "no step seen"}
    t90 = next((t for t, v in rel if t > onset and v >= lo + 0.9 * d), None)
    after = [(t - onset, v) for t, v in rel if onset <= t <= onset + 1.0]
    peak = max(v for _, v in after)
    out = [t for t, v in after if abs(v - hi) > band]
    settle = (max(out) if out else 0.0)
    win = [(t, v) for t, v in after if t >= PRED["settle_ms"] / 1000]
    inb = sum(1 for _, v in win if abs(v - hi) <= band) / len(win) if win else None
    worst = max(abs(v - hi) for _, v in win) if win else None
    post = [v for t, v in rel if onset + 1.0 <= t <= onset + 5.5]
    ti = [v for t, v in ser["speed_Hz"] if onset + 0.3 <= t - t0 <= onset + 5.5]
    states = sorted({v for t, v in ser["motorState"] if 12.0 <= t - t0 <= 19.0})
    pct = lambda xs, q: sorted(xs)[min(len(xs) - 1, int(q * len(xs)))]
    return {"tag": tag, "kp": meta.get("kp"), "ki": meta.get("ki"), "states_12_19s": states,
            "pre_mean_hz": round(st.mean(pre), 2), "pre_p5_p95_hz": [round(pct(pre, .05), 2), round(pct(pre, .95), 2)],
            "rise_10_90_ms": round((t90 - onset) * 1000) if t90 else None,
            "overshoot_pct": round(100 * (peak - hi) / d, 1), "settle_ms": round(settle * 1000),
            "in_band_after_86ms": round(inb, 3) if inb is not None else None, "worst_dev_hz": round(worst, 2) if worst else None,
            "post_mean_hz": round(st.mean(post), 2), "post_p5_p95_hz": [round(pct(post, .05), 2), round(pct(post, .95), 2)],
            "ti_speed_max_hz": round(max(ti), 1) if ti else None, "peak_A": meta["post"].get("peak_A"),
            "fault": meta["post"].get("faultMtrNow"), "end_reason": meta["post"].get("reason")}


if __name__ == "__main__":
    rows = [analyse(sys.argv[1], t) for t in sys.argv[2:]]
    for r in rows: print(json.dumps(r))
    p = pathlib.Path(sys.argv[1]) / "summary.json"; p.write_text(json.dumps({"prediction_20hz": PRED, "runs": rows}, indent=1))
