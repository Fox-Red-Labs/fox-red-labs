#!/usr/bin/env python3
"""Radar evaluation platform v0: antenna-configuration comparison on the IWRL6432BOOST running TI's
Motion and Presence 2D demo. Python 3 + pyserial (use ~/.venvs/iwrl6432/bin/python).

  radar_eval.py accept   [--configs 1x1,2x2,2x3,...]      apply each channelCfg, read the CLI replies, check the stream
  radar_eval.py empty    [--configs ...] [--seconds 300]  empty-room window per config (nobody in the room)
  radar_eval.py run      [--configs ...] [--passes walk,sit2,sit4] [--rounds 1] [--ask-timeout 900]
                                                          scripted passes; each human pass is a dashboard NEEDS YOU
                                                          (or a keypress in this terminal) and waits for done
  radar_eval.py restore                                    stock PresenceDetect.cfg (lowPowerCfg 1) back on the demo
  radar_eval.py summarize                                  rebuild results/<date>-<config>.summary.json from the frame CSVs

Why `lowPowerCfg 0` in every eval config: the demo honours sensorStop only when lowPowerCfg is 0 (motion_detect.c, "Sensor Stop
command is honoured only when Low Power Cfg is disabled") and does not read the CLI while it runs with 1. The shipped cfg has 1,
so the first eval after a boot needs a fresh boot (NRST); `restore` puts lowPowerCfg 1 back at the end.
Presence = any zone nonzero in TLV 315. Range: the demo reports zones (boxes) and a minor-motion range profile (TLV 303);
no point range. Latencies are measured from the cue this tool gives, so they include the person's own reaction and walk time;
the same cue is used for every config, so differences between configs are fair, absolute values are not a sensor latency."""
import argparse, csv, json, os, random, statistics, subprocess, sys, time, select, datetime
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from radar_link import Radar, CONFIGS, cfg_lines, decode, CFG, PORT  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
RES = os.path.join(HERE, "results")
STATE = os.path.expanduser(os.environ.get("BENCH_STATE", "~/.frl/bench/state.json"))
BOARD = "iwrl6432boost"
APPLIED = []
EVAL_OVR = {"lowPowerCfg": "0"}
# range bin width from the cfg: 256 real samples at 12.5 Msps = 20.48 us, slope 60.5 MHz/us -> B = 1239 MHz -> c/2B
BANDWIDTH_MHZ = 60.5 * 256 / 12.5
M_PER_BIN = 299.792458 / (2 * BANDWIDTH_MHZ)   # c in m/us * MHz -> m ; 0.121 m
BIN_LO, BIN_HI = 2, 62                          # rangeSelCfg 0.25 to 7.5 m

def say(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)

