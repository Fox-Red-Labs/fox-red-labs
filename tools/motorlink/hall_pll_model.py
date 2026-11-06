#!/usr/bin/env python3
"""Offline model of the Hall tracking observer in the AM13 bench guard, revision H3 (frl_guard.c, hall_pll_*; our code).

The same equations and gains as the firmware, in Python floats (the firmware uses float32), run at the 20 kHz control
rate. It is checked here before anything runs on the board:

  python3 tools/motorlink/hall_pll_model.py            every check, a summary table, writes JSON beside the H3 results
  python3 tools/motorlink/hall_pll_model.py --pole 0.7 the same with another pole (the firmware's is FRL_PLL_POLE)
  python3 tools/motorlink/hall_pll_model.py --h3b      also rewrite model-H3b.json (otherwise kept as committed)

Revision H4 (2026-10-19, before any H4 run) adds h4_section, written to results/guard-revH4-20261019/model-H4.json: the
bench's 24 sector widths from the captures (a mechanical revolution), the SDK's calibration sweep simulated on them
(the tables it should publish and how long it takes), the steady scatter and angle error with the default table, the
calibrated table and a 24-entry table, and the closed loop on the 24 sectors for H2, H3b, H4fw (hallTable 1, hallPll 0)
and H4 (hallTable 1, hallPll 1).

Inputs:
  - the batch 1 and batch 2 captures (am13e230/foc-drv8376/results/batch{1,2}-20261019/*.capture.json): the raw Hall
    pins (HALLRAW) and the firmware's own speed (SPEED) at 1 kHz from the run start. 1 kHz quantises every edge time to
    1 ms, so a replay overstates the observer's scatter; the synthetic trains below time edges at the 50 us control rate
    as the firmware sees them.
  - synthetic edge trains: constant 1, 20, 60, 100 Hz electrical, a speed step, a reversal, a missed edge, a bounced
    edge, sensor spacing errors of +/-5 degrees, and a train with the bench's own sector spacing estimated from batch 1.

The observer, per control ISR (dt = 50 us):
  predict  theta += w * 2 pi dt; the travel since the last edge is bounded at FRL_PLL_STALE x 60 degrees: past it the
           rotor must be slower than w, so |w| = FRL_PLL_STALE x 60 deg / (2 pi x time since the edge) and theta holds
  edge     (debounced, as the firmware reads it) the step from the forward order 3, 2, 6, 4, 5, 1 gives the direction; the
           edge angle is the calibrated table's (forward: fwd[new], backward: rev[new]) plus the mounting offset, moved on
           by w x the debounce delay (2.5 ISRs)
             first edge after standstill: theta = edge, w = 0
             second edge the same way: w = (angle between the two edges) / (time between them), theta = edge
             a reversal: w = 0, theta = edge, and the first edge rule again; if the next edge reverses back within a
               quarter of a sector at the speed held before, it was a bounce and that speed comes back
             tracking: e = wrap(edge - theta); theta += clamp(G1 e, +/-FRL_PLL_CMAX); w += G2 e / (2 pi T_edge)
           G1 = 1 - p^2 and G2 = (1 - p)^2 put both poles of the per-edge error loop at p: the loop settles in a fixed
           number of edges at any speed, so its bandwidth in Hz scales with the edge rate (6 per electrical revolution)
  standstill  no edge for 100 ms: theta = middle of the sector, w = 0
"""
import argparse, bisect, json, math, pathlib, statistics as st, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RES = ROOT / "am13e230/foc-drv8376/results"
OUT = RES / "guard-revH3-20261019" / "model-H3b.json"   # model.json beside it is the H3 run, kept as committed

DT = 50e-6
TWO_PI = 2.0 * math.pi
SECT = math.pi / 3.0
STILL_ISRS = 2000
DEBOUNCE = 3
DELAY_ISRS = 2.5
FWD_ORDER = [3, 2, 6, 4, 5, 1]
IDX = {s: i for i, s in enumerate(FWD_ORDER)}
# the firmware's default tables (hal_hall_sen_interface.h); the bench runs a calibrated table, not recorded in the captures
FWD = {1: 0.66, 2: 2.72, 3: 1.77, 4: -1.44, 5: -0.33, 6: -2.42}
REV = {1: 1.74, 2: -2.46, 3: 2.69, 4: -0.366, 5: 0.610, 6: -1.49}

# the firmware's constants (frl_guard.c); keep these two places in step
POLE = 0.0        # H3b; 0.6 in H3 as first flashed
STALE = 1.2
CMAX = SECT       # H3b; 20 degrees in H3
BOUNCE_FRAC = 0.25
REACQ = None
LEARN = False
LEARN_RATE = 1.0 / 16.0


def wrap(a):
    while a >= math.pi: a -= TWO_PI
    while a < -math.pi: a += TWO_PI
    return a


def nxt(s): return FWD_ORDER[(IDX[s] + 1) % 6]


def sector_mid(s, fwd=FWD, rev=REV, off=0.0):
    return wrap(wrap(fwd[s] + 0.5 * wrap(rev[s] - fwd[s])) + off)


