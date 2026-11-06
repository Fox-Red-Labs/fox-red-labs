#!/usr/bin/env python3
"""Find the Hall angle offset from the current minimum, at one or more speeds (our code; the guard bounds every run).
In the firmware's control-run state frlGuard.angleAdj_deg is added to the FOC angle; the angle that gives the least
current for the same speed is where the current lies across the magnet. A constant minimum over speed is a fixed angle
error; one that grows with speed is a delay (degrees = 360 x f x delay).

  python3 tools/frl/lock.py exec --serial AL130001 --op hall-offset -- python3 tools/motorlink/hall_offset.py sweep OUTDIR 20,40,60 [40,50,55,60,65,70,80]
  python3 tools/motorlink/hall_offset.py analyse OUTDIR          (offline: per-step means, fitted minimum, slope; writes OUTDIR/offset.json)

angleAdj_deg goes back to the guard's boot value (link.hall_adj_default, 60 from guard revision H) and the speed cap to 40 Hz after every run.
"""
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign as C
import link as L

STEP_S, FIRST_S, SETTLE_S = 2.5, 11.0, 1.0
NAMES = ["speed_Hz", "Is_A", "motorVars_M1.outVars.Idq_in_A.value[0]", "motorVars_M1.outVars.Idq_in_A.value[1]", "VdcBus_V",
         "frlGuard.state", "motorState"]


def sweep(out, speeds, steps):
    out.mkdir(parents=True, exist_ok=True)
    k = L.Link()
    try:
        for hz in speeds:
            for n, v in (("frlGuard.obsMode", 0), ("frlGuard.handover_Hz", 0.0), ("frlGuard.hallFree", 0), ("frlGuard.angleAdj_deg", 0.0),
                         ("frlGuard.speedMax_Hz", max(40.0, hz)), ("motorVars_M1.inputCtrl.flagEnableFlyingStart", True)):
                k.write(n, v)

            def at(d):
                def go(col):
                    col.mark(f"adj {d}"); C.raw_write(k, "frlGuard.angleAdj_deg", float(d))
                return go
            plan = [(FIRST_S + STEP_S * i, at(d)) for i, d in enumerate(steps)]
            cap = int((FIRST_S + STEP_S * len(steps) + 0.5) * 1000)
            tag = f"hz{int(hz)}"
            try:
                _, _, _, post = C.one_run(k, tag, out, float(hz), cap, plan, NAMES)
            finally:
                k.write("frlGuard.angleAdj_deg", L.hall_adj_default()); k.write("frlGuard.speedMax_Hz", 40.0)
            p = out / f"{tag}.meta.json"
            m = json.loads(p.read_text()); m.update(steps=steps, step_s=STEP_S, first_s=FIRST_S); p.write_text(json.dumps(m))
            if post.get("reason") not in (1, 9):
                raise SystemExit(f"stopped: {tag} ended with guard reason {post.get('reason')}")
    finally:
        try:
            C.raw_write(k, C.RUN, False); k.read(0.2)
            k.write("frlGuard.angleAdj_deg", L.hall_adj_default()); k.write("frlGuard.speedMax_Hz", 40.0)
        finally:
            k.close()


def fit_min(pts):
    """parabola through the lowest point and its neighbours (by angle); None at an edge"""
    pts = sorted(pts)
    i = min(range(len(pts)), key=lambda j: pts[j][1])
    if i == 0 or i == len(pts) - 1:
        return None, pts[i]
    (x0, y0), (x1, y1), (x2, y2) = pts[i - 1], pts[i], pts[i + 1]
    den = (x0 - x1) * (x0 - x2) * (x1 - x2)
    a = (x2 * (y1 - y0) + x1 * (y0 - y2) + x0 * (y2 - y1)) / den
    b = (x2 * x2 * (y0 - y1) + x1 * x1 * (y2 - y0) + x0 * x0 * (y1 - y2)) / den
    if a <= 0:
        return None, pts[i]
    xm = -b / (2 * a)
    c = y1 - a * x1 * x1 - b * x1
    return (round(xm, 1), round(a * xm * xm + b * xm + c, 4)), pts[i]


def analyse(out):
    import campaign_analyse as A
    res = []
    for p in sorted(out.glob("hz*.meta.json"), key=lambda q: int(q.name[2:].split(".")[0])):
        tag = p.name[:-10]
        meta, s = A.load(out, tag)
        t0 = meta["t0"]
        rows = []
        for i, d in enumerate(meta["steps"]):
            a, b = FIRST_S + STEP_S * i + SETTLE_S, FIRST_S + STEP_S * (i + 1)
            w = [v for t, v in s["Is_A"] if a <= t - t0 < b]
            sp = [v for t, v in s["speed_Hz"] if a <= t - t0 < b]
            rows.append({"adj_deg": d, "Is_A": round(sum(w) / len(w), 4) if w else None, "speed_Hz": round(sum(sp) / len(sp), 2) if sp else None, "n": len(w)})
        # repeated angles are averaged for the fit
        by = {}
        for r in rows:
            if r["Is_A"] is not None:
                by.setdefault(r["adj_deg"], []).append(r["Is_A"])
        m, lo = fit_min([(d, sum(v) / len(v)) for d, v in by.items()])
        res.append({"ref_Hz": meta["ref_hz"], "steps": rows, "min_fit": m, "lowest_step": lo, "reason": meta["post"]["reason"]})
    good = [(r["ref_Hz"], r["min_fit"][0]) for r in res if r["min_fit"]]
    slope = None
    if len(good) >= 2:
        n = len(good); mx = sum(x for x, _ in good) / n; my = sum(y for _, y in good) / n
        sxx = sum((x - mx) ** 2 for x, _ in good)
        slope = {"deg_per_Hz": round(sum((x - mx) * (y - my) for x, y in good) / sxx, 3), "offset_at_0Hz_deg": None}
        slope["offset_at_0Hz_deg"] = round(my - slope["deg_per_Hz"] * mx, 1)
        slope["delay_ms"] = round(slope["deg_per_Hz"] / 360 * 1000, 3)
    (out / "offset.json").write_text(json.dumps({"runs": res, "fit": slope}, indent=1))
    for r in res:
        print(f"{r['ref_Hz']:g} Hz:", " ".join(f"{x['adj_deg']:g}:{x['Is_A']}" for x in r["steps"]), "| min", r["min_fit"], "reason", r["reason"])
    print("fit", slope)


if __name__ == "__main__":
    if sys.argv[1] == "sweep":
        out = pathlib.Path(sys.argv[2])
        speeds = [float(x) for x in sys.argv[3].split(",")]
        steps = [float(x) for x in sys.argv[4].split(",")] if len(sys.argv) > 4 else [40, 50, 55, 60, 65, 70, 80]
        sweep(out, speeds, steps)
        analyse(out)
    else:
        analyse(pathlib.Path(sys.argv[2]))
