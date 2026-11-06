#!/usr/bin/env python3
"""First runs on the 20 V tool pack, scored against
am13e230/foc-drv8376/results/battery-20261018/PREDICTIONS.md. Run under the probe lock:

    python3 tools/frl/lock.py exec --serial AL130001 -- python3 tools/motorlink/battery_run.py preflight
    python3 tools/frl/lock.py exec --serial AL130001 -- python3 tools/motorlink/campaign.py cal
    python3 tools/frl/lock.py exec --serial AL130001 -- python3 tools/motorlink/battery_run.py runs OUTDIR

preflight sets the pack window (15.5 to 21 V) and the 1.0 A phase trip, then reads them back with the
guard version, bus, fault word and Hall calibration, and prints GO or NO GO. runs does three 15 s forward
runs at 10, 20 and 40 Hz electrical with the bus logged, then writes summary.json. No reversals.
"""
import json, pathlib, statistics, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign as C
import link as L

WINDOW = (15.5, 21.0)
LIMIT_A = 1.0
NAMES = ["speed_Hz", "Is_A", "VdcBus_V", "frlGuard.state"]
STEPS_HZ = (10.0, 20.0, 40.0)


def set_limits(k):
    k.write("frlGuard.busMin_V", WINDOW[0]); k.write("frlGuard.busMax_V", WINDOW[1]); k.write("frlGuard.limit_A", LIMIT_A)
    return C.snapshot(k, ["frlGuard.version", "frlGuard.busMin_V", "frlGuard.busMax_V", "frlGuard.limit_A", "frlGuard.state",
                          "VdcBus_V", "faultMtrNow", "hallSensorData.flagCalValid"], 0.5)


def preflight(k):
    s = set_limits(k)
    checks = {
        f"guard version {L.guard_version()}": s.get("version") == L.guard_version(),
        "pack window set": abs(s.get("busMin_V", 0) - WINDOW[0]) < 0.01 and abs(s.get("busMax_V", 0) - WINDOW[1]) < 0.01,
        "trip at 1.0 A": abs(s.get("limit_A", 0) - LIMIT_A) < 0.01,
        "bus inside the window": WINDOW[0] <= s.get("VdcBus_V", 0) <= WINDOW[1],
        "guard idle": s.get("state") in (0, 3),
    }
    for name, ok in checks.items():
        print(f"{'ok  ' if ok else 'FAIL'} {name}")
    print("bus", round(s.get("VdcBus_V", 0), 2), "V; fault word", s.get("faultMtrNow"),
          "(a latched under-voltage from power-up is cleared before a run); Hall table",
          "calibrated" if s.get("flagCalValid") else "NOT calibrated: run campaign.py cal next")
    print("GO" if all(checks.values()) else "NO GO")


def summarise(col, vs, t0, cap_s):
    """mean and spread of each logged signal over the last 4 s before the cap (steady state)"""
    data = C.parse(col.chunks, vs)
    out = {}
    for short in NAMES[:3]:
        full = next((nm for nm in data if nm == short or nm.endswith("." + short)), None)
        xs = [v for t, v in data.get(full, []) if t0 + cap_s - 4 <= t <= t0 + cap_s] if full else []   # steady state: the last 4 s (the firmware aligns and ramps for about 7 s)
        allv = [v for _, v in data.get(full, [])] if full else []
        if len(xs) < 20:
            out[short] = {"n": len(xs)}
            continue
        q = statistics.quantiles(xs, n=20)
        out[short] = {"mean": round(statistics.fmean(xs), 4), "p5": round(q[0], 4), "p95": round(q[-1], 4),
                      "min_all": round(min(allv), 4), "max_all": round(max(allv), 4), "n": len(xs)}
    return out


def safe_summary(col, vs, t0):
    try:
        return summarise(col, vs, t0, 15.0)
    except Exception as e:   # the raw stream is already saved by one_run; a summary bug must not stop the series
        return {"error": repr(e)}


def runs(k, outdir):
    pathlib.Path(outdir).mkdir(parents=True, exist_ok=True)
    set_limits(k)
    summary = {}
    for hz in STEPS_HZ:
        tag = f"pack-fwd-{int(hz)}hz"
        col, vs, t0, post = C.one_run(k, tag, outdir, hz, 15000, [], NAMES)
        summary[tag] = {"ref_hz": hz, "post": post, "pre_bus_V": C.PRE.get("VdcBus_V"), "stats": safe_summary(col, vs, t0)}
        print(tag, json.dumps(summary[tag]["stats"]), flush=True)
        if post.get("reason") not in (1, 9):   # anything but the cap or a host stop: stop the series
            print("run ended by", post.get("reason"), "- series stopped", flush=True)
            break
    pathlib.Path(outdir, "summary.json").write_text(json.dumps(summary, indent=1) + "\n")
    print("summary", pathlib.Path(outdir, "summary.json"))


def profile(k, outdir):
    """one run: 10 Hz, up to 40 Hz at 12 s, down to 10 Hz at 22 s (a regenerative brake), up to 25 Hz at 30 s, cap at 36 s"""
    set_limits(k)
    def ref(hz):
        return lambda col: (col.mark(f"ref {hz}"), C.raw_write(k, C.SPEEDREF, hz))
    C.one_run(k, "pack-profile", outdir, 10.0, 36000, [(12.0, ref(40.0)), (22.0, ref(10.0)), (30.0, ref(25.0))], NAMES)


def reverse(k, outdir):
    """from standstill straight to -20 Hz (not a reversal through zero, which faults: the firmware takes direction from the reference)"""
    set_limits(k)
    col, vs, t0, post = C.one_run(k, "pack-rev-20hz", outdir, -20.0, 15000, [], NAMES)
    print("pack-rev-20hz", json.dumps(safe_summary(col, vs, t0)), flush=True)


def main():
    k = L.Link()
    try:
        if sys.argv[1] == "preflight":
            preflight(k)
        elif sys.argv[1] == "profile":
            profile(k, sys.argv[2])
        elif sys.argv[1] == "reverse":
            reverse(k, sys.argv[2])
        elif sys.argv[1] == "runs":
            runs(k, sys.argv[2])
        else:
            raise SystemExit(__doc__)
    finally:
        k.close()


if __name__ == "__main__":
    main()