class PLL:
    def __init__(self, pole=POLE, fwd=FWD, rev=REV, off=0.0, cmax=None, reacq="default", learn="default"):
        """pole, cmax and reacq default to the current firmware's (POLE, CMAX, REACQ); H3 as flashed is
        pole 0.6, cmax 20 degrees, reacq None (no re-acquire)"""
        pole = POLE if pole is None else pole
        self.cmax = CMAX if cmax is None else cmax
        self.reacq = REACQ if reacq == "default" else reacq
        self.g1, self.g2 = 1.0 - pole * pole, (1.0 - pole) ** 2
        self.fwd, self.rev, self.off = fwd, rev, off
        self.learn = LEARN if learn == "default" else learn
        # learned sector widths (rad), by forward position; start from the table's own spacing
        self.W = [wrap(fwd[FWD_ORDER[(i + 1) % 6]] - fwd[FWD_ORDER[i]]) % TWO_PI for i in range(6)]
        self.hist = []   # (position of the state entered, sign, ISRs since the previous edge), last 6
        self.B = dict(fwd)
        self.th = 0.0; self.w = 0.0; self.n = 0; self.dir = 1; self.tE = 10 ** 9; self.adv = 0.0
        self.lastEa = 0.0; self.wSave = 0.0; self.dirSave = 1; self.pend = False; self.err = 0.0
        self.still = True

    def isr(self, s, sp, new_edge):
        k = TWO_PI * DT
        self.tE += 1
        d = self.w * k
        self.th += d; self.adv += abs(d)
        lim = STALE * SECT
        if self.adv > lim:   # past the next boundary without an edge: the rotor is slower than w
            back = self.adv - lim
            self.th -= back if self.w > 0 else -back
            self.adv = lim
            self.w = math.copysign(lim / (TWO_PI * DT * self.tE), self.w)
        self.th = wrap(self.th)
        if new_edge and 1 <= s <= 6 and 1 <= sp <= 6:
            step = (IDX[s] - IDX[sp]) % 6
            if step == 0 or step == 3:
                self.n = 0; self.pend = False; self.th = sector_mid(s, self.fwd, self.rev, self.off)
            else:
                sgn = 1 if step in (1, 2) else -1
                if self.learn:
                    self.learn_edge(s, sgn)
                    base = self.B[s] if sgn > 0 else self.B[nxt(s)]
                else:
                    base = self.fwd[s] if sgn > 0 else self.rev[s]
                ea = base + self.off + self.w * k * DELAY_ISRS
                ea = wrap(ea)
                e = wrap(ea - self.th)
                if self.n == 0:
                    self.th = ea; self.w = 0.0; self.n = 1; self.dir = sgn; self.pend = False
                elif sgn != self.dir:
                    if self.pend and sgn == self.dirSave and self.tE * abs(self.wSave) * k < BOUNCE_FRAC * SECT:
                        self.w = self.wSave; self.th = ea; self.n = 2; self.dir = sgn; self.pend = False
                    else:
                        self.wSave = self.w; self.dirSave = self.dir; self.pend = True
                        self.th = ea; self.w = 0.0; self.n = 1; self.dir = sgn
                else:
                    self.pend = False
                    if self.n == 1 or (self.reacq is not None and abs(e) > self.reacq):
                        if self.n >= 2: self.err = e
                        span = min(max(wrap(ea - self.lastEa) * sgn, SECT * 0.5), SECT * 1.5)
                        self.w = sgn * span / (TWO_PI * DT * self.tE); self.th = ea; self.n = 2
                    else:
                        ec = max(-SECT, min(SECT, e))
                        c = max(-self.cmax, min(self.cmax, self.g1 * ec))
                        self.th = wrap(self.th + c)
                        self.w += self.g2 * ec / (TWO_PI * DT * self.tE)
                        self.err = e
                self.lastEa = ea
            self.tE = 0; self.adv = 0.0
        if self.tE >= STILL_ISRS and 1 <= s <= 6:
            self.th = sector_mid(s, self.fwd, self.rev, self.off); self.w = 0.0; self.n = 0; self.pend = False
        return self.th, self.w


    def learn_edge(self, s, sgn):
        """sector widths from the last revolution's edge times: with six edges the same way inside 0.3 s and no interval
        over three times another, the sector just left took T / R of the revolution; each width moves 1/16 of the way
        there. The boundaries are the widths summed round from the table, shifted so their mean matches the table's."""
        self.hist.append((IDX[s], sgn, self.tE))
        if len(self.hist) > 6: self.hist.pop(0)
        if len(self.hist) == 6 and all(h[1] == sgn for h in self.hist):
            ts = [h[2] for h in self.hist]
            R = sum(ts)
            if R * DT < 0.3 and max(ts) < 3 * min(ts):
                left = (IDX[s] - 1) % 6 if sgn > 0 else (IDX[s] + 1) % 6   # forward position of the sector just crossed
                self.W[left] += LEARN_RATE * (TWO_PI * ts[-1] / R - self.W[left])
                tot = sum(self.W); self.W = [x * TWO_PI / tot for x in self.W]
                b, a = {}, 0.0
                for i, st_ in enumerate(FWD_ORDER):
                    b[st_] = a; a += self.W[i]
                c = st.fmean(wrap(b[x] - self.fwd[x]) for x in FWD_ORDER)
                self.B = {x: wrap(v - c) for x, v in b.items()}


class Debounce:
    """the firmware's 3-read debounce (hal_hall_sen_interface.h, FRL_HALL_DEBOUNCE_TICKS)"""
    def __init__(self):
        self.state = 0; self.cand = 0; self.cnt = 0

    def read(self, raw):
        if raw == self.cand:
            self.cnt = min(self.cnt + 1, 255)
        else:
            self.cand = raw; self.cnt = 1
        st_ = raw
        if self.state != 0 and self.cnt < DEBOUNCE:
            st_ = self.state
        prev = self.state; self.state = st_
        return st_, prev, st_ != prev


class FirmwareSpeed:
    """the firmware's Hall speed: on each debounced edge 1 / (6 x the raw interval) with the commanded direction's sign
    (filter gain 1 at 20 kHz, the port's cap), scaled down as 1 / (time since the edge) once an edge is overdue"""
    def __init__(self, direction=1.0):
        self.d = direction; self.v = 0.0; self.no = 0; self.lastRaw = None; self.rawT = None

    def isr(self, new_edge, raw_period_s):
        if new_edge:
            self.no = 0
            if raw_period_s and raw_period_s > 1.0 / (6 * 320.0):
                self.v = self.d / (6.0 * raw_period_s)
            return self.v
        self.no += 1
        a = abs(self.v)
        if a > 0:
            exp = 1.0 / (a * 6.0 * DT)
            if self.no >= exp:
                self.v = self.d * (exp / self.no) * a
        return self.v


def run_raw(raw_states, pole=POLE, true_raw_edges=None, fwd=FWD, rev=REV, direction=1.0):
    """raw Hall state per ISR -> observer theta, w and the firmware speed per ISR"""
    db, pll, fw = Debounce(), PLL(pole, fwd, rev), FirmwareSpeed(direction)
    lastRawEdge = None; prevRaw = None; rawPer = None
    out = []
    for i, r in enumerate(raw_states):
        if prevRaw is not None and r != prevRaw:
            t = true_raw_edges[i] if true_raw_edges is not None else i * DT
            if lastRawEdge is not None: rawPer = t - lastRawEdge
            lastRawEdge = t
        prevRaw = r
        s, sp, ne = db.read(r)
        th, w = pll.isr(s, sp, ne)
        v = fw.isr(ne, rawPer if ne else None)
        out.append((th, w, v, s, ne, pll.err))
    return out


# ---- synthetic trains ---------------------------------------------------------------------------------------------
def boundaries(spacing_err_deg=None):
    """true crossing angles: forward into s at fwd[s], backward into s at rev[s], each moved by a spacing error"""
    f, r = dict(FWD), dict(REV)
    if spacing_err_deg:
        for i, s in enumerate(FWD_ORDER):
            e = math.radians(spacing_err_deg[i])
            f[s] = wrap(f[s] + e)                 # the edge entering s forward ...
            p = FWD_ORDER[(i - 1) % 6]            # ... is the same boundary as entering p backward
            r[p] = wrap(r[p] + e)
    return f, r


