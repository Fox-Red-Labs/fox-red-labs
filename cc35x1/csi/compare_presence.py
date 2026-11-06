#!/usr/bin/env python3
"""Join the CC35X1 Wi-Fi CSI free-run log with the radar eval pass log on host time (epoch seconds, same Mac clock).

  python3 cc35x1/csi/compare_presence.py [--date 2026-10-10] [--config 2x3] [--csi <freerun.csv>] [--radar <radar csv>]

Reads iwrl6432boost/eval/results/<date>-<config>.csv (columns: config,pass,round,t,frame,pres,...,event; event rows carry
cue_enter / cue_go / mark_* / cue_leave) and cc35x1/csi/results/<date>-freerun.csv (t,iso,cls,p1). For each radar pass
and round it reports, inside the pass window (first to last radar row): on-fraction of each sensor, first detect after the
enter cue and first clear after the leave cue (clear = first off that stays off for 3 s, as radar_eval.py does).
Read-only: touches no board and no port.
"""
import argparse, csv, datetime, glob, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
HOLD = 3.0

def first_on(rows, t0, t1):
    for t, on in rows:
        if t0 <= t <= t1 and on: return t
def first_clear(rows, t_from, hold=HOLD):
    s = [(t, on) for t, on in rows if t >= t_from]
    for i, (t, on) in enumerate(s):
        if not on and s[-1][0] >= t + hold and all(not o for u, o in s[i:] if u <= t + hold): return t
def frac(rows, t0, t1):
    w = [on for t, on in rows if t0 <= t <= t1]
    return (sum(w) / len(w), len(w)) if w else (None, 0)
def lat(t, ref): return round(t - ref, 1) if t is not None and ref is not None else None
def pct(x): return "n/a" if x is None else "%d%%" % round(100 * x)

def load_radar(path):
    P = {}
    for r in csv.DictReader(open(path)):
        d = P.setdefault((r["pass"], r["round"]), {"rows": [], "ev": {}})
        if r["event"]: d["ev"][r["event"]] = float(r["t"])
        elif r["pres"] != "": d["rows"].append((float(r["t"]), r["pres"] == "1"))
    return P

SETTLE = 6.0   # s after each cue not scored (person is moving in or out; the board averages 3 windows = 6 s)
STEP = 0.5
CONFIRM = 5.0  # s: radar must agree within this of a Wi-Fi flag

def at(rows, ts, t, stale):
    """Latest value at or before t (None if older than stale seconds). rows sorted; ts = their times."""
    import bisect
    i = bisect.bisect_right(ts, t) - 1
    return rows[i][1] if i >= 0 and t - rows[i][0] <= stale else None

def any_on(rows, ts, t0, t1):
    import bisect
    return any(on for _, on in rows[bisect.bisect_left(ts, t0):bisect.bisect_right(ts, t1)])

def label_at(p, ev, t):
    """1 inside the person-present span, 0 outside, None inside a settle band. Empty passes are all 0."""
    if p == "empty": return 0
    a = ev.get("cue_enter", ev.get("cue_go")); b = ev.get("cue_leave")
    if a is None or b is None: return None
    if abs(t - a) < SETTLE or abs(t - b) < SETTLE: return None
    return 1 if a < t < b else 0

def fusion(passes_data, csi):
    """passes_data: list of (pass, round, rows, ev, t0, t1). Returns per-rule counts, per pass and total."""
    cts = [t for t, _ in csi]
    rules = ["radar", "wifi", "OR", "AND", "radar_confirms_wifi"]
    tot = {r: {"miss": 0, "pos": 0, "fa": 0, "neg": 0} for r in rules}
    per = []
    for p, rnd, rows, ev, t0, t1 in passes_data:
        rts = [t for t, _ in rows]
        c = {r: {"miss": 0, "pos": 0, "fa": 0, "neg": 0} for r in rules}
        t = t0
        while t <= t1:
            lab = label_at(p, ev, t)
            rv, cv = at(rows, rts, t, 1.5), at(csi, cts, t, 4.0)
            if lab is not None and rv is not None and cv is not None:
                r_, c_ = bool(rv), bool(cv)
                v = {"radar": r_, "wifi": c_, "OR": r_ or c_, "AND": r_ and c_,
                     "radar_confirms_wifi": c_ and any_on(rows, rts, t - CONFIRM, t + CONFIRM)}
                for k, x in v.items():
                    key = ("pos", "miss") if lab == 1 else ("neg", "fa")
                    c[k][key[0]] += 1; tot[k][key[0]] += 1
                    if (lab == 1 and not x) or (lab == 0 and x): c[k][key[1]] += 1; tot[k][key[1]] += 1
            t += STEP
        per.append((p, rnd, c))
    return per, tot

