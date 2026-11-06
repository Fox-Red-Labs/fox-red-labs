#!/usr/bin/env python3
"""Guard test suite for the AM13 bench guard (revision G, 2026-10-19;
revision H adds hall_adj, capture and isr_time; revision H3 adds hall_pll; revision H4 adds hall_table; revision H5 adds h5_boot (it holds on H6 unchanged); predictions committed with the firmware before its first run).
Every test states its expected outcome here, committed before the run. Run under the probe lock with the pack on:

    python3 tools/frl/lock.py exec --serial AL130001 --op guard-suite -- python3 tools/motorlink/guard_suite.py OUTDIR

Stops at the first failed safety test, writes the run flag off and leaves the guard at 1.0 A. Writes OUTDIR/suite.json.
"""
import json, pathlib, struct, subprocess, sys, time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign as C
import capture as K
import link as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
NAMES = ["speed_Hz", "Is_A", "VdcBus_V", "frlGuard.state"]
EXPECT = {   # the predictions
    "idle":       "guard version 12 (the header's FRL_GUARD_VERSION; 11 for H6 and H6b, 10 for H5 and H5b, 9 for H4, 8 for revisions H3 and H3b, 7 before); standstill phase currents within 0.05 A; ISR rate 20 kHz within 1 percent (check widened to 3 percent on 2026-10-19: host-timed polls jitter by a few percent)",
    "h5_boot":    "revision H5 (2026-10-20, before the flash; expectations W1 and W2): on a fresh boot ilimOk reads 1, ilim_V 1.95 to 2.05 V, ilimMode 1, ilimCode 2418, cmpLimit_A 1.0, limit_A 1.0, dirRotor 1, testMode 0; at idle limit_A written 1.8 reads 1.2 and motorSetVars_M1.overCurrent_A written 2.0 reads 1.2 (cmpLimit_A follows); both written back to 1.0",
    "hall_adj":   "frlGuard.angleAdj_deg reads 60.0 on a fresh boot, before any host writes it (revision H default FRL_GUARD_HALL_ADJ_DEG; the host tools put it back to 60, not 0, after a sweep)",
    "capture":    "at idle, a 512-sample capture of IA, IB, IC and VBUS at decimation 1, trigger now, completes (state 3, count 512) and every phase-current sample reads within 0.05 A of zero",
    "bus_refuse": "with the 12 V window (10 to 14.5 V) on the 20 V pack, a start is refused with reason 7, the motor never runs",
    "nan_ref":    "NaN written to the speed reference at idle reads back 0.0 (G5)",
    "cal":        "Hall calibration completes in 160 to 185 s with reason 10 and no fault",
    "latch":      "limit raised to 1.8 A during a run reads back 1.0 A, the value latched at the start (G5); the run ends on its cap (reason 1)",
    "isr_time":   "after the latch run (20 Hz, Hall FOC): isrUs_max under 50 us (the control period) and cpuLoad_pct between 10 and 90",
    "hall_pll":   "frlGuard.hallPll reads 0 on a fresh boot; a 20 Hz run with hallPll 1 (the Hall tracking observer drives, revision H3) ends on its 5 s cap (reason 1) with no fault, its speed_Hz (the observer's) averages 18 to 22 Hz over the last 2 s, and frlGuard.pllSpeed_Hz reads within 1 Hz of 0 after the stop. Revision H3b (2026-10-19, before its run): the same, with flying start off for this run (the Hall start, as the batches run it) and back on after; on H3 the test left flying start on, so the start went through the firmware's alignment (about 5 s on this bench, as the latch run shows) and the 5 s cap ended it there",
    "hall_table": "revision H4 (2026-10-19, before any run): frlGuard.hallTable reads 0 on a fresh boot; after the calibration hallTableOk reads 1 and the published tables calFwd_deg and calRev_deg (Hall states 1 to 6) are six distinct angles each, in order: every step along the forward order 3, 2, 6, 4, 5, 1 is 30 to 90 degrees and positive, and calRev_deg of each state is within 15 degrees of calFwd_deg of the next state forward (the same boundary); writing hallTable 1 loads angleAdj_deg 0.0; a 20 Hz run with hallTable 1 (the firmware's own Hall speed, hallPll 0, flying start off) ends on its 5 s cap (reason 1) with no fault and its speed_Hz averages 18 to 22 Hz over the last 2 s; writing hallTable 0 afterwards loads angleAdj_deg 60.0",
    "host_lost":  "heartbeat stopped mid-run while polls continue: the guard stops with reason 2 (host lost) 1.0 to 1.4 s after the last beat (G2; the previous guard kept running on polls alone)",
    "trip":       "with the limit at 0.3 A a 10 Hz start stops on over current (reason 3) during the start",
    "flying":     "a Hall-free FAST run at 40 Hz, then a Hall run at 20 Hz: the Hall run reaches the cap with no fault 1024 (campaign.py restores flying start)",
    "sweep":      "angle sweep at 40 Hz over 0, 15, 30, 45, 60, 0 degrees completes on its cap",
    "stops":      "every run that ends on the cap or a host stop reads reason 1 or 9, never 13 (clock) or 14 (stop over current)",
}