def speak(msg, twice=False, block=False):
    say("SPOKEN: " + msg); dash("note", "radar voice: " + msg)
    try: (subprocess.run if block else subprocess.Popen)(["bash", "-c", 'say "$1"; ' + ('say "$1"' if twice else ":"), "_", msg], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception: pass

DASH = os.path.join(ROOT, "tools/bench_dashboard/state.py")   # the lab's hands-needed tracker; absent in the reader copy, so every call is optional
def dash(*args):
    if not os.path.exists(DASH):
        return ""
    return subprocess.run([sys.executable, DASH, *args], capture_output=True, text=True, cwd=ROOT).stdout.strip()

def board_status(activity, status="running"):
    dash("board", BOARD, "--activity", activity, "--status", status)

def need_done(nid):
    try:
        for n in json.load(open(STATE))["needs"]:
            if n["id"] == nid: return bool(n.get("done"))
    except Exception: pass
    return False

def wait_for_owner(text, image=None, timeout=900, kind="hands"):
    """Post NEEDS YOU, print it, wait for dashboard done or Enter in the terminal. Returns 'done' or 'timeout'."""
    a = ["need", "add", "NEEDS YOU: " + text, "--board", BOARD, "--kind", kind]
    if image: a += ["--image", image]
    out = dash(*a)
    try: nid = int(out.split()[-1])
    except Exception: nid = None
    board_status("waiting for the operator: " + text[:90], "waiting")
    say("NEEDS YOU: " + text + (f"  (dashboard need {nid}; or press Enter here)" if nid else ""))
    speak("The radar test needs you. Please look at the dashboard.")
    t0 = time.time()
    while time.time() - t0 < timeout:
        if nid and need_done(nid): return "done", nid
        if sys.stdin.isatty():
            r, _, _ = select.select([sys.stdin], [], [], 1.0)
            if r: sys.stdin.readline(); return "done", nid
        else: time.sleep(1.0)
    return "timeout", nid

def announce(text, image=None, countdown=10, kind="hands"):
    """Say the whole instruction twice (the voice stands alone), post the same words as the dashboard record, then start after `countdown` s.
    A dashboard done or a keypress starts it early. Returns ("done", nid)."""
    a = ["need", "add", "NEEDS YOU: " + text, "--board", BOARD, "--kind", kind]
    if image: a += ["--image", image]
    out = dash(*a)
    try: nid = int(out.split()[-1])
    except Exception: nid = None
    board_status("instruction given: " + text[:90], "waiting")
    speak(text, True, block=True)
    t0 = time.time()
    say(f"starting in {countdown} s")
    while time.time() - t0 < countdown:
        if nid and need_done(nid): break
        time.sleep(0.5)
    return "done", nid

def close_need(nid):
    if nid: dash("need", "done", str(nid))

# ---------------------------------------------------------------- radar control
def boot_state(r):
    """-> 'running' (streaming frames) | 'cli' (answers the CLI) | 'silent'"""
    r.flush(); t = time.time(); n = 0
    while time.time() - t < 3: n += len(r.poll())
    if n: return "running"
    ok, rep = r.cmd("version", 2.0)
    return "cli" if "Version" in rep or "Platform" in rep else "silent"

def apply_config(r, name, log=say, overrides=EVAL_OVR):
    """sensorStop, cfg lines with channelCfg for `name`, sensorStart. Returns dict with replies and verdict."""
    lines = cfg_lines(CFG, name, overrides)
    res = r.apply(lines, log=lambda s: None)
    bad = [(l, rep) for l, ok, rep in res if not ok and not l.startswith("sensorStop")]
    stop = [(ok, rep) for l, ok, rep in res if l.startswith("sensorStop")]
    log(f"config {name}: {len(res)} lines sent, {len(bad)} not answered Done" + ("" if not bad else ": " + "; ".join(f"{l} -> {rep or '(silent)'}" for l, rep in bad[:3])))
    return {"config": name, "channelCfg": [l for l in lines if l.startswith("channelCfg")][0],
            "replies": [{"line": l, "ok": ok, "reply": rep} for l, ok, rep in res], "not_done": [{"line": l, "reply": rep} for l, rep in bad]}

def stream_check(r, secs=8):
    """Read the stream for `secs`: frame count, frame-number continuity, rate, bytes/frame, zone count, profile sanity."""
    r.flush(); t0 = time.time(); fr = []
    while time.time() - t0 < secs: fr += r.poll()
    if not fr: return {"frames": 0}
    nums = [h["frame"] for _, h, _ in fr]
    span = fr[-1][0] - fr[0][0]
    d = decode(fr[-1][2])
    return {"frames": len(fr), "frame_first": nums[0], "frame_last": nums[-1],
            "gaps": sum(1 for a, b in zip(nums, nums[1:]) if b != a + 1),
            "rate_hz": round((len(fr) - 1) / span, 3) if span > 0 else None,
            "bytes_per_frame": statistics.median(h["length"] for _, h, _ in fr),
            "zones": d["nz"], "tlvs": sorted({t for _, _, tl in fr for t, _, _ in tl}),
            "profile_bins": len(d["prof"]) if d["prof"] else 0,
            "profile_median": statistics.median(d["prof"]) if d["prof"] else None}

# ---------------------------------------------------------------- recording
class Rec:
    FIELDS = ["config", "pass", "round", "t", "frame", "pres", "z1", "z2", "z3", "z4", "peak_bin", "peak_val", "event"]
    def __init__(self, r, cfgname, date):
        self.r, self.cfg, self.date = r, cfgname, date
        os.makedirs(RES, exist_ok=True)
        self.path = os.path.join(RES, f"{date}-{cfgname}.csv")
        new = not os.path.exists(self.path)
        self.f = open(self.path, "a", newline=""); self.w = csv.writer(self.f)
        if new: self.w.writerow(self.FIELDS)
    def event(self, pas, rnd, ev, t=None):
        t = t or time.time(); self.w.writerow([self.cfg, pas, rnd, f"{t:.3f}", "", "", "", "", "", "", "", "", ev]); self.f.flush(); return t
    def record(self, pas, rnd, secs, until=None):
        """Read frames for `secs` seconds, one CSV row per frame. Returns rows (dicts)."""
        end = time.time() + secs; rows = []
        while time.time() < end and not (until and until()):
            for t, h, tl in self.r.poll():
                d = decode(tl); z = (d["zones"] + [0, 0, 0, 0])[:4]
                pk = pv = ""
                if d["prof"]:
                    seg = d["prof"][BIN_LO:BIN_HI + 1]; pv = max(seg); pk = BIN_LO + seg.index(pv)
                row = [self.cfg, pas, rnd, f"{t:.3f}", h["frame"], d["pres"] if d["pres"] is not None else "", *z, pk, pv, ""]
                self.w.writerow(row); rows.append(dict(t=t, frame=h["frame"], pres=d["pres"], z=z, peak=pk, val=pv))
        self.f.flush(); return rows
    def close(self): self.f.close()

# ---------------------------------------------------------------- analysis
def runs_of(rows, key=lambda x: x["pres"] == 1, merge_gap=2.0):
    """Presence runs [(t_start, t_end)] with gaps shorter than merge_gap merged."""
    out = []
    for x in rows:
        if key(x):
            if out and x["t"] - out[-1][1] <= merge_gap: out[-1][1] = x["t"]
            else: out.append([x["t"], x["t"]])
    return out

def first_sustained(rows, t_from, key=lambda x: x["pres"] == 1, n=2):
    s = [x for x in rows if x["t"] >= t_from]
    for i in range(len(s) - n + 1):
        if all(key(x) for x in s[i:i + n]): return s[i]["t"]
    return None

def first_clear(rows, t_from, hold=3.0):
    s = [x for x in rows if x["t"] >= t_from]
    for i, x in enumerate(s):
        if x["pres"] == 0 and all(y["pres"] == 0 for y in s[i:] if y["t"] <= x["t"] + hold) and s[-1]["t"] >= x["t"] + hold:
            return x["t"]
    return None

def analyse_empty(rows):
    if len(rows) < 2: return {"frames": len(rows)}
    dur = rows[-1]["t"] - rows[0]["t"]; runs = runs_of(rows)
    carry = [r for r in runs if r[0] - rows[0]["t"] < 1.0]      # flag already on when the window opened: the person leaving plus the demo's hold time, not a false alarm
    runs = [r for r in runs if r not in carry]
    on = sum(1 for x in rows if x["pres"] == 1)
    return {"seconds": round(dur, 1), "frames": len(rows), "false_alarms": len(runs), "on_frames": on,
            "alarm_runs_s": [round(b - a + 0.25, 2) for a, b in runs], "carry_over_s": round(carry[0][1] - carry[0][0] + 0.25, 1) if carry else 0,
            "per_hour": round(len(runs) / dur * 3600, 1) if dur > 0 else None,
            "upper95_per_hour_if_zero": round(3 / dur * 3600, 1) if (dur > 0 and not runs) else None}

def analyse_sit(rows, t_enter, t_leave, dist):
    hold = [x for x in rows if t_enter + 15 <= x["t"] <= t_leave]
    det = first_sustained(rows, t_enter)
    clr = first_clear(rows, t_leave)
    peaks = [x["peak"] for x in hold if x["pres"] == 1 and x["peak"] != ""]
    pre = [x for x in rows if x["t"] < t_enter]
    return {"distance_m": dist, "already_on_at_cue": bool(pre and pre[-1]["pres"] == 1),
            "detect_latency_s": round(det - t_enter, 2) if det else None,
            "hold_on_fraction": round(sum(1 for x in hold if x["pres"] == 1) / len(hold), 3) if hold else None,
            "clear_latency_s": round(clr - t_leave, 2) if clr else None,
            "median_peak_bin": statistics.median(peaks) if peaks else None,
            "median_peak_m": round(statistics.median(peaks) * M_PER_BIN, 2) if peaks else None,
            "far_zone_fraction": round(sum(1 for x in hold if x["z"][1] or x["z"][3]) / len(hold), 3) if hold else None}

def analyse_walk(rows, t0, marks):
    """marks: [(distance_m, t_cue)] person at distance d from t_cue until the next cue. Detection range = distance at first sustained presence."""
    def dist_at(t):
        d = marks[0][0]
        for i, (dm, tc) in enumerate(marks):
            if t >= tc:
                nxt = marks[i + 1] if i + 1 < len(marks) else None
                d = dm if not nxt else dm + (nxt[0] - dm) * min(1.0, (t - tc) / (nxt[1] - tc))
        return d
    td = first_sustained(rows, t0)
    pre = [x for x in rows if x["t"] < t0]
    return {"start_m": marks[0][0], "already_on_at_start": bool(pre and pre[-1]["pres"] == 1),
            "detect_t_s": round(td - t0, 2) if td else None,
            "detect_range_m": round(dist_at(td), 2) if td else None,
            "presence_frames_walk": sum(1 for x in rows if t0 <= x["t"] <= marks[-1][1] + 4 and x["pres"] == 1)}

# ---------------------------------------------------------------- passes
def p_empty(rec, rnd, secs):
    rec.event("empty", rnd, "start"); rows = rec.record("empty", rnd, secs)
    return analyse_empty(rows)

def p_sit(rec, rnd, dist, tag, ask_timeout, image):
    """Human: out of the field -> ENTER cue -> sit still at dist -> LEAVE cue -> out."""
    ans, nid = announce(f"Please stand outside the radar's view, out of the room is best, for about one minute. I start in ten seconds. When I say enter, walk to the chair {dist:g} metres straight in front of the radar and sit still, phone down, for forty seconds. Then I say leave: walk out of the room and stay out for thirty seconds. I will say done.", image)
    if ans != "done": return {"status": "no answer in %d s" % ask_timeout}, False
    close_need(nid); rec.r.flush(); board_status(f"seated pass at {dist} m, {rec.cfg}, recording")
    rows = rec.record(tag, rnd, 20)
    t_e = rec.event(tag, rnd, "cue_enter"); speak(f"Sit now in the chair {dist:g} metres in front of the radar, stay still, phone down, for forty seconds. I will say done.", True); say(f"CUE enter, sit at {dist} m")
    rows += rec.record(tag, rnd, 45)
    t_l = rec.event(tag, rnd, "cue_leave"); speak("Leave the room now and close the door. Stay out for thirty seconds. I will call you back.", True); say("CUE leave")
    rows += rec.record(tag, rnd, 30)
    speak("Done, thank you. You can come back in."); return analyse_sit(rows, t_e, t_l, dist), True

def p_walk(rec, rnd, ask_timeout, image, marks_m=(8, 7, 6, 5, 4, 3, 2, 1), step_s=4):
    ans, nid = announce("Please start outside the room, behind the door, and stay out of the radar's view. I start in ten seconds. When I say go, open the door and walk straight toward the radar at a steady pace, and stop one metre in front of it; I call each metre as you go. Then I say leave: walk out of the room and stay out for twenty five seconds. I will say done.", image)
    if ans != "done": return {"status": "no answer in %d s" % ask_timeout}, False
    close_need(nid); rec.r.flush(); board_status(f"walking pass, {rec.cfg}, recording")
    rows = rec.record("walk", rnd, 20)
    t0 = rec.event("walk", rnd, "cue_go"); speak("Go. Open the door and walk straight toward the radar at a steady pace, and stop one metre in front of it. About forty seconds. I will call each metre.", True); say("CUE go, 8 m")
    marks = [(marks_m[0], t0)]
    for d in marks_m[1:]:
        rows += rec.record("walk", rnd, step_s)
        tc = rec.event("walk", rnd, f"mark_{d}m"); marks.append((d, tc)); speak(str(d)); say(f"CUE {d} m")
    rows += rec.record("walk", rnd, step_s)
    t_l = rec.event("walk", rnd, "cue_leave"); speak("Leave the room now and close the door. Stay out for twenty five seconds. I will call you back.", True); say("CUE leave")
    rows += rec.record("walk", rnd, 25); speak("Done, thank you. You can come back in.")
    out = analyse_walk(rows, t0, marks); out["clear_latency_s"] = None
    c = first_clear(rows, t_l); out["clear_latency_s"] = round(c - t_l, 2) if c else None
    return out, True

# ---------------------------------------------------------------- summary
def load_frames(path):
    out = {}
    for row in csv.DictReader(open(path)):
        k = (row["pass"], row["round"])
        out.setdefault(k, {"rows": [], "events": {}})
        if row["event"]: out[k]["events"][row["event"]] = float(row["t"])
        elif row["pres"] != "" or row["frame"]:
            out[k]["rows"].append(dict(t=float(row["t"]), frame=int(row["frame"]), pres=int(row["pres"]) if row["pres"] != "" else None,
                                       z=[int(row[f"z{i}"] or 0) for i in range(1, 5)], peak=int(row["peak_bin"]) if row["peak_bin"] != "" else "", val=row["peak_val"]))
    return out

def summarize_file(path):
    cfg = os.path.basename(path).split("-", 3)[3].rsplit(".csv", 1)[0]
    date = "-".join(os.path.basename(path).split("-")[:3])
    S = {"config": cfg, "date": date, "passes": {}, "m_per_bin_derived": round(M_PER_BIN, 4)}
    for (p, rnd), d in load_frames(path).items():
        ev, rows = d["events"], d["rows"]
        if not rows: continue
        if p == "empty": res = analyse_empty(rows)
        elif __import__("re").fullmatch(r"sit\d+(\.\d+)?", p) and "cue_enter" in ev:
            res = analyse_sit(rows, ev["cue_enter"], ev["cue_leave"], float(p[3:]))
        elif p == "walk" and "cue_go" in ev:
            ms = [(8, ev["cue_go"])] + [(int(k[5:-1]), t) for k, t in sorted(ev.items(), key=lambda kv: kv[1]) if k.startswith("mark_")]
            res = analyse_walk(rows, ev["cue_go"], ms)
            c = first_clear(rows, ev["cue_leave"]) if "cue_leave" in ev else None
            res["clear_latency_s"] = round(c - ev["cue_leave"], 2) if c else None
        elif p == "rate":
            n = [x["frame"] for x in rows]; span = rows[-1]["t"] - rows[0]["t"]
            res = {"frames": len(rows), "gaps": sum(1 for a, b in zip(n, n[1:]) if b != a + 1), "rate_hz": round((len(rows) - 1) / span, 3), "seconds": round(span, 1)}
        else: continue
        S["passes"].setdefault(p, []).append({"round": rnd, **res})
    jp = os.path.join(RES, os.path.basename(path).replace(".csv", ".summary.json"))
    old = json.load(open(jp)) if os.path.exists(jp) else {}
    for k in ("accept", "stream_check", "earlier_attempt", "label"):
        if k in old: S[k] = old[k]
    json.dump(S, open(jp, "w"), indent=1); return jp

def save_meta(date, cfg, key, val):
    jp = os.path.join(RES, f"{date}-{cfg}.summary.json"); os.makedirs(RES, exist_ok=True)
    S = json.load(open(jp)) if os.path.exists(jp) else {"config": cfg, "date": date, "passes": {}}
    S[key] = val; json.dump(S, open(jp, "w"), indent=1)

# ---------------------------------------------------------------- commands
FORWARD = {"port": "/dev/cu.usbmodemMG0000011", "wfile": os.path.join(ROOT, "cc35x1/csi/results/2026-10-10-freerun.csv")}

def open_radar():
    r = Radar(PORT)
    if FORWARD["port"]:
        try:
            r.enable_forward(FORWARD["port"], FORWARD["wfile"]); dash("note", f"radar eval: forwarding radar frames and the Wi-Fi line to {FORWARD['port']}")
        except Exception as e:
            say(f"forwarding to {FORWARD['port']} not started: {e}")
    st = boot_state(r)
    say(f"radar state: {st}")
    if st == "running" and APPLIED:
        return r
    if st == "running":
        # try to stop it (works only with lowPowerCfg 0)
        r.cmd("sensorStop 0", 2.0); time.sleep(1.0); r.flush(); t = time.time(); n = 0
        while time.time() - t < 2: n += len(r.poll())
        if n:
            say("The demo is streaming and ignores the CLI (lowPowerCfg 1). It needs one NRST (S2) press. Stopping.")
            sys.exit(3)
    elif st == "silent":
        say("No stream and no CLI answer. Stopping."); sys.exit(4)
    return r

def cmd_accept(a):
    date = a.date; r = open_radar(); allres = {}
    boot_t = time.strftime("%H:%M")
    for i, c in enumerate(a.configs):
        say(f"== accept {c}")
        board_status(f"config acceptance {c}")
        rep = apply_config(r, c); sc = stream_check(r, 8)
        say(f"   stream: {sc}")
        rep["stream"] = sc; allres[c] = rep
        save_meta(date, c, "accept", {**{k: rep[k] for k in ("channelCfg", "not_done")}, "start_in_boot": i + 1, "boot": f"the boot this accept run found at {boot_t}"}); save_meta(date, c, "stream_check", sc)
        if rep["not_done"] or sc.get("frames", 0) == 0:
            say(f"{c} refused or silent: the demo hangs after that; remaining configs not tried (they need a fresh boot)"); break
        r.cmd("sensorStop 0", 2.0); time.sleep(0.5)
    json.dump(allres, open(os.path.join(RES, f"{date}-accept.json"), "w"), indent=1)
    say("accept done"); return r

def cmd_empty(a):
    r = open_radar()
    for c in a.configs:
        board_status(f"empty-room window {a.seconds}s, config {c}")
        if not (a.keep and APPLIED): apply_config(r, c); APPLIED.append(c); sc = stream_check(r, 4)
        rec = Rec(r, c, a.date); r.flush()
        say(f"== empty {c} for {a.seconds}s")
        res = p_empty(rec, a.round, a.seconds); rec.close()
        # frame rate rows from the same window are in the CSV; summary:
        summarize_file(rec.path); say(f"   {res}")
        if not a.keep: r.cmd("sensorStop 0", 2.0)
    return r

def cmd_run(a):
    r = open_radar(); recs = {}
    img = "docs/arrivals/img/iwrl6432boost-eval-layout.png"
    for rnd in range(a.round_start, a.round_start + a.rounds):
        k = (rnd - 1) % len(a.configs); order = a.configs[k:] + a.configs[:k]
        for p in a.passes:
            for c in order:
                if not (a.keep and APPLIED): apply_config(r, c); APPLIED.append(c); stream_check(r, 2)
                rec = Rec(r, c + a.suffix, a.date); r.flush()
                if p == "walk": res, ok = p_walk(rec, rnd, a.ask_timeout, img)
                elif p.startswith("sit"): res, ok = p_sit(rec, rnd, float(p[3:]), p, a.ask_timeout, img)
                elif p == "empty":
                    ans, nid = announce(f"Please leave the room now and close the door. Stay out for about two minutes. I start when the door has been closed for ten seconds. I will say done when you can come back in.", img)
                    if ans != "done": res, ok = {"status": "no answer"}, False
                    else:
                        close_need(nid); speak('Leave the room now and close the door. Stay out for sixty seconds. I will call you back.', True); time.sleep(8); r.flush(); res, ok = p_empty(rec, rnd, a.seconds), True; speak("Done, thank you. You can come back in.")
                        dash("note", "radar: empty window done, come in now")
                else: continue
                rec.close(); summarize_file(rec.path); say(f"   {c} {p} round {rnd}: {res}")
                if not a.keep: r.cmd("sensorStop 0", 2.0)
                if not ok:
                    say("no answer: leaving the rest of the passes open"); return r
    return r

def cmd_restore(a):
    r = Radar(PORT)
    r.cmd("sensorStop 0", 2.0); time.sleep(1)
    res = r.apply([l.strip() for l in open(CFG) if l.strip() and not l.startswith("%") and not l.startswith("baudRate")], log=lambda s: None)
    bad = [(l, rep) for l, ok, rep in res if not ok and not l.startswith("sensorStop")]
    sc = stream_check(r, 6); say(f"restore: {len(res)} lines, not done {bad}; stream {sc}")
    return r

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cmd", choices=["accept", "empty", "run", "restore", "summarize"])
    ap.add_argument("--configs", default="1x1,2x2,2x3"); ap.add_argument("--passes", default="walk,sit2,sit4")
    ap.add_argument("--seconds", type=int, default=300); ap.add_argument("--rounds", type=int, default=1); ap.add_argument("--round-start", type=int, default=1)
    ap.add_argument("--round", type=int, default=1); ap.add_argument("--no-apply", action="store_true", help="the config is already running from an earlier command in this boot; do not send it again (implies --keep)"); ap.add_argument("--suffix", default="", help="added to the config name in result files, e.g. -both"); ap.add_argument("--forward", default="/dev/cu.usbmodemMG0000011", help="G3519 USB serial to forward whole radar frames and a W,<cls>,<p> line to; empty to turn off"); ap.add_argument("--keep", action="store_true", help="apply the config once, never stop/restart between passes (the demo can hang on a second start)"); ap.add_argument("--ask-timeout", type=int, default=900)
    ap.add_argument("--date", default=datetime.date.today().isoformat())
    a = ap.parse_args()
    FORWARD['port'] = a.forward
    if a.no_apply: a.keep = True; APPLIED.append('already-running')
    a.configs = [c for c in a.configs.split(",") if c]; a.passes = [p for p in a.passes.split(",") if p]
    if a.cmd == "summarize":
        for f in sorted(os.listdir(RES)):
            if f.endswith(".csv"): print(summarize_file(os.path.join(RES, f)))
        return
    r = {"accept": cmd_accept, "empty": cmd_empty, "run": cmd_run, "restore": cmd_restore}[a.cmd](a)
    r.close()

if __name__ == "__main__":
    main()