def fr(c, a, b): return "-" if not c[b] else "%d%%" % round(100 * c[a] / c[b])

def extras(pdata, csi_full, rp):
    """Agreement per pass, threshold sweep on the model's probability, and a 2 s timeline per pass."""
    csi = [(t, c) for t, c, _ in csi_full]; cts = [t for t, _ in csi]
    p1 = [(t, v) for t, _, v in csi_full]
    print("\nAgreement per pass, on the 0.5 s grid (Wi-Fi uses the model's own class). 'both on' = radar present and Wi-Fi present; 'Wi-Fi on, radar off' = disagreement; Wi-Fi off = the model said empty.\n")
    print("| pass | moments | both on | Wi-Fi on, radar off | radar on, Wi-Fi off | both off |"); print("|---|---|---|---|---|---|")
    for p, rnd, rows, ev, t0, t1 in pdata:
        rts = [t for t, _ in rows]; n = bb = wo = ro = nn = 0; t = t0
        while t <= t1:
            r, c = at(rows, rts, t, 1.5), at(csi, cts, t, 4.0)
            if r is not None and c is not None:
                n += 1; bb += r and c; wo += c and not r; ro += r and not c; nn += (not r) and (not c)
            t += STEP
        f = lambda x: "%d%%" % round(100 * x / n) if n else "-"
        print("| %s %s | %d | %s | %s | %s | %s |" % (p, rnd, n, f(bb), f(wo), f(ro), f(nn)))
    print("\nThreshold sweep: Wi-Fi counts as present when p1 is at least the threshold. Empty = the empty-room pass; seated = the two seated passes between the enter and leave cues (6 s settle excluded). Reference for 'agrees with radar' = the radar's per-frame presence in the same moments.\n")
    print("| threshold | Wi-Fi present in empty pass (false alarms) | Wi-Fi absent in seated windows (misses) | agrees with radar, all moments |"); print("|---|---|---|---|")
    pts = [t for t, _ in p1]
    for th in (0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 0.97, 0.99, "shipped class"):
        e = [0, 0]; sx = [0, 0]; ag = [0, 0]
        for p, rnd, rows, ev, t0, t1 in pdata:
            rts = [t for t, _ in rows]; t = t0
            while t <= t1:
                r = at(rows, rts, t, 1.5); c = at(p1, pts, t, 4.0)
                if r is not None and c is not None:
                    w = (c >= th) if th != "shipped class" else at(csi, cts, t, 4.0)
                    ag[0] += (bool(w) == bool(r)); ag[1] += 1
                    lab = label_at(p, ev, t)
                    if p == "empty": e[0] += bool(w); e[1] += 1
                    elif p.startswith("sit") and lab == 1: sx[0] += (not w); sx[1] += 1
                t += STEP
        g = lambda x: "-" if not x[1] else "%d%% (%d of %d)" % (round(100 * x[0] / x[1]), x[0], x[1])
        print("| %s | %s | %s | %s |" % (th, g(e), g(sx), g(ag)))
    print("\nTimeline per pass, one character per 2 s bin. R = radar (# present in at least half the bin's frames, . empty), W = Wi-Fi class (# present, . empty, ' ' no result), P = Wi-Fi p1 (9 = at least 0.9, 8 = 0.8 to 0.9, and so on down to 4), C = cues (E enter or go, L leave, empty pass has none).\n```")
    for p, rnd, rows, ev, t0, t1 in pdata:
        nb = int((t1 - t0) // 2) + 1; R = []; W = []; P = []; C = [" "] * nb
        for b in range(nb):
            a0, a1 = t0 + 2 * b, t0 + 2 * b + 2
            rr = [on for t, on in rows if a0 <= t < a1]; R.append("#" if rr and sum(rr) * 2 >= len(rr) else ".")
            cc = [(c, v) for t, c, v in csi_full if a0 <= t < a1]
            W.append(("#" if cc[-1][0] else ".") if cc else " "); P.append(str(min(9, int(cc[-1][1] * 10))) if cc else " ")
        for k, ch in (("cue_enter", "E"), ("cue_go", "E"), ("cue_leave", "L")):
            if k in ev and 0 <= int((ev[k] - t0) // 2) < nb: C[int((ev[k] - t0) // 2)] = ch
        print("%s %s (%d s)\n  C %s\n  R %s\n  W %s\n  P %s" % (p, rnd, 2 * nb, "".join(C), "".join(R), "".join(W), "".join(P)))
    print("```")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--date", default=datetime.date.today().isoformat())
    ap.add_argument("--config", default="2x3")
    ap.add_argument("--radar"); ap.add_argument("--csi"); ap.add_argument("--json")
    a = ap.parse_args()
    rp = a.radar or glob.glob(os.path.join(ROOT, "iwrl6432boost/eval/results/%s-%s.csv" % (a.date, a.config)))
    rp = a.radar or (rp[0] if rp else None)
    cp = a.csi or os.path.join(HERE, "results", a.date + "-freerun.csv")
    if not rp or not os.path.exists(rp): sys.exit("no radar pass csv for %s %s yet" % (a.date, a.config))
    if not os.path.exists(cp): sys.exit("no CSI free-run csv at " + cp)
    csi_full = [(float(r["t"]), r["cls"] == "1", float(r["p1"])) for r in csv.DictReader(open(cp))]
    csi = [(t, c) for t, c, _ in csi_full]
    out, table = [], []
    for (p, rnd), d in sorted(load_radar(rp).items(), key=lambda kv: (min([x[0] for x in kv[1]["rows"]] or [1e18]))):
        if not d["rows"]: continue
        t0, t1 = d["rows"][0][0], d["rows"][-1][0]
        ev = d["ev"]; enter = ev.get("cue_enter", ev.get("cue_go")); leave = ev.get("cue_leave")
        res = {"pass": p, "round": rnd, "window_s": round(t1 - t0, 1)}
        for name, rows in (("radar", d["rows"]), ("csi", csi)):
            f, n = frac(rows, t0, t1)
            res[name] = {"on_fraction": None if f is None else round(f, 3), "samples": n}
            if enter is not None:
                res[name]["detect_s_after_enter"] = lat(first_on(rows, enter, t1), enter)
            if leave is not None:
                res[name]["clear_s_after_leave"] = lat(first_clear([x for x in rows if x[0] <= t1 + HOLD + 2] , leave), leave)
        out.append(res)
        g = lambda n, k: "-" if res[n].get(k) is None else res[n][k]
        table.append("| %s %s | %s | %s (%d) | %s (%d) | %s / %s | %s / %s |" % (p, rnd, res["window_s"], pct(res["radar"]["on_fraction"]), res["radar"]["samples"],
            pct(res["csi"]["on_fraction"]), res["csi"]["samples"], g("radar", "detect_s_after_enter"), g("csi", "detect_s_after_enter"),
            g("radar", "clear_s_after_leave"), g("csi", "clear_s_after_leave")))
    pdata = [(p, rnd, d["rows"], d["ev"], d["rows"][0][0], d["rows"][-1][0]) for (p, rnd), d in load_radar(rp).items() if d["rows"]]
    per, tot = fusion(pdata, csi)
    print("| pass | window s | radar present (frames) | Wi-Fi CSI present (windows) | detect s after cue, radar / CSI | clear s after leave, radar / CSI |")
    print("|---|---|---|---|---|---|"); print("\n".join(table))
    names = {"radar": "radar alone", "wifi": "Wi-Fi alone", "OR": "OR (either says present)", "AND": "AND (both say present)",
             "radar_confirms_wifi": "radar confirms Wi-Fi (Wi-Fi flag and radar present within 5 s)"}
    print("\nFusion, scored against the pass label (empty / person present), %.0f s settle band after each cue not scored." % SETTLE)
    print("Measured only on grid points (every %.1f s) where both logs have a fresh value.\n" % STEP)
    print("| rule | misses (person present, said empty) | false alarms (room empty, said present) | scored present / empty points |")
    print("|---|---|---|---|")
    for k, nm in names.items():
        c = tot[k]; print("| %s | %s (%d of %d) | %s (%d of %d) | %d / %d |" % (nm, fr(c, "miss", "pos"), c["miss"], c["pos"], fr(c, "fa", "neg"), c["fa"], c["neg"], c["pos"], c["neg"]))
    print("\nPer pass (misses / false alarms, percent):\n")
    print("| pass | " + " | ".join(names) + " |"); print("|---|" + "---|" * len(names))
    for p, rnd, c in per:
        print("| %s %s | " % (p, rnd) + " | ".join("%s / %s" % (fr(c[k], "miss", "pos"), fr(c[k], "fa", "neg")) for k in names) + " |")
    extras(pdata, csi_full, rp)
    if a.json: json.dump({"passes": out, "fusion_total": tot, "fusion_per_pass": [(p, r, c) for p, r, c in per]}, open(a.json, "w"), indent=1)

if __name__ == "__main__":
    main()