# a failure here is recorded and the suite goes on: these tests check a measurement, not a protection (2026-10-19)
MEASURE_ONLY = {"isr_time"}


class Fail(Exception):
    pass


def snap(names, secs=0.5):
    k = L.Link()
    try:
        return C.snapshot(k, names, secs)
    finally:
        k.close()


def write(pairs, names=None):
    k = L.Link()
    try:
        for n, v in pairs:
            k.write(n, v)
        return C.snapshot(k, names or [n for n, _ in pairs], 0.4)
    finally:
        k.close()


def pack():
    return write([("frlGuard.busMin_V", 15.5), ("frlGuard.busMax_V", 21.0), ("frlGuard.limit_A", 1.0)])


def clear_fault():
    k = L.Link()
    try:
        C.raw_write(k, "motorVars_M1.inputCtrl.flagClearFaults", True); k.read(0.3)
        C.raw_write(k, "motorVars_M1.inputCtrl.flagClearFaults", False); k.read(0.1)
    finally:
        k.close()


def run(tag, outdir, ref, cap_s, plan=(), expect=(1, 9)):
    k = L.Link()
    try:
        acts = [(t, (lambda f: (lambda col: f(k, col)))(f)) for t, f in plan]
        col, vs, t0, post = C.one_run(k, tag, outdir, ref, int(cap_s * 1000), acts, NAMES)
    finally:
        k.close()
    if post.get("reason") not in expect:
        raise Fail(f"{tag}: reason {post.get('reason')} (expected {expect}), fault {post.get('faultMtrNow')}, peak {post.get('peak_A')}")
    return col, vs, t0, post