def synth(speed_fn, secs, spacing_err_deg=None, theta0=None, events=None):
    """integrate a true angle and emit the raw Hall state per ISR. events: (t_s, kind) applied at the first forward edge
    after t_s: 'miss' (the new state is never seen: the pins hold the old state until the next boundary, so the debounce
    sees a two-state jump) or 'bounce' (the new state for 0.3 ms, the old one back for 0.4 ms, then the new one)"""
    f, r = boundaries(spacing_err_deg)
    n = int(secs / DT)
    th = sector_mid(3) if theta0 is None else theta0
    s = 3
    states, truth, speed = [], [], []
    ov_from = ov_to = -1; ov_state = None; miss = None; ev = sorted(events or [])
    for i in range(n):
        t = i * DT
        w = speed_fn(t)
        th = wrap(th + TWO_PI * w * DT)
        # forward boundary out of s is f[next(s)]; backward boundary out of s is r[prev(s)]
        nx = nxt(s); pv = FWD_ORDER[(IDX[s] - 1) % 6]
        if w > 0 and 0 <= wrap(th - f[nx]) < 0.5:
            old = s; s = nx; miss = None
            if ev and ev[0][0] <= t:
                kind = ev.pop(0)[1]
                if kind == "bounce": ov_from, ov_to, ov_state = i + 6, i + 14, old
                if kind == "miss": miss = old
        elif w < 0 and -0.5 < wrap(th - r[pv]) <= 0:
            s = pv; miss = None
        raw = miss if miss is not None else (ov_state if ov_from <= i < ov_to else s)
        states.append(raw); truth.append(th); speed.append(w)
    return states, truth, speed


def steady_stats(vals):
    m = st.fmean(vals)
    return {"mean": m, "sd": st.pstdev(vals), "sd_pct": 100 * st.pstdev(vals) / abs(m) if abs(m) > 1e-9 else float("nan")}


def synthetic_checks(pole, bench_spacing=None):
    res = {}
    for hz in (1.0, 2.0, 3.0, 20.0, 60.0, 100.0):
        secs = max(2.0, 30.0 / hz)
        cases = [("exact", None, False), ("spacing+-5", [5, -5, 3, -3, 4, -4], False)]
        if bench_spacing: cases += [("bench_uncal", bench_spacing, False), ("bench_cal", bench_spacing, True)]
        for label, sp, cal in cases:
            raw, tru, _ = synth(lambda t, hz=hz: hz, secs, sp)
            f, r = boundaries(sp) if cal else (FWD, REV)   # cal: the table holds the true edges, as a calibration measures
            o = run_raw(raw, pole, fwd=f, rev=r)
            i0 = int(0.6 * len(o))
            w = [x[1] for x in o[i0:]]; v = [x[2] for x in o[i0:]]
            aerr = [math.degrees(wrap(o[i][0] - tru[i])) for i in range(i0, len(o))]
            res[f"const_{hz:g}Hz_{label}"] = {"pll": steady_stats(w), "firmware": steady_stats(v),
                                              "angle_err_deg": {"mean": st.fmean(aerr), "sd": st.pstdev(aerr),
                                                                "maxabs": max(abs(a) for a in aerr)}}
    # a speed step 20 -> 30 Hz at 0.5 s (instant): edges and ms to within 5 percent of 30 Hz
    raw, tru, spd = synth(lambda t: 20.0 if t < 0.5 else 30.0, 1.0)
    o = run_raw(raw, pole)
    i_step = int(0.5 / DT)
    settle = None; edges = 0
    for i in range(i_step, len(o)):
        if o[i][4]: edges += 1
        if abs(o[i][1] - 30.0) < 1.5 and all(abs(o[j][1] - 30.0) < 1.5 for j in range(i, min(len(o), i + int(0.05 / DT)))):
            settle = (i - i_step) * DT; break
    res["step_20_30"] = {"settle_ms_5pct": None if settle is None else round(1000 * settle, 1), "edges_to_settle": edges}
    # a reversal: +20 Hz, decelerate at 2000 Hz/s through zero to -20 Hz
    def rev_fn(t):
        if t < 0.3: return 20.0
        return max(-20.0, 20.0 - 2000.0 * (t - 0.3))
    raw, tru, spd = synth(rev_fn, 0.6)
    o = run_raw(raw, pole)
    i_zero = int((0.3 + 0.01) / DT)
    # edges after the true speed changes sign until the observer's sign is right and stays right
    first_neg_edge = None; wrong_after = 0; ecount = 0; sign_ok_edge = None
    for i in range(i_zero, len(o)):
        if o[i][4]:
            ecount += 1
            if sign_ok_edge is None and o[i][1] < 0: sign_ok_edge = ecount
        if spd[i] < -1.0 and o[i][1] > 0.5: wrong_after += 1
    res["reversal_20_-20"] = {"edges_until_negative_speed": sign_ok_edge, "isrs_with_positive_w_while_turning_backward": wrong_after,
                              "firmware_sign_at_end": math.copysign(1, o[-1][2])}
    # missed and bounced edges at 20 Hz
    for kind in ("miss", "bounce"):
        raw, tru, _ = synth(lambda t: 20.0, 1.0, None, events=[(0.5, kind)])
        o = run_raw(raw, pole)
        i0 = int(0.48 / DT); i1 = int(0.7 / DT)
        aerr = [abs(math.degrees(wrap(o[i][0] - tru[i]))) for i in range(i0, i1)]
        w = [o[i][1] for i in range(i0, i1)]
        res[f"{kind}_edge_20Hz"] = {"max_angle_err_deg": max(aerr), "w_min": min(w), "w_max": max(w)}
    return res


# ---- replay of the bench captures -------------------------------------------------------------------------------------
def load_capture(p):
    d = json.loads(p.read_text())
    cols = d["columns"]; rows = d["rows"]
    ci = {c: cols.index(c) for c in cols}
    return [r[ci["t_s"]] for r in rows], [r[ci["SPEED"]] for r in rows], [int(round(r[ci["HALLRAW"]])) for r in rows]


def hall_rev_speed(t, hall):
    """speed from six raw edges (one electrical revolution), so sensor spacing cancels; at each edge"""
    et = [t[i] for i in range(1, len(hall)) if hall[i] != hall[i - 1]]
    return [(et[i], 1.0 / (et[i] - et[i - 6])) for i in range(6, len(et))]


def replay(p, pole):
    t, spd, hall = load_capture(p)
    per = int(round(0.001 / DT))
    raw = []
    for h in hall: raw += [h] * per
    o = run_raw(raw, pole)
    w_ms = [o[i * per + per - 1][1] for i in range(len(t))]
    # the sign after the start: edges in time order and the observer's speed after each
    edges = [(i, hall[i - 1], hall[i]) for i in range(1, len(hall)) if hall[i] != hall[i - 1]]
    seq = []
    for (i, a, b) in edges[:12]:
        step = (IDX.get(b, 0) - IDX.get(a, 0)) % 6
        seq.append({"t_ms": i, "from": a, "to": b, "dir": "fwd" if step in (1, 2) else "back",
                    "pll_Hz": round(w_ms[min(i + 1, len(w_ms) - 1)], 1), "fw_Hz": round(spd[min(i + 1, len(spd) - 1)], 1)})
    return t, spd, hall, w_ms, seq


