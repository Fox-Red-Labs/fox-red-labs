#!/usr/bin/env python3
"""Rotor inertia and friction on the AM13 bench, unattended (predictions:
am13e230/foc-drv8376/results/mech-ident-20261020/expectations.md). Our code; the guard bounds every run; no firmware change.

  python3 tools/frl/lock.py exec --serial AL130001 --op mech-ident -- python3 tools/motorlink/mech_ident.py ramps OUTDIR
  ... mech_ident.py coast OUTDIR
  ... mech_ident.py lc OUTDIR        the 80 Hz limit cycle grid: observer or firmware speed, Kp 0.05 or 0.025, 20 Hz/s
  ... mech_ident.py steady OUTDIR    20, 40, 60 Hz, three each, on the firmware's own Hall speed: mean Iq for friction
  python3 tools/motorlink/mech_ident.py analyse OUTDIR      no board: writes OUTDIR/analysis.json

Ramps: 20 Hz, then the reference to 80 Hz and back to 20 at 500 Hz/s, IQ and the Hall tracking observer's speed captured at
2.5 kHz; at equal speed friction cancels between the up and down ramps, so J = Kt (Iq_up - Iq_down) / (alpha_up - alpha_down).
Coast: 80 Hz, run flag off, the observer speed, the firmware speed and the raw Hall lines captured at 5 kHz; a fit of
omega(t) = (omega0 + Tc/B) exp(-t B/J) - Tc/B gives Tc/J and B/J.
"""
import json, math, pathlib, statistics as st, sys, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

G = "frlGuard."
KT = 0.0284                      # N m/A, from the measured back-EMF (expectations.md)
HZ_TO_RADS = 2 * math.pi / 4     # electrical Hz to mechanical rad/s, 4 pole pairs
NAMES = ["speed_Hz", "Is_A", "VdcBus_V", "frlGuard.state", "frlGuard.pllSpeed_Hz", "motorState"]
STOP_ALWAYS = {2, 5, 7, 13, 14}


def setup(k, B):
    v = k.board_version()
    if v != 12:
        raise SystemExit(f"guard version {v!r}; these runs were written for 12 (revision H7)")
    cfg = B.configure(k, 0.05, 0.005, 500.0)   # flying start off, default speed gains, 500 Hz/s slew
    k.write(G + "testMode", 0); k.write(G + "hallPll", 1); k.write(G + "speedMax_Hz", 90.0)
    return cfg


def restore(k, B):
    for n, v in list(B.DEFAULT.items()) + [(G + "hallPll", 0), (G + "speedMax_Hz", 40.0)]:
        try:
            k.write(n, v)
        except SystemExit as e:
            print("restore", n, e, flush=True)


def runs(out, kind, n=3):
    import batch1 as B, campaign as C, capture as K, link as L
    k = L.Link(); log = []
    try:
        cfg = setup(k, B)
        for i in range(1, n + 1):
            tag = f"{kind}_{i}"
            if kind == "lc":
                pll, kp = ((1, 0.05), (1, 0.025), (0, 0.05), (0, 0.025))[i - 1]; tag = f"lc80_pll{pll}_kp{kp}"
                B.configure(k, kp, 0.005, 20.0); k.write(G + "hallPll", pll)
                K.arm(k, chans=("IQREF", "SPEED", "IQ", "PLLSPEED"), decim=20, length=2048, pre=2040, trig="stop")
                post = C.one_run(k, tag, out, 80.0, 4500, [], NAMES, tail=0.3)[3]
            elif kind == "steady":
                ref = (20.0, 40.0, 60.0)[(i - 1) % 3]; tag = f"steady_{int(ref)}_{(i - 1) // 3 + 1}"
                k.write(G + "hallPll", 0)
                K.arm(k, chans=("IQ", "SPEED", "IQREF", "PLLSPEED"), decim=20, length=2048, pre=2040, trig="stop")
                post = C.one_run(k, tag, out, ref, 4500, [], NAMES, tail=0.3)[3]
            elif kind == "ramp":
                K.arm(k, chans=("IQ", "PLLSPEED", "IQREF", "SPEED"), decim=8, length=2048, pre=2040, trig="stop")
                plan = [(2.0, lambda col: (col.mark("ref80"), C.raw_write(k, C.SPEEDREF, 80.0))),
                        (2.45, lambda col: (col.mark("ref20"), C.raw_write(k, C.SPEEDREF, 20.0)))]
                post = C.one_run(k, tag, out, 20.0, 2750, plan, NAMES, tail=0.3)[3]
            else:
                K.arm(k, chans=("PLLSPEED", "SPEED", "HALLRAW", "IQ"), decim=4, length=2048, pre=100, trig="stop")
                post = C.one_run(k, tag, out, 80.0, 2500, [], NAMES, tail=0.6)[3]
            try:
                K.wait_done(k, 5.0); K.readout(k, out, tag)
            except SystemExit as e:
                post["capture_error"] = str(e); K.cancel(k)
            log.append(dict(tag=tag, settings=cfg, post=post))
            (out / f"{kind}s.json").write_text(json.dumps(log, indent=1, default=str) + "\n")
            print(tag, "reason", post.get("reason"), "peak", round(post.get("peak_A") or 0, 3), "fault", post.get("faultMtrNow"), flush=True)
            if post.get("reason") in STOP_ALWAYS or post.get("faultMtrNow"):
                raise SystemExit(f"stopped: {tag} ended with reason {post.get('reason')}, fault {post.get('faultMtrNow')}")
            time.sleep(1.5)
    finally:
        C.raw_write(k, C.RUN, False); restore(k, B); k.close()
    return log


