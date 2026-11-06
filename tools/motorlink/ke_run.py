#!/usr/bin/env python3
"""Back-EMF constant from the running phase voltages (predictions: am13e230/foc-drv8376/results/ke-20261019/expectations.md).
Our code; the guard bounds the run. One run: Hall start with the Hall offset applied, 4 s at each reference, phase voltages logged.

  python3 tools/frl/lock.py exec --serial AL130001 --op ke -- python3 tools/motorlink/ke_run.py run OUTDIR [20,40,60]
  python3 tools/motorlink/ke_run.py analyse OUTDIR
"""
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

STEP_S, FIRST_S = 4.0, 1.5
ADJ = 57.0
NAMES = ["speed_Hz", "Is_A", "halFOCDataObj_M1.adcData.V_V.value[0]", "halFOCDataObj_M1.adcData.V_V.value[1]",
         "halFOCDataObj_M1.adcData.V_V.value[2]", "VdcBus_V", "frlGuard.state"]
RPM_PER_HZ = 15.0


def run(out, speeds):
    import campaign as C, link as L
    out.mkdir(parents=True, exist_ok=True)
    k = L.Link()
    try:
        idle = C.snapshot(k, NAMES[2:6], 0.6)
        for n, v in (("motorVars_M1.inputCtrl.flagEnableFlyingStart", False), ("motorSetVars_M1.accelerationMax_Hzps", 100.0),
                     ("frlGuard.angleAdj_deg", ADJ), ("frlGuard.speedMax_Hz", max(40.0, max(speeds)))):
            k.write(n, v)

        def at(hz):
            def go(col):
                col.mark(f"ref {hz}"); C.raw_write(k, C.SPEEDREF, float(hz))
            return go
        plan = [(FIRST_S + STEP_S * i, at(hz)) for i, hz in enumerate(speeds[1:], 1)]
        cap = int((FIRST_S + STEP_S * len(speeds)) * 1000)
        try:
            _, _, _, post = C.one_run(k, "ke", out, float(speeds[0]), cap, plan, NAMES, tail=0.3)
        finally:
            for n, v in (("motorVars_M1.inputCtrl.flagEnableFlyingStart", True), ("motorSetVars_M1.accelerationMax_Hzps", 20.0),
                         ("frlGuard.speedMax_Hz", 40.0)):
                k.write(n, v)
        p = out / "ke.meta.json"
        m = json.loads(p.read_text()); m.update(speeds=speeds, step_s=STEP_S, first_s=FIRST_S, idle_V=idle, angleAdj_deg=ADJ); p.write_text(json.dumps(m))
        print("post", post, "idle", idle)
    finally:
        k.close()


def pct(xs, q):
    xs = sorted(xs); return xs[min(len(xs) - 1, max(0, int(q * (len(xs) - 1))))]


def analyse(out):
    import campaign_analyse as A
    meta, s = A.load(out, "ke")
    t0 = meta["t0"]
    # campaign_analyse shortens names: V_V.value[0..2] arrive as Ia/Ib/Ic keys there; map back by position
    keys = [k for k in s if k not in ("speed_Hz", "Is_A", "VdcBus_V", "state")]
    va, vb, vc = (s[k] for k in keys[:3])
    rows = []
    for i, hz in enumerate(meta["speeds"]):
        a, b = FIRST_S + STEP_S * i + 1.0, FIRST_S + STEP_S * (i + 1)
        A_ = [(t, v) for t, v in va if a <= t - t0 < b]; B_ = [(t, v) for t, v in vb if a <= t - t0 < b]
        n = min(len(A_), len(B_))
        vab = [A_[j][1] - B_[j][1] for j in range(n)]
        sp = [v for t, v in s["speed_Hz"] if a <= t - t0 < b]
        isa = [v for t, v in s["Is_A"] if a <= t - t0 < b]
        amp = (pct(vab, 0.98) - pct(vab, 0.02)) / 2 if vab else None
        rows.append({"ref_Hz": hz, "speed_Hz": round(sum(sp) / len(sp), 2), "rpm": round(sum(sp) / len(sp) * RPM_PER_HZ, 0),
                     "Is_A": round(sum(isa) / len(isa), 4), "Vab_amp_V": round(amp, 3), "Vab_mean_V": round(sum(vab) / n, 3), "n": n,
                     "Va_range_V": [round(pct([v for _, v in A_], 0.02), 2), round(pct([v for _, v in A_], 0.98), 2)]})
    x = [r["rpm"] / 1000 for r in rows]; y = [r["Vab_amp_V"] for r in rows]
    nn = len(x); mx = sum(x) / nn; my = sum(y) / nn
    sxx = sum((u - mx) ** 2 for u in x); sxy = sum((u - mx) * (w - my) for u, w in zip(x, y))
    slope = sxy / sxx; icpt = my - slope * mx
    ss = sum((w - my) ** 2 for w in y); sr = sum((w - (icpt + slope * u)) ** 2 for u, w in zip(x, y))
    fit = {"V_per_krpm_LL_peak": round(slope, 3), "intercept_V": round(icpt, 3), "r2": round(1 - sr / ss, 4) if ss else None,
           "datasheet_V_per_krpm": 2.45}
    (out / "ke.json").write_text(json.dumps({"rows": rows, "fit": fit, "idle_V": meta.get("idle_V"), "post": meta["post"]}, indent=1))
    for r in rows:
        print(r)
    print("fit", fit, "idle", meta.get("idle_V"))


if __name__ == "__main__":
    if sys.argv[1] == "run":
        out = pathlib.Path(sys.argv[2])
        run(out, [float(x) for x in (sys.argv[3] if len(sys.argv) > 3 else "20,40,60").split(",")])
        analyse(out)
    else:
        analyse(pathlib.Path(sys.argv[2]))