def bench_spacing_estimate(paths):
    """mean share of a revolution per sector at steady 20 Hz from batch 1's no-step runs, as a spacing error in degrees"""
    acc = {s: [] for s in FWD_ORDER}
    for p in paths:
        t, spd, hall = load_capture(p)
        et = [(t[i], hall[i]) for i in range(1, len(hall)) if hall[i] != hall[i - 1] and t[i] > 0.6]
        for k in range(6, len(et)):
            rev = et[k][0] - et[k - 6][0]
            if rev <= 0: continue
            # sector that ended at edge k: state et[k-1][1], its duration
            acc[et[k - 1][1]].append((et[k][0] - et[k - 1][0]) / rev * 360.0)
    widths = {s: st.fmean(v) for s, v in acc.items() if v}
    # the boundary error entering each state: cumulative width error, zero mean
    cum, errs = 0.0, []
    for s in FWD_ORDER:
        errs.append(cum); cum += widths.get(s, 60.0) - 60.0
    m = st.fmean(errs)
    return [e - m for e in errs], widths


def windows(tag, step):
    return [(0.6, 1.15), (1.5, 2.0)] if step else [(0.6, 2.0)]


def replay_all(pole):
    runs = []
    for b in ("batch1-20261019", "batch2-20261019"):
        meta = {r["tag"]: r for r in json.loads((RES / b / "runs.json").read_text())}
        for p in sorted((RES / b).glob("*.capture.json")):
            tag = p.name.split(".capture")[0]
            r = meta.get(tag, {})
            t, spd, hall, w, seq = replay(p, pole)
            moved = any(hall[i] != hall[0] for i in range(len(hall)))
            rec = {"batch": b[:6], "tag": tag, "kp": r.get("kp"), "reason": (r.get("post") or {}).get("reason"),
                   "fault": (r.get("post") or {}).get("faultMtrNow"), "moved": moved, "edges": seq}
            if moved and rec["reason"] == 1:
                hr = hall_rev_speed(t, hall)
                win = []
                for a, z in windows(tag, r.get("step")):
                    idx = [i for i in range(len(t)) if a <= t[i] < z]
                    if not idx: continue
                    fw = [spd[i] for i in idx]; pl = [w[i] for i in idx]
                    hv = [v for (tt, v) in hr if a <= tt < z]
                    win.append({"from_s": a, "to_s": z, "firmware": steady_stats(fw), "pll_1ms_edges": steady_stats(pl),
                                "hall_rev": steady_stats(hv) if len(hv) > 2 else None})
                rec["steady"] = win
            # sign check: after the first backward edge, how many edges until the observer's sign follows the edge order
            back_wrong = 0
            for k, e in enumerate(seq):
                if k >= 2 and e["dir"] == "back" and seq[k - 1]["dir"] == "back" and e["pll_Hz"] > 0: back_wrong += 1
            rec["pll_positive_during_backward_runs"] = back_wrong
            fw_wrong = sum(1 for k, e in enumerate(seq) if k >= 1 and e["dir"] == "back" and seq[k - 1]["dir"] == "back" and e["fw_Hz"] > 0)
            rec["firmware_positive_during_backward_runs"] = fw_wrong
            runs.append(rec)
    return runs


# ---- closed loop (added for revision H3b, after the H3 bench runs) --------------------------------------------------
# A rough plant, not a fitted one: torque per amp A (Hz/s per A of Iq, electrical), viscous drag B (1/s) and Coulomb
# friction, torque scaled by cos(FOC angle - rotor angle - the 60 degree Hall offset). With A 2500 and B 7.5 the motor
# needs 0.06 A at 20 Hz (batch 1) and accelerates at 1000 Hz/s on 0.4 A (d1 saw about 800). The real rotor's edges sit
# at the bench widths measured in batch 1, not at the default table the firmware uses (the calibrated table is never
# wired in on this build, so the firmware reads the defaults). The firmware's Hall start, hand-over, speed PI (every
# 10 ISRs, parallel form, clamp +/-0.4 A) and speed ramp follow application.h, foc.h and traj.h.
BENCH_W = {3: 48.5, 2: 67.0, 6: 64.4, 4: 48.5, 5: 68.6, 1: 62.2}
OFFSET_TRUE = math.radians(60.0)


def bench_boundaries():
    b, a = {}, FWD[3]
    for s in FWD_ORDER:
        b[s] = a; a += math.radians(BENCH_W[s])
    c = st.fmean(wrap(b[s] - FWD[s]) for s in FWD_ORDER)
    b = {s: wrap(v - c) for s, v in b.items()}
    rev = {s: b[nxt(s)] for s in FWD_ORDER}
    return b, rev


def state_at(th, bf):
    """the Hall state whose forward sector holds angle th"""
    for s in FWD_ORDER:
        if 0 <= wrap(th - bf[s]) < wrap(bf[nxt(s)] - bf[s]) % TWO_PI: return s
    return FWD_ORDER[0]


def warm_widths(hz=20.0, secs=4.0, pll_kw=None):
    """the bench rotor turning steadily at hz through the debounce and the observer: the widths it has learned"""
    bf, br = bench_boundaries()
    db, pll = Debounce(), PLL(**dict(pll_kw or {}, learn=True))
    th = wrap(bf[3] + 0.3); s_true = 3; db.state = db.cand = 3; db.cnt = DEBOUNCE
    for i in range(int(secs / DT)):
        th = wrap(th + hz * TWO_PI * DT)
        nx = nxt(s_true)
        if 0 <= wrap(th - bf[nx]) < 0.5: s_true = nx
        sd, sp, ne = db.read(s_true)
        pll.isr(sd, sp, ne)
    return list(pll.W), dict(pll.B)