def cap(out, tag):
    p = out / f"{tag}.capture.json"
    if not p.exists():
        return None
    d = json.loads(p.read_text())
    return {c: [r[i] for r in d["rows"]] for i, c in enumerate(d["columns"])}


def linfit(x, y):
    mx, my = st.mean(x), st.mean(y)
    sxx = sum((a - mx) ** 2 for a in x)
    b = sum((a - mx) * (c - my) for a, c in zip(x, y)) / sxx
    return b, my - b * mx


def analyse_ramp(c, lo=30.0, hi=70.0):
    t, w, iq = c["t_s"], c["PLLSPEED"], c["IQ"]
    # split at the speed peak: the up ramp before it, the down ramp after
    ipk = max(range(len(w)), key=lambda i: w[i])
    seg = {}
    for name, rng in (("up", range(0, ipk)), ("down", range(ipk, len(w)))):
        idx = [i for i in rng if lo <= w[i] <= hi]
        if len(idx) < 20:
            return None
        slope, _ = linfit([t[i] for i in idx], [w[i] for i in idx])
        seg[name] = dict(n=len(idx), slope_Hzps=round(slope, 1), iq_mean=round(st.mean(iq[i] for i in idx), 4),
                         t_span=round(t[idx[-1]] - t[idx[0]], 4))
    au, ad = seg["up"]["slope_Hzps"] * HZ_TO_RADS, seg["down"]["slope_Hzps"] * HZ_TO_RADS
    diq = seg["up"]["iq_mean"] - seg["down"]["iq_mean"]
    J = KT * diq / (au - ad)
    return dict(seg, diq=round(diq, 4), J=J, iq_friction=round((seg["up"]["iq_mean"] + seg["down"]["iq_mean"]) / 2, 4),
                Tf=KT * seg["up"]["iq_mean"] - J * au)


def hall_edges(t, h, t_from=0.0):
    """mechanical speed from raw Hall edges: 24 edges per turn, so each interval is 2 pi / 24 rad"""
    e = [t[i] for i in range(1, len(h)) if h[i] != h[i - 1] and t[i] >= t_from]
    return [((a + b) / 2, (2 * math.pi / 24) / (b - a)) for a, b in zip(e, e[1:]) if b > a], e


def fit_coast(pts):
    """least squares for d omega / dt = -(a + b omega) on (t, omega) points, by a grid on b and a closed form for the rest"""
    best = None
    for k in range(1, 400):
        b = k * 0.1   # 1/s
        # omega(t) = (w0 + a/b) e^{-bt} - a/b: linear in (w0 + a/b) and a/b for a fixed b
        xs = [math.exp(-b * t) for t, _ in pts]
        A11 = sum(x * x for x in xs); A12 = -sum(xs); A22 = len(xs)
        r1 = sum(x * w for x, (_, w) in zip(xs, pts)); r2 = -sum(w for _, w in pts)
        det = A11 * A22 - A12 * A12
        if abs(det) < 1e-12:
            continue
        P = (r1 * A22 - A12 * r2) / det; Q = (A11 * r2 - A12 * r1) / det
        sse = sum((P * x - Q - w) ** 2 for x, (_, w) in zip(xs, pts))
        if best is None or sse < best[0]:
            best = (sse, b, P, Q)
    if best is None:
        return None
    sse, b, P, Q = best
    return dict(B_over_J=round(b, 2), Tc_over_J=round(Q * b, 1), omega0=round(P - Q, 2), rms=round(math.sqrt(sse / len(pts)), 3))


def analyse_coast(c):
    t, h = c["t_s"], c["HALLRAW"]
    pts, edges = hall_edges(t, h, 0.0)
    pts = [(a, w) for a, w in pts if a > 0.002]
    if len(pts) < 5:
        return None
    f = fit_coast(pts)
    if f is None:
        return None
    return dict(f, n_edges=len(edges), coast_s=round(edges[-1], 4) if edges else None,
                first_w=round(pts[0][1], 2), last_w=round(pts[-1][1], 2))


def analyse(out):
    out = pathlib.Path(out); res = {"ramps": {}, "coasts": {}}
    for i in (1, 2, 3):
        c = cap(out, f"ramp_{i}")
        if c:
            res["ramps"][i] = analyse_ramp(c)
        c = cap(out, f"coast_{i}")
        if c:
            res["coasts"][i] = analyse_coast(c)
    Js = [r["J"] for r in res["ramps"].values() if r]
    if Js:
        res["J_mean"] = st.mean(Js)
    cs = [r for r in res["coasts"].values() if r]
    if Js and cs:
        J = res["J_mean"]
        res["Tc"] = st.mean(r["Tc_over_J"] for r in cs) * J
        res["B"] = st.mean(r["B_over_J"] for r in cs) * J
    (out / "analysis.json").write_text(json.dumps(res, indent=1, default=str) + "\n")
    print(json.dumps(res, indent=1, default=str))
    return res


if __name__ == "__main__":
    what, o = sys.argv[1], pathlib.Path(sys.argv[2]); o.mkdir(parents=True, exist_ok=True)
    if what == "analyse":
        analyse(o)
    else:
        runs(o, {"ramps": "ramp", "coast": "coast", "steady": "steady", "lc": "lc"}[what], n={"steady": 9, "lc": 4}.get(what, 3))