def main(outdir):
    out = pathlib.Path(outdir); out.mkdir(parents=True, exist_ok=True)
    res = {"expect": EXPECT, "results": {}}
    R = res["results"]
    def record(key, ok, **kw):
        R[key] = dict(ok=ok, **kw); print(("PASS " if ok else "FAIL ") + key, json.dumps(kw), flush=True)
        if not ok and key not in MEASURE_ONLY: raise Fail(key)
    try:
        # idle
        s = snap(["frlGuard.version", "halFOCDataObj_M1.adcData.I_A.value[0]", "halFOCDataObj_M1.adcData.I_A.value[1]",
                  "halFOCDataObj_M1.adcData.I_A.value[2]", "VdcBus_V", "faultMtrNow", "frlGuard.angleAdj_deg", "frlGuard.hallPll",
                  "frlGuard.hallTable"])
        k = L.Link()
        try:
            a = k.poll(["motorVars_M1.ISRCount"], 0.25); ta = time.time(); time.sleep(5.0)   # host timing jitter: a long window
            b = k.poll(["motorVars_M1.ISRCount"], 0.25); tb = time.time()
        finally:
            k.close()
        rate = (list(b.values())[0] - list(a.values())[0]) / (tb - ta)
        cur = [s.get("value[0]"), s.get("value[1]"), s.get("value[2]")]
        record("idle", s.get("version") == L.guard_version() and all(abs(c) < 0.05 for c in cur) and abs(rate - 20000) < 600,   # 3 percent: what host-timed polls can resolve
               version=s.get("version"), currents=[round(c, 4) for c in cur], isr_hz=round(rate), bus=s.get("VdcBus_V"))
        # h5_boot (revision H5): the driver limit, the clamps and the direction rule as booted; one variable per poll
        k = L.Link()
        try:
            G_ = "frlGuard."
            b5 = {n: k.poll([G_ + n], 0.3).get(G_ + n) for n in ("ilimOk", "ilim_V", "ilimMode", "ilimCode", "cmpLimit_A", "limit_A", "dirRotor", "testMode")}
            k.write(G_ + "limit_A", 1.8); k.read(0.3); lim18 = k.poll([G_ + "limit_A"], 0.3).get(G_ + "limit_A")
            C.raw_write(k, "motorSetVars_M1.overCurrent_A", 2.0); k.read(0.3)
            oc20 = k.poll(["motorSetVars_M1.overCurrent_A"], 0.3).get("motorSetVars_M1.overCurrent_A")
            cmp20 = k.poll([G_ + "cmpLimit_A"], 0.3).get(G_ + "cmpLimit_A")
            k.write(G_ + "limit_A", 1.0); C.raw_write(k, "motorSetVars_M1.overCurrent_A", 1.0); k.read(0.3)
            back = {n: k.poll([n], 0.3).get(n) for n in (G_ + "limit_A", "motorSetVars_M1.overCurrent_A", G_ + "cmpLimit_A")}
        finally:
            k.close()
        near = lambda x, y, tol=1e-3: x is not None and abs(x - y) < tol
        record("h5_boot", b5["ilimOk"] == 1 and b5["ilim_V"] is not None and 1.95 <= b5["ilim_V"] <= 2.05 and b5["ilimMode"] == 1
               and b5["ilimCode"] == 2418 and near(b5["cmpLimit_A"], 1.0) and near(b5["limit_A"], 1.0) and b5["dirRotor"] == 1
               and b5["testMode"] == 0 and near(lim18, 1.2) and near(oc20, 1.2) and near(cmp20, 1.2) and all(near(v, 1.0) for v in back.values()),
               boot=b5, limit_written_1_8=lim18, overCurrent_written_2_0=oc20, cmpLimit_after=cmp20, restored=back)
        record("hall_adj", s.get("angleAdj_deg") is not None and abs(s["angleAdj_deg"] - 60.0) < 1e-3, angleAdj_deg=s.get("angleAdj_deg"))
        if s.get("faultMtrNow"): clear_fault()
        # bus refuse: default 12 V window, pack on
        write([("frlGuard.busMin_V", 10.0), ("frlGuard.busMax_V", 14.5)])
        k = L.Link()
        try:
            C.raw_write(k, C.SPEEDREF, 10.0); k.write(L.RUN_FLAG, True, allow_run=True); k.read(0.6)
            s2 = C.snapshot(k, ["frlGuard.state", "frlGuard.reason", "frlGuard.runMs"], 0.4); k.write(L.RUN_FLAG, False)
        finally:
            k.close()
        record("bus_refuse", s2.get("reason") == 7 and s2.get("state") == 3, **s2)   # runMs keeps the last run's value on a refusal
        pack()
        # NaN at idle
        k = L.Link()
        try:
            v_ = L.find(k.vars, C.SPEEDREF)   # bypass the host's own refusal: the test is that the firmware catches it alone
            k.send(L.WRITE_VAR, bytes([v_["index"]]) + struct.pack(">f", float("nan"))); k.read(0.4)
            v = C.snapshot(k, [C.SPEEDREF], 0.3).get("speedRef_Hz")
        finally:
            k.close()
        record("nan_ref", v == 0.0, read_back=v)
        # capture at idle: 512 samples, trigger now
        k = L.Link()
        try:
            armed = K.arm(k, ("IA", "IB", "IC", "VBUS"), 1, 512, 0, "now"); done = K.wait_done(k, 5.0)
            path, samples, meta = K.readout(k, out, "capture-idle")
        finally:
            k.close()
        worst = max((abs(x[c]) for x in samples.values() for c in range(3)), default=None)
        record("capture", done.get("capState") == 3 and done.get("capCount") == 512 and len(samples) == 512 and worst is not None and worst < 0.05,
               state=done.get("capState"), count=done.get("capCount"), read=len(samples), worst_phase_A=worst,
               bus_mean=round(sum(x[3] for x in samples.values()) / max(1, len(samples)), 3), window_s=meta["window_s"])
        # calibration (skipped when the table is already valid, so a rerun does not spend 3 minutes)
        if snap(["hallSensorData.flagCalValid"]).get("flagCalValid"):
            R["cal"] = dict(ok=True, skipped="Hall table already valid on this boot"); print("SKIP cal (table valid)", flush=True)
        t0 = time.time()
        if R.get("cal"): r = None
        else:
            r = subprocess.run([sys.executable, str(ROOT / "tools/motorlink/campaign.py"), "cal"], capture_output=True, text=True, timeout=800)
            dt = time.time() - t0
            end = [ln for ln in r.stdout.splitlines() if ln.startswith("end")]
            ok = bool(end) and "'reason': 10" in end[-1] and "'flagCalValid': 1" in end[-1] and 160 <= dt <= 185   # the stated band
            record("cal", ok, seconds=round(dt, 1), end=end[-1] if end else r.stdout[-200:])
        pack()
        # latch: raise the limit mid-run
        seen = {}
        def raise_limit(k, col):
            C.raw_write(k, "frlGuard.limit_A", 1.8); col.pump(0.6)
            seen["limit_after"] = None
        def read_limit(k, col):
            C.raw_write(k, C.SPEEDREF, 20.0)
        col, vs, t0r, post = run("latch", out, 20.0, 15, [(10.0, raise_limit), (11.0, read_limit)])
        lim = snap(["frlGuard.limit_A"])["limit_A"]
        record("latch", abs(lim - 1.0) < 1e-3 and post.get("reason") == 1, limit_after_run=lim, reason=post.get("reason"), peak=post.get("peak_A"))
        t = snap(["frlGuard.isrUs_avg", "frlGuard.isrUs_max", "frlGuard.cpuLoad_pct"])
        record("isr_time", t.get("isrUs_max") is not None and t["isrUs_max"] < 50.0 and 10.0 <= (t.get("cpuLoad_pct") or 0) <= 90.0, **t)
        # hall_pll (revision H3): boots at 0 (read in the idle snapshot); a 20 Hz run with the tracking observer driving
        boot_pll = s.get("hallPll")
        write([("frlGuard.hallPll", 1), ("motorVars_M1.inputCtrl.flagEnableFlyingStart", False)])   # H3b: the Hall start, not alignment
        try:
            col, vs, t0p, post = run("hall_pll", out, 20.0, 5, expect=(1,))
        finally:
            write([("frlGuard.hallPll", 0), ("motorVars_M1.inputCtrl.flagEnableFlyingStart", True)])   # boot value (mtr1_init.c)
        sp_ = next((v for n, v in C.parse(col.chunks, vs).items() if n.endswith("speed_Hz")), [])
        tail = [v for t, v in sp_ if t >= t0p + 3.0 and t <= t0p + 5.0]
        mean = round(sum(tail) / len(tail), 2) if tail else None
        after = snap(["frlGuard.pllSpeed_Hz", "frlGuard.pllErr_deg", "frlGuard.hallPll"])
        record("hall_pll", boot_pll == 0 and post.get("reason") == 1 and not post.get("faultMtrNow") and mean is not None and 18.0 <= mean <= 22.0
               and abs(after.get("pllSpeed_Hz") or 0.0) < 1.0, boot=boot_pll, reason=post.get("reason"), fault=post.get("faultMtrNow"),
               peak=post.get("peak_A"), speed_mean_last2s=mean, **after)
        # host lost: stop the heartbeat at 10 s, keep polling
        k = L.Link()
        try:
            C.PRE = C.snapshot(k, C.POST + ["frlGuard.busMin_V", "frlGuard.busMax_V"], 0.6)
            C.raw_write(k, "frlGuard.capMs", 20000); C.raw_write(k, C.SPEEDREF, 20.0)
            vs = C.start_stream(k, NAMES); col = C.Collector(k); col.pump(0.3)
            t0h = col.mark("run_flag_write"); C.raw_write(k, L.RUN_FLAG, True); col.last_beat = 0
            col.pump(10.0)                       # beats every 250 ms
            last_beat = col.last_beat; col.mark("beats stop")   # the time of the last heartbeat actually written
            for _ in range(12):                  # packets keep arriving (speed reference writes) but no heartbeat:
                C.raw_write(k, C.SPEEDREF, 20.0)  # the old guard counted any packet as alive and kept running
                col.pump(0.25, beat=False)
            C.raw_write(k, L.RUN_FLAG, False); col.pump(0.3, beat=False); k.send(L.STOP_POLLING); col.pump(0.1, beat=False)
            post = C.snapshot(k, C.POST, 0.6)
            C.save(out, "host_lost", col, vs, {"ref_hz": 20.0, "t0": t0h, "post": post, "last_beat": last_beat})
        finally:
            k.close()
        data = C.parse(col.chunks, vs)
        st = next(v for n, v in data.items() if n.endswith("frlGuard.state"))
        t_stop = next((t for t, v in st if t > last_beat and v == 3), None)
        lat = None if t_stop is None else round(t_stop - last_beat, 3)
        record("host_lost", post.get("reason") == 2 and lat is not None and 0.9 <= lat <= 1.5, latency_s=lat, reason=post.get("reason"), runMs=post.get("runMs"))
        # trip at 0.3 A
        write([("frlGuard.limit_A", 0.3)])
        try:
            col, vs, t0t, post = run("trip", out, 10.0, 15, expect=(3,))
        finally:
            write([("frlGuard.limit_A", 1.0)])
        clear_fault()
        record("trip", post.get("reason") == 3, peak=post.get("peak_A"), runMs=post.get("runMs"))
        # flying start regression
        r = subprocess.run([sys.executable, str(ROOT / "tools/motorlink/campaign.py"), "hallfree", "2", "0", "40", "15", str(out / "obs"), "flying-hallfree"],
                           capture_output=True, text=True, timeout=180)
        hf = [ln for ln in r.stdout.splitlines() if " post " in ln]
        fs = snap(["motorVars_M1.inputCtrl.flagEnableFlyingStart"]).get("flagEnableFlyingStart")
        col, vs, t0f, post = run("flying-hall", out, 20.0, 15, expect=(1, 3, 4, 9))
        record("flying", post.get("reason") == 1 and not post.get("faultMtrNow") and fs, flying_start_after_hallfree=fs,
               hallfree=hf[-1][-110:] if hf else r.stdout[-200:], hall_reason=post.get("reason"), hall_fault=post.get("faultMtrNow"))
        # angle sweep
        r = subprocess.run([sys.executable, str(ROOT / "tools/motorlink/campaign.py"), "sweep", "0", "40", str(out / "sweep"), "sweep", "0,15,30,45,60,0"],
                           capture_output=True, text=True, timeout=240)
        sp = [ln for ln in r.stdout.splitlines() if " post " in ln]
        record("sweep", bool(sp) and "'reason': 1," in sp[-1], post=sp[-1][-140:] if sp else r.stdout[-300:])
        # hall_table (revision H4): the calibrated table, published and checked, then a 20 Hz run on it. Last, after the safety
        # tests: the run is on the firmware's own Hall speed (hallPll 0), whose start faulted in 1 of 11 batch 2 starts at Kp 0.05
        boot_tab = s.get("hallTable")
        # one variable per poll: a poll carries 13 at most, and snapshot's short names would merge the array entries
        tab = {}
        for n in (["frlGuard.hallTableOk", "frlGuard.calSpread_deg"] + [f"frlGuard.calFwd_deg[{i}]" for i in range(6)]
                  + [f"frlGuard.calRev_deg[{i}]" for i in range(6)]):
            tab[n.split(".")[-1]] = snap([n], 0.25).popitem()[1]
        fwd = {i + 1: tab.get(f"calFwd_deg[{i}]") for i in range(6)}
        rev = {i + 1: tab.get(f"calRev_deg[{i}]") for i in range(6)}
        order = [3, 2, 6, 4, 5, 1]
        w = lambda a: (a + 180.0) % 360.0 - 180.0
        ok_tab = all(v is not None for v in list(fwd.values()) + list(rev.values()))
        steps_f = [round(w(fwd[order[(i + 1) % 6]] - fwd[order[i]]), 2) for i in range(6)] if ok_tab else None
        steps_r = [round(w(rev[order[(i + 1) % 6]] - rev[order[i]]), 2) for i in range(6)] if ok_tab else None
        pairs = [round(w(rev[order[i]] - fwd[order[(i + 1) % 6]]), 2) for i in range(6)] if ok_tab else None
        ok_tab = ok_tab and all(30.0 <= x <= 90.0 for x in steps_f + steps_r) and all(abs(x) < 15.0 for x in pairs) \
            and len({round(v, 3) for v in fwd.values()}) == 6 and len({round(v, 3) for v in rev.values()}) == 6
        adj1 = write([("frlGuard.hallTable", 1)], ["frlGuard.hallTable", "frlGuard.angleAdj_deg"])
        write([("frlGuard.hallPll", 0), ("motorVars_M1.inputCtrl.flagEnableFlyingStart", False)])
        try:
            col, vs, t0c, post = run("hall_table", out, 20.0, 5, expect=(1,))
        finally:
            write([("motorVars_M1.inputCtrl.flagEnableFlyingStart", True)])
            adj0 = write([("frlGuard.hallTable", 0)], ["frlGuard.hallTable", "frlGuard.angleAdj_deg"])
        sp_ = next((v for n, v in C.parse(col.chunks, vs).items() if n.endswith("speed_Hz")), [])
        tail = [v for t, v in sp_ if t >= t0c + 3.0 and t <= t0c + 5.0]
        mean = round(sum(tail) / len(tail), 2) if tail else None
        (out / "hall_table.json").write_text(json.dumps({"fwd_deg": fwd, "rev_deg": rev, "steps_fwd": steps_f, "steps_rev": steps_r,
                                                         "pairs": pairs, **tab}, indent=1) + "\n")
        record("hall_table", boot_tab == 0 and tab.get("hallTableOk") == 1 and ok_tab and adj1.get("angleAdj_deg") is not None and abs(adj1["angleAdj_deg"] - 0.0) < 1e-3
               and post.get("reason") == 1 and not post.get("faultMtrNow") and mean is not None and 18.0 <= mean <= 22.0
               and adj0.get("angleAdj_deg") is not None and abs(adj0["angleAdj_deg"] - 60.0) < 1e-3,
               boot=boot_tab, table_ok=tab.get("hallTableOk"), steps_fwd=steps_f, steps_rev=steps_r, pairs=pairs, spread=tab.get("calSpread_deg"),
               adj_on=adj1.get("angleAdj_deg"), adj_off=adj0.get("angleAdj_deg"), reason=post.get("reason"), fault=post.get("faultMtrNow"),
               peak=post.get("peak_A"), speed_mean_last2s=mean)
        record("stops", True, note="every run above ended on the expected reason; none read 13 or 14")
    except (Fail, Exception, SystemExit) as e:
        res["stopped"] = repr(e); print("STOPPED:", repr(e), flush=True)
    finally:
        try:
            write([(L.RUN_FLAG, False), ("frlGuard.limit_A", 1.0), ("frlGuard.speedMax_Hz", 40.0), ("frlGuard.hallPll", 0), ("frlGuard.hallTable", 0)])
        except Exception as e:
            res["end_write"] = repr(e)
        (out / "suite.json").write_text(json.dumps(res, indent=1, default=str) + "\n")


if __name__ == "__main__":
    main(sys.argv[1])