def closed_loop(mode, th0, kp=0.05, ki=0.005, A=2500.0, B=7.5, accel=100.0, ref=20.0, secs=0.6, pll_kw=None, warm=None):
    """mode 'H2': the firmware's Hall angle and speed drive; 'H3': the observer drives the Hall start and control run
    (as flashed); 'H3b': the firmware's Hall start, the observer in control run, the reference seeded at the observer's
    first speed. Returns a dict of what a capture would show."""
    bf, br = bench_boundaries()
    db, pll = Debounce(), PLL(**(pll_kw or {}))
    if warm: pll.W, pll.B = list(warm[0]), dict(warm[1])
    th, w = th0, 0.0
    s_true = state_at(th, bf)
    db.state = db.cand = s_true; db.cnt = DEBOUNCE
    hdA = sector_mid(s_true); hdW = 0.0; noE = 10 ** 6; lastRaw = -2.0; prevRaw = s_true
    state = "hall"; Ui = 0.0; traj = 0.0; iq = 0.4; cnt = 0; seeded = False
    adj = math.radians(60.0); cf = A * 0.01
    edges = []; log = []; dmax = accel * DT
    for i in range(int(secs / DT)):
        t = i * DT
        # plant
        foc = None
        # raw Hall from the true angle
        nx = nxt(s_true); pv = FWD_ORDER[(IDX[s_true] - 1) % 6]
        if 0 <= wrap(th - bf[nx]) < 0.5: s_true = nx
        elif -0.5 < wrap(th - br[pv]) < 0: s_true = pv
        raw = s_true
        rawPer = None
        if raw != prevRaw:
            rawPer = t - lastRaw; lastRaw = t; prevRaw = raw
        sd, sp, ne = db.read(raw)
        if ne and rawPer is None: rawPer = None
        # the firmware's Hall read (direction +1, commanded)
        if ne:
            noE = 0
            per = t - getattr(closed_loop, "_le", -2.0) if False else None
        if ne:
            hdW = 1.0 / (6.0 * max(1e-6, closed_loop_last(edges, t)))
            hdA = FWD[sd]
            edges.append((t, sp, sd))
        else:
            noE += 1
            a_ = abs(hdW)
            if a_ > 0:
                exp = 1.0 / (a_ * 6.0 * DT)
                if noE >= exp: hdW = (exp / noE) * a_
            hdA = wrap(hdA + hdW * TWO_PI * DT)
        if noE >= STILL_ISRS: hdA = sector_mid(sd)          # hall_standstill_angle (H2)
        pth, pw = pll.isr(sd, sp, ne)
        if mode == "H3" and state == "hall": hdA, hdW = pth, pw
        # state machine
        if state == "hall":
            iq = 0.4; foc = hdA
            if abs(hdW) > 0:
                state = "run"; Ui = 0.4; traj = hdW
        else:
            useP = mode in ("H3", "H3b")
            est, fb = (pth, pw) if useP else (hdA, hdW)
            if mode == "H3b" and not seeded and t < 1.0 and pll.n >= 2 and pw * ref > 0:
                traj = pw if abs(pw) < abs(ref) else ref; seeded = True
            traj += max(-dmax, min(dmax, ref - traj))
            cnt += 1
            if cnt >= 10:
                cnt = 0
                e = traj - fb
                Ui = max(-0.4, min(0.4, Ui + ki * e))
                iq = max(-0.4, min(0.4, kp * e + Ui))
            foc = est + adj
            log.append((t, th, w, fb, iq))
        tq = A * iq * math.cos(foc - th - OFFSET_TRUE) - B * w
        if abs(w) < 0.05 and abs(tq) < cf: w = 0.0
        else: w += (tq - math.copysign(cf, w if w != 0 else tq)) * DT
        th = wrap(th + w * TWO_PI * DT)
        if w < -30.0: return {"mode": mode, "result": "runaway", "t_s": round(t, 3), "edges": edges[:8]}
    return summarise(mode, edges, log, ref, secs)


def closed_loop_last(edges, t):
    return (t - edges[-1][0]) if edges else 2.0


def summarise(mode, edges, log, ref, secs):
    dirs = ["f" if (IDX[b] - IDX[a]) % 6 in (1, 2) else "b" for _, a, b in edges[:8]]
    et = [e[0] for e in edges]
    rev = [(et[i], 1.0 / (et[i] - et[i - 6])) for i in range(6, len(et))]
    t20 = next((t for t, v in rev if v >= ref), None)
    tail = [x for x in log if x[0] >= secs - 0.3]
    tw = [x[2] for x in tail]; fb = [x[3] for x in tail]; iq = [x[4] for x in tail]
    stall = (not et) or max([b - a for a, b in zip(et, et[1:])] + [secs - et[-1]]) > 0.25
    return {"mode": mode, "result": "stall" if stall else "ran", "first_edges": "".join(dirs),
            "t20_s": None if t20 is None else round(t20, 3),
            "true_speed": (round(st.fmean(tw), 2), round(st.pstdev(tw), 2)) if tw else None,
            "fb_sd": round(st.pstdev(fb), 2) if fb else None,
            "iq_rms": round(math.sqrt(st.fmean([q * q for q in iq])), 3) if iq else None}


H3_KW = {"pole": 0.6, "cmax": math.radians(20.0), "reacq": None}   # revision H3 as first flashed


