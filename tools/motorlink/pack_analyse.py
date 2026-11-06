#!/usr/bin/env python3
"""Analyse the 20 V pack runs saved by battery_run.py (raw streams in a results folder). Needs the board connected
only for the variable layout. Usage, under the probe lock:
    python3 tools/motorlink/pack_analyse.py am13e230/foc-drv8376/results/battery-20261018
Writes analysis.json there and prints the numbers that score PREDICTIONS.md."""
import gzip, json, pathlib, statistics, struct, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign as C
import link as L

NAMES = ["speed_Hz", "Is_A", "VdcBus_V", "frlGuard.state"]


def load(d, tag, vs):
    meta = json.loads((d / f"{tag}.meta.json").read_text())
    b = gzip.open(d / f"{tag}.raw.gz").read()
    chunks, i = [], 0
    while i < len(b):
        t, n = struct.unpack("<dI", b[i:i + 12]); chunks.append((t, b[i + 12:i + 12 + n])); i += 12 + n
    data = C.parse(chunks, vs)
    t0 = meta["t0"]
    return {short: [(t - t0, v) for t, v in data[v_["name"]]] for short, v_ in zip(NAMES, vs)}, meta


def window(series, a, b):
    return [v for t, v in series if a <= t < b]


def stats(xs):
    if len(xs) < 5:
        return None
    q = statistics.quantiles(xs, n=20)
    return {"mean": round(statistics.fmean(xs), 4), "p5": round(q[0], 4), "p95": round(q[-1], 4), "n": len(xs)}


def settle_time(speed, t_step, ref, tol=1.0, hold=0.5, horizon=6.0):
    """first time after the step from which the speed stays within tol of ref for hold seconds (1 electrical rev of Hall
    edges per sample, so single samples jump; a 0.1 s moving mean is used)"""
    pts = [(t, v) for t, v in speed if t_step <= t <= t_step + horizon]
    sm = []
    for j, (t, _) in enumerate(pts):
        w = [v for tt, v in pts if t - 0.1 <= tt <= t]
        sm.append((t, sum(w) / len(w)))
    for j, (t, v) in enumerate(sm):
        if all(abs(x - ref) <= tol for tt, x in sm[j:] if tt <= t + hold) and t + hold <= t_step + horizon:
            return round(t - t_step, 3)
    return None


def main():
    d = pathlib.Path(sys.argv[1])
    k = L.Link()
    try:
        vs = [L.find(k.vars, n) for n in NAMES]
    finally:
        k.close()
    out = {}
    for tag, ref, cap in (("pack-fwd-10hz", 10, 15), ("pack-fwd-20hz", 20, 15), ("pack-fwd-40hz", 40, 15), ("pack-rev-20hz", -20, 15)):
        if not (d / f"{tag}.meta.json").exists():
            continue
        s, meta = load(d, tag, vs)
        out[tag] = {k_: stats(window(s[k_], cap - 4, cap)) for k_ in ("speed_Hz", "Is_A", "VdcBus_V")}
        out[tag]["bus_pre_V"] = meta["pre"].get("VdcBus_V")
        out[tag]["time_to_closed_loop_s"] = next((round(t, 2) for t, v in s["speed_Hz"] if t > 0 and abs(v - ref) <= 0.2 * abs(ref)), None)
    if (d / "pack-profile.meta.json").exists():
        s, meta = load(d, "pack-profile", vs)
        steps = [(12.0, 10, 40), (22.0, 40, 10), (30.0, 10, 25)]
        prof = {"bus_pre_V": meta["pre"].get("VdcBus_V"), "steps": []}
        for t_step, a, b in steps:
            before_bus = window(s["VdcBus_V"], t_step - 2, t_step)
            after_bus = window(s["VdcBus_V"], t_step, t_step + 3)
            prof["steps"].append({
                "at_s": t_step, "from_hz": a, "to_hz": b,
                "settle_within_1hz_s": settle_time(s["speed_Hz"], t_step, b),
                "is_before": stats(window(s["Is_A"], t_step - 2, t_step)),
                "is_peak_after": round(max(window(s["Is_A"], t_step, t_step + 3)), 4),
                "is_min_after": round(min(window(s["Is_A"], t_step, t_step + 3)), 4),
                "bus_before_mean": round(statistics.fmean(before_bus), 4),
                "bus_max_after": round(max(after_bus), 4), "bus_min_after": round(min(after_bus), 4),
                "speed_after_3s": stats(window(s["speed_Hz"], t_step + 3, t_step + 6)),
            })
        out["pack-profile"] = prof
    (d / "analysis.json").write_text(json.dumps(out, indent=1) + "\n")
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
