#!/usr/bin/env python3
"""Score the guard revision H5 runs against am13e230/foc-drv8376/results/guard-revH5-20261019/expectations.md.

  python3 tools/motorlink/h5_analyse.py OUTDIR      writes OUTDIR/analysis.json and prints the table
"""
import json, math, pathlib, statistics as st, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))


def cap(out, tag):
    p = out / f"{tag}.capture.json"
    if not p.exists():
        return None
    d = json.loads(p.read_text())
    cols = d["columns"]
    return {c: [r[i] for r in d["rows"]] for i, c in enumerate(cols)}


def mean_in(t, x, a, b):
    v = [xi for ti, xi in zip(t, x) if a <= ti <= b]
    return (st.mean(v), len(v)) if v else (None, 0)


def ident_R(out):
    c = cap(out, "R_holds")
    if not c:
        return {"error": "no R_holds capture"}
    t, ia, va, vb, vc = c["t_s"], c["IA"], c["VA"], c["VB"], c["VC"]
    u = [a - (b + cc) / 2 for a, b, cc in zip(va, vb, vc)]
    w = {"0p3": (1.1, 2.0), "0p6": (2.7, 3.6)}
    m = {}
    for k, (a, b) in w.items():
        m[k] = {"I": mean_in(t, ia, a, b)[0], "u": mean_in(t, u, a, b)[0], "n": mean_in(t, ia, a, b)[1]}
    if None in (m["0p3"]["I"], m["0p6"]["I"]):
        return {"error": "windows empty", "means": m}
    dI = m["0p6"]["I"] - m["0p3"]["I"]; du = m["0p6"]["u"] - m["0p3"]["u"]
    R = du / (1.5 * dI)
    icpt = m["0p3"]["u"] - 1.5 * R * m["0p3"]["I"]
    return {"means": m, "R_ohm": round(R, 4), "intercept_V": round(icpt, 4), "R_simple_0p3": round(m["0p3"]["u"] / (1.5 * m["0p3"]["I"]), 4),
            "R_simple_0p6": round(m["0p6"]["u"] / (1.5 * m["0p6"]["I"]), 4)}


def ident_L(out, tag, R):
    c = cap(out, tag)
    if not c:
        return {"error": f"no {tag} capture"}
    t, i = c["t_s"], c["ID"]
    n = len(i)
    # the step: the first sample where the current has risen 5 percent of the final swing above the start level
    i0 = st.mean(i[:200]); i1 = st.mean(i[-300:]); sw = i1 - i0
    if abs(sw) < 0.05:
        return {"error": "no step seen", "i0": i0, "i1": i1}
    def cross(f):
        th = i0 + f * sw
        for k in range(1, n):
            if (i[k - 1] - th) * (i[k] - th) <= 0 and (i[k] - i0) * sw > 0:
                return t[k - 1] + (th - i[k - 1]) / (i[k] - i[k - 1]) * (t[k] - t[k - 1]) if i[k] != i[k - 1] else t[k]
        return None
    t10, t90 = cross(0.1), cross(0.9)
    tau = (t90 - t10) / math.log(9) if t10 is not None and t90 is not None else None
    return {"i0_A": round(i0, 4), "i_final_A": round(i1, 4), "t10_s": t10, "t90_s": t90, "tau_ms": round(tau * 1e3, 3) if tau else None,
            "Ld_mH": round(tau * R * 1e3, 3) if tau and R else None}


def main(out):
    out = pathlib.Path(out); a = {}
    if (out / "boot.json").exists():
        a["boot"] = json.loads((out / "boot.json").read_text())
    if (out / "protect.json").exists():
        a["protect"] = [{k: r[k] for k in ("tag", "angle", "limit", "cmp", "ilim")} | {"reason": r["post"].get("reason"), "peak_A": r["post"].get("peak_A"),
                        "fault": r["post"].get("faultMtrNow"), "nFault": r["post"].get("frlGuard.nFault")} for r in json.loads((out / "protect.json").read_text())]
    if (out / "R_holds.capture.json").exists():
        a["R"] = ident_R(out)
        R = a["R"].get("R_ohm")
        a["L"] = {tag: ident_L(out, tag, R) for tag in ("L_step1", "L_step2")}
    if (out / "direction.json").exists():
        a["direction"] = [{"tag": r["tag"], "dirRotor": r["dirRotor"], "reason": r["post"].get("reason"), "peak_A": r["post"].get("peak_A"),
                           "flips": r["post"].get("frlGuard.dirFlips")} for r in json.loads((out / "direction.json").read_text())]
    (out / "analysis.json").write_text(json.dumps(a, indent=1, default=str) + "\n")
    print(json.dumps(a, indent=1, default=str))


if __name__ == "__main__":
    main(sys.argv[1])