def closed_loop_grid(modes=("H2", "H3", "H3b"), kps=(0.05, 0.1), As=(300.0, 1000.0, 2500.0)):
    """12 starts (two places in each sector) per mode, gain and plant; medians"""
    bf, _ = bench_boundaries()
    starts = [wrap(bf[s] + math.radians(f * BENCH_W[s])) for s in FWD_ORDER for f in (0.25, 0.75)]
    md = lambda xs: sorted(xs)[len(xs) // 2] if xs else None
    out = {}
    for m in modes:
        for kp in kps:
            for A in As:
                rs = [closed_loop(m, th0, kp=kp, A=A, B=A * 0.06 / 20.0, pll_kw=H3_KW if m == "H3" else None) for th0 in starts]
                ok = [r for r in rs if r["result"] == "ran"]
                out[f"{m}_kp{kp}_A{int(A)}"] = {
                    "ran": len(ok), "runaway": sum(r["result"] == "runaway" for r in rs), "stall": sum(r["result"] == "stall" for r in rs),
                    "backstep_in_first_6": sum("b" in r["first_edges"][:6] for r in ok),
                    "true_speed_sd_Hz": md([r["true_speed"][1] for r in ok]), "fb_speed_sd_Hz": md([r["fb_sd"] for r in ok]),
                    "iq_rms_A": md([r["iq_rms"] for r in ok]), "t20_s": md([r["t20_s"] for r in ok if r["t20_s"]])}
    return out


def replay_d1(path=RES / "guard-revH3-20261019/diag/d1.capture.json"):
    """the H3 observer (pole 0.6, 20 degree clamp) on d1's raw Hall pins at 4 kHz, against the board's own PLLSPEED and
    PLLANGLE in the same capture"""
    if not path.exists(): return None
    d = json.loads(path.read_text()); rows = d["rows"]; c = d["columns"]
    iw, ia, ih = c.index("PLLSPEED"), c.index("PLLANGLE"), c.index("HALLRAW")
    per = int(round((rows[1][1] - rows[0][1]) / DT))
    db, pll = Debounce(), PLL(**H3_KW)
    db.state = db.cand = int(round(rows[0][ih])); db.cnt = DEBOUNCE
    out = []
    for r in rows:
        for _ in range(per):
            sd, sp, ne = db.read(int(round(r[ih]))); th, w = pll.isr(sd, sp, ne)
        out.append((th, w))
    dw = [abs(o[1] - r[iw]) for o, r in zip(out, rows)]
    da = [abs(math.degrees(wrap(o[0] - r[ia]))) for o, r in zip(out, rows)]
    n = len(rows)
    return {"samples": n, "speed_within_0.2Hz_or_1pct": sum(1 for x, r in zip(dw, rows) if x <= max(0.2, 0.01 * abs(r[iw]))) / n,
            "angle_within_1deg": sum(1 for x in da if x <= 1.0) / n, "speed_diff_median_Hz": st.median(dw),
            "angle_diff_median_deg": st.median(da),
            "note": "the rest sit on the sample of a debounced edge or a standstill switch, one capture sample apart"}


# ---- revision H4: the calibrated table, and the bench's 24 sectors (added 2026-10-19, before any H4 run) ----------------
# The batch 1 and 2 captures at steady 20 Hz, folded by mechanical revolution (24 sectors on the 4-pole-pair BLY171S),
# repeat the same 24 widths in every run (rms 1 to 2 degrees between runs, after the right one of four shifts): the same
# Hall state is 36 to 58 degrees wide (state 3) or 51 to 79 (state 5) depending on the pole pair. A 6-entry table, the
# default or a calibrated one, can only hold each state's mean, so the per-edge speed keeps the pole-pair part of the
# scatter. Assumption: the true electrical angle is 4 x the mechanical angle and the Halls carry the irregularity (magnet
# edges or sensor placement); a speed ripple locked to the mechanical angle would look the same in the captures.
OUT_H4 = RES / "guard-revH4-20261019" / "model-H4.json"
DITHER_RET = math.radians(1.0)      # the calibration samples the Hall state with the rotor held at sweep - 1 degree
CAL_STEP = math.pi / 90.0           # HallCalStepSize_rad, 2 degrees
CAL_JUMP = math.pi / 4.0            # HALL_CAL_JUMP_ANGLE_RAD, 45 degrees
CAL_STEP_S = 0.4                    # dither forward 200 ms + dither back 200 ms per step (mtr1_init.c)
CAL_ALIGN_S = 0.4
NPP = 4


def bench_widths24(paths=None):
    """the 24 sector widths (degrees, a mechanical revolution, position 0 a state-3 sector), averaged over the runs after
    aligning each run's pattern to the first by the best of four pole-pair shifts; and the rms of each run against it"""
    if paths is None:
        paths = sorted((RES / "batch1-20261019").glob("S0_*.capture.json")) + sorted((RES / "batch2-20261019").glob("H2_*.capture.json"))
    pats = []
    for pth in paths:
        t, spd, hall = load_capture(pth)
        et = [(t[i], hall[i]) for i in range(1, len(hall)) if hall[i] != hall[i - 1] and t[i] > 0.6]
        if len(et) < 60: continue
        ws = []
        for k in range(3, len(et) - 4):
            rev = et[k + 3][0] - et[k - 3][0]
            ws.append((et[k][1], (et[k + 1][0] - et[k][0]) / rev * 360.0))
        i0 = next(i for i, (s_, w) in enumerate(ws) if s_ == 3)
        ws = ws[i0:]
        if any(s_ != FWD_ORDER[i % 6] for i, (s_, w) in enumerate(ws)): continue
        pos = [[] for _ in range(24)]
        for i, (s_, w) in enumerate(ws): pos[i % 24].append(w)
        pats.append([st.fmean(x) for x in pos])
    ref = pats[0]; al = [ref]; rms = []
    for q in pats[1:]:
        b = min(range(4), key=lambda r: sum((q[(i + 6 * r) % 24] - ref[i]) ** 2 for i in range(24)))
        q2 = [q[(i + 6 * b) % 24] for i in range(24)]
        al.append(q2); rms.append(math.sqrt(sum((q2[i] - ref[i]) ** 2 for i in range(24)) / 24))
    m = [st.fmean(a[i] for a in al) for i in range(24)]
    tot = sum(m)
    return [x * 1440.0 / tot for x in m], {"runs": len(pats), "rms_vs_first_deg": [round(x, 2) for x in rms]}


class Bench24:
    """the rotor through its 24 Hall sectors. phi: electrical angle summed over the mechanical revolution (0 to 8 pi).
    Boundaries in the Hall-table frame (as bench_boundaries); the FOC frame is that plus OFFSET_TRUE"""
    def __init__(self, w24):
        b6, _ = bench_boundaries()
        cum = [0.0]
        for w in w24[:-1]: cum.append(cum[-1] + math.radians(w))
        # per-state mean of (cum - k 2 pi) must equal b6: shift the whole pattern by one constant
        c = st.fmean(wrap(cum[p] - 2 * math.pi * (p // 6) - b6[FWD_ORDER[p % 6]]) for p in range(24))
        self.B = [cum[p] - c for p in range(24)]     # forward boundary into position p, phi units, from about b6[3]
        self.L = 8.0 * math.pi
        dev = {}
        for p in range(24):
            s_ = FWD_ORDER[p % 6]
            dev.setdefault(s_, []).append(math.degrees(wrap(self.B[p] - 2 * math.pi * (p // 6) - b6[s_])))
        self.dev = dev                               # each pole pair's boundary minus the state's mean, degrees

    def pos(self, phi):
        x = (phi - self.B[0]) % self.L + self.B[0]
        return max(0, bisect.bisect_right(self.B, x) - 1)

    def state(self, phi): return FWD_ORDER[self.pos(phi) % 6]

    def edge_in(self, p, sgn):
        """the true boundary (Hall frame, wrapped) crossed entering position p forward (its lower edge) or backward (upper)"""
        return wrap(self.B[p]) if sgn > 0 else wrap(self.B[(p + 1) % 24] + (self.L if p == 23 else 0.0))


def simulate_cal(bench, start_phi=0.0):
    """the SDK's sweep (hall_cal.h) on the 24-sector rotor, quasi-static (no friction, no hysteresis): the d-axis held at
    the sweep angle puts the rotor at FOC angle = sweep, Hall angle = sweep - OFFSET_TRUE, sampled at sweep - 1 degree.
    Forward from 0 until 24 state entries, then back from 0 until 24. Entries averaged circularly (revision H4). Returns
    the tables (FOC frame, rad), each entry's spread, and the time."""
    out = {}
    tsec = 2 * CAL_ALIGN_S
    for sgn, key in ((1, "fwd"), (-1, "rev")):
        # align at 0: the rotor's phi at FOC 0 nearest start_phi
        k0 = round((start_phi - (-OFFSET_TRUE)) / TWO_PI)
        base = -OFFSET_TRUE + k0 * TWO_PI
        sweep = 0.0
        prev = bench.state(base + sweep - DITHER_RET)
        acc = {s_: [] for s_ in FWD_ORDER}; n = 0
        while n < 6 * NPP:
            st_ = bench.state(base + sweep - DITHER_RET)
            tsec += CAL_STEP_S
            if st_ != prev:
                acc[st_].append(wrap(sweep)); n += 1
                if n >= 6 * NPP: break
                sweep += sgn * CAL_JUMP
            else:
                sweep += sgn * CAL_STEP
            prev = st_
        tab, spr = {}, {}
        for s_, v in acc.items():
            r0 = v[0]; d = [wrap(x - r0) for x in v]
            tab[s_] = wrap(r0 + st.fmean(d)); spr[s_] = math.degrees(max(d) - min(d))
        out[key] = tab; out[key + "_spread_deg"] = spr; out[key + "_counts"] = {s_: len(v) for s_, v in acc.items()}
        start_phi = base + sweep
    out["seconds"] = round(tsec, 1)
    return out


def table_check(fwd, rev):
    """frl_guard.c hall_table_check: steps 30 to 90 forward in both tables, reverse edge into a state within 15 degrees
    of the forward edge into the next"""
    steps_f = [math.degrees(wrap(fwd[nxt(s_)] - fwd[s_])) for s_ in FWD_ORDER]
    steps_r = [math.degrees(wrap(rev[nxt(s_)] - rev[s_])) for s_ in FWD_ORDER]
    pairs = [math.degrees(wrap(rev[s_] - fwd[nxt(s_)])) for s_ in FWD_ORDER]
    ok = all(30 <= x <= 90 for x in steps_f + steps_r) and all(abs(x) < 15 for x in pairs)
    return ok, steps_f, steps_r, pairs


def steady_scatter24(bench, hz, fwd, rev, frame, secs=3.0, oracle=False):
    """the rotor at a constant hz through the 24 sectors: the observer's speed (pole 0, H3b/H4) with a given table, the
    firmware's per-edge speed and its Hall angle (HAL_Hall_computeAngle_inline: the table at an edge, the speed integrated
    between), sampled at 1 kHz as the capture channels are. frame: the table's frame minus the Hall frame (0 for the default
    table, which needs angleAdj; OFFSET_TRUE for a calibrated one); the angle errors are against the true FOC angle, so for
    the default table they read about -60 (what angleAdj_deg adds back). oracle: the observer reads each pole pair's own
    boundary (a 24-entry table with the mechanical position known), the lower bound"""
    db, pll, fw = Debounce(), PLL(fwd=dict(fwd), rev=dict(rev), off=0.0), FirmwareSpeed(1.0)
    phi = bench.B[0] + 0.01; s0 = bench.state(phi); db.state = db.cand = s0; db.cnt = DEBOUNCE
    lastRaw = None; prevRaw = s0; per = None; hdA = 0.0
    ws, vs, ae, fe = [], [], [], []
    for i in range(int(secs / DT)):
        t = i * DT
        phi += hz * TWO_PI * DT
        p = bench.pos(phi); raw = FWD_ORDER[p % 6]
        if raw != prevRaw:
            if lastRaw is not None: per = t - lastRaw
            lastRaw = t; prevRaw = raw
        if oracle:
            pll.fwd[raw] = wrap(bench.edge_in(p, 1) + frame); pll.rev[raw] = wrap(bench.edge_in(p, -1) + frame)
        sd, sp, ne = db.read(raw)
        th, w = pll.isr(sd, sp, ne)
        v = fw.isr(ne, per if ne else None)
        hdA = fwd[sd] if ne else wrap(hdA + v * TWO_PI * DT)
        if t > 1.0 and i % 20 == 0:
            true = wrap(phi) + OFFSET_TRUE
            ws.append(w); vs.append(v)
            ae.append(math.degrees(wrap(th - true))); fe.append(math.degrees(wrap(hdA - true)))
    return {"observer_sd_Hz": round(st.pstdev(ws), 3), "observer_mean_Hz": round(st.fmean(ws), 3),
            "firmware_sd_Hz": round(st.pstdev(vs), 3), "firmware_mean_Hz": round(st.fmean(vs), 3),
            "observer_angle_err_deg": [round(st.fmean(ae), 2), round(st.pstdev(ae), 2)],
            "firmware_angle_err_deg": [round(st.fmean(fe), 2), round(st.pstdev(fe), 2)]}


def closed_loop24(mode, phi0, bench, cal, kp=0.05, ki=0.005, A=2500.0, B=7.5, accel=100.0, ref=20.0, secs=0.6, adj_deg=None):
    """closed_loop on the 24-sector rotor. Modes: 'H2' firmware angle and speed, default table, adj 60; 'H3b' firmware
    Hall start, the observer (pole 0) in control run, default table, adj 60, reference seeded; 'H4fw' as H2 on the
    calibrated table, adj 0 (hallTable 1, hallPll 0); 'H4' as H3b on the calibrated table, adj 0 (hallTable 1, hallPll 1).
    The Hall start runs on the table angle with no adj in every mode, as the firmware does."""
    useCal = mode in ("H4", "H4fw")
    tf, tr = (cal["fwd"], cal["rev"]) if useCal else (FWD, REV)
    adj = math.radians(adj_deg if adj_deg is not None else (0.0 if useCal else 60.0))
    db, pll = Debounce(), PLL(fwd=dict(tf), rev=dict(tr), off=0.0)
    phi, w = phi0, 0.0
    s_true = bench.state(phi); db.state = db.cand = s_true; db.cnt = DEBOUNCE
    hdA = sector_mid(s_true, tf, tr); hdW = 0.0; noE = 10 ** 6; prevRaw = s_true
    state = "hall"; Ui = 0.0; traj = 0.0; iq = 0.4; cnt = 0; seeded = False
    cf = A * 0.01; edges = []; log = []; dmax = accel * DT
    for i in range(int(secs / DT)):
        t = i * DT
        raw = bench.state(phi)
        prevRaw = raw
        sd, sp, ne = db.read(raw)
        if ne:
            noE = 0
            hdW = 1.0 / (6.0 * max(1e-6, closed_loop_last(edges, t)))
            hdA = tf[sd]
            edges.append((t, sp, sd))
        else:
            noE += 1
            a_ = abs(hdW)
            if a_ > 0:
                exp = 1.0 / (a_ * 6.0 * DT)
                if noE >= exp: hdW = (exp / noE) * a_
            hdA = wrap(hdA + hdW * TWO_PI * DT)
        if noE >= STILL_ISRS: hdA = sector_mid(sd, tf, tr)
        pth, pw = pll.isr(sd, sp, ne)
        if state == "hall":
            iq = 0.4; foc = hdA
            if abs(hdW) > 0:
                state = "run"; Ui = 0.4; traj = hdW
        else:
            useP = mode in ("H3b", "H4")
            est, fb = (pth, pw) if useP else (hdA, hdW)
            if useP and not seeded and t < 1.0 and pll.n >= 2 and pw * ref > 0:
                traj = pw if abs(pw) < abs(ref) else ref; seeded = True
            traj += max(-dmax, min(dmax, ref - traj))
            cnt += 1
            if cnt >= 10:
                cnt = 0
                e = traj - fb
                Ui = max(-0.4, min(0.4, Ui + ki * e))
                iq = max(-0.4, min(0.4, kp * e + Ui))
            foc = est + adj
            log.append((t, phi, w, fb, iq))
        th = wrap(phi)
        tq = A * iq * math.cos(foc - th - OFFSET_TRUE) - B * w
        if abs(w) < 0.05 and abs(tq) < cf: w = 0.0
        else: w += (tq - math.copysign(cf, w if w != 0 else tq)) * DT
        phi += w * TWO_PI * DT
        if w < -30.0: return {"mode": mode, "result": "runaway", "t_s": round(t, 3)}
    return summarise(mode, edges, log, ref, secs)


def closed_loop_grid24(bench, cal, modes=("H2", "H3b", "H4fw", "H4"), kps=(0.05, 0.1), As=(300.0, 1000.0, 2500.0), secs=0.6):
    """12 starts (two places in each of the six sectors of one pole pair, and the next pole pair's) per mode, gain, plant"""
    starts = []
    for p in range(0, 24, 2):
        lo, hi = bench.B[p], bench.B[(p + 1) % 24] + (bench.L if p == 23 else 0.0)
        starts.append(lo + 0.3 * (hi - lo))
    md = lambda xs: sorted(xs)[len(xs) // 2] if xs else None
    out = {}
    for m in modes:
        for kp in kps:
            for A in As:
                rs = [closed_loop24(m, ph, bench, cal, kp=kp, A=A, B=A * 0.06 / 20.0, secs=secs) for ph in starts]
                ok = [r for r in rs if r["result"] == "ran"]
                out[f"{m}_kp{kp}_A{int(A)}"] = {
                    "ran": len(ok), "runaway": sum(r["result"] == "runaway" for r in rs), "stall": sum(r["result"] == "stall" for r in rs),
                    "backstep_in_first_6": sum("b" in r["first_edges"][:6] for r in ok),
                    "true_speed_sd_Hz": md([r["true_speed"][1] for r in ok]), "fb_speed_sd_Hz": md([r["fb_sd"] for r in ok]),
                    "iq_rms_A": md([r["iq_rms"] for r in ok]), "t20_s": md([r["t20_s"] for r in ok if r["t20_s"]])}
    return out


def h4_section():
    w24, fold = bench_widths24()
    bench = Bench24(w24)
    cals = [simulate_cal(bench, start_phi=bench.B[0] + k * TWO_PI + 0.3) for k in range(NPP)]
    cal = cals[0]
    ok, sf, sr, pairs = table_check(cal["fwd"], cal["rev"])
    # the calibrated table against the default one, and against the mean boundaries (the bias of the sweep itself)
    b6, r6 = bench_boundaries()
    vs_default = {s_: round(math.degrees(wrap(cal["fwd"][s_] - FWD[s_])), 1) for s_ in FWD_ORDER}
    bias_fwd = {s_: round(math.degrees(wrap(cal["fwd"][s_] - b6[s_] - OFFSET_TRUE)), 2) for s_ in FWD_ORDER}
    bias_rev = {s_: round(math.degrees(wrap(cal["rev"][s_] - r6[s_] - OFFSET_TRUE)), 2) for s_ in FWD_ORDER}
    scat = {f"{hz:g}Hz": {
        "default_table": steady_scatter24(bench, hz, FWD, REV, 0.0),
        "calibrated_table": steady_scatter24(bench, hz, cal["fwd"], cal["rev"], OFFSET_TRUE),
        "next_24_entry_table_oracle": steady_scatter24(bench, hz, b6, r6, OFFSET_TRUE, oracle=True)} for hz in (20.0, 60.0)}
    grid = closed_loop_grid24(bench, cal)
    return {
        "widths24_deg": [round(x, 2) for x in w24], "fold": fold,
        "pole_pair_boundary_dev_deg": {str(s_): [round(x, 1) for x in v] for s_, v in bench.dev.items()},
        "cal_predicted": {"fwd_deg": {str(s_): round(math.degrees(v), 1) for s_, v in cal["fwd"].items()},
                          "rev_deg": {str(s_): round(math.degrees(v), 1) for s_, v in cal["rev"].items()},
                          "fwd_minus_default_deg": vs_default, "bias_fwd_deg": bias_fwd, "bias_rev_deg": bias_rev,
                          "spread_fwd_deg": {str(k): round(v, 1) for k, v in cal["fwd_spread_deg"].items()},
                          "spread_rev_deg": {str(k): round(v, 1) for k, v in cal["rev_spread_deg"].items()},
                          "counts": [cal["fwd_counts"], cal["rev_counts"]], "seconds": cal["seconds"],
                          "check_ok": ok, "steps_fwd_deg": [round(x, 1) for x in sf], "steps_rev_deg": [round(x, 1) for x in sr],
                          "pairs_deg": [round(x, 1) for x in pairs],
                          "other_start_positions_fwd_deg": [{str(s_): round(math.degrees(v), 1) for s_, v in c["fwd"].items()} for c in cals[1:]]},
        "steady_open_loop": scat,
        "closed_loop": grid,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pole", type=float, default=POLE)
    ap.add_argument("--no-write", action="store_true")
    ap.add_argument("--h3b", action="store_true", help="also rewrite model-H3b.json (kept as committed otherwise)")
    a = ap.parse_args()
    b1 = sorted((RES / "batch1-20261019").glob("S0_*.capture.json"))
    spacing, widths = bench_spacing_estimate(b1)
    syn = synthetic_checks(a.pole, [round(x, 1) for x in spacing])
    rep = replay_all(a.pole)
    d1 = replay_d1()
    cl = closed_loop_grid()
    out = {"d1_replay_H3": d1, "closed_loop": cl, "pole": a.pole, "g1": 1 - a.pole ** 2, "g2": (1 - a.pole) ** 2, "stale": STALE, "cmax_deg": math.degrees(CMAX),
           "bench_sector_widths_deg": widths, "bench_spacing_err_deg": spacing, "synthetic": syn, "replay": rep}
    print(f"pole {a.pole}: G1 {out['g1']:.4f}, G2 {out['g2']:.4f}")
    print("bench sector widths (deg, batch 1 S0 runs):", {k: round(v, 1) for k, v in widths.items()})
    for k, v in syn.items():
        if k.startswith("const"):
            print(f"{k:28s} pll sd {v['pll']['sd']:.3f} Hz ({v['pll']['sd_pct']:.1f} %), firmware sd {v['firmware']['sd']:.3f} Hz "
                  f"({v['firmware']['sd_pct']:.1f} %), angle err sd {v['angle_err_deg']['sd']:.1f} max {v['angle_err_deg']['maxabs']:.1f} deg")
        else:
            print(k, v)
    for r in rep:
        s = r.get("steady") or []
        line = " ".join(f"[{w['from_s']}-{w['to_s']} s fw {w['firmware']['sd']:.2f} pll1ms {w['pll_1ms_edges']['sd']:.2f}"
                        f" rev {w['hall_rev']['sd'] if w['hall_rev'] else float('nan'):.2f} Hz]" for w in s)
        print(f"{r['batch']} {r['tag']:22s} kp {r['kp']} reason {r['reason']} pll+back {r['pll_positive_during_backward_runs']} "
              f"fw+back {r['firmware_positive_during_backward_runs']} {line}")
    print("d1 replay (H3 observer against the board):", d1)
    for k_, v_ in cl.items(): print("closed loop", k_, v_)
    h4 = h4_section()
    print("H4: calibrated table predicted", h4["cal_predicted"])
    for k_, v_ in h4["steady_open_loop"].items(): print("H4: steady", k_, v_)
    for k_, v_ in h4["closed_loop"].items(): print("closed loop 24", k_, v_)
    if not a.no_write:
        if a.h3b:
            OUT.parent.mkdir(parents=True, exist_ok=True)
            OUT.write_text(json.dumps(out, indent=1) + "\n")
            print("wrote", OUT.relative_to(ROOT))
        OUT_H4.parent.mkdir(parents=True, exist_ok=True)
        OUT_H4.write_text(json.dumps(h4, indent=1) + "\n")
        print("wrote", OUT_H4.relative_to(ROOT))


if __name__ == "__main__":
    main()
