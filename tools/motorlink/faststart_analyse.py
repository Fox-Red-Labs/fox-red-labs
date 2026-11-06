#!/usr/bin/env python3
"""Offline analysis of a faststart.py campaign (no board). Our code. Per run: the motor-state sequence, time to speed
(run flag write to the first sample inside 10 % of the reference that stays there 0.3 s), guard peak, stop time (the
stop or ramp write to the last Hall edge), the largest Is and bus after the stop write. Writes OUTDIR/analysis.json.

  python3 tools/motorlink/faststart_analyse.py OUTDIR
"""
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign_analyse as A

STATES = {0: "stop idle", 5: "seek position", 6: "alignment", 9: "open-loop start", 10: "open-loop end", 11: "closed loop running",
          12: "control run", 25: "Hall startup", 1: "fault stop", 3: "normal stop"}


def ev(meta, name):
    for t, n in meta.get("events", []):
        if n == name:
            return t
    return None


def analyse(out, tag):
    meta, s = A.load(out, tag)
    t0, ref = meta["t0"], meta["ref_hz"]
    seq, prev = [], None
    for t, v in s.get("motorState", []):
        v = int(v)
        if v != prev:
            seq.append([round(t - t0, 3), v, STATES.get(v, str(v))]); prev = v
    sp = s["speed_Hz"]
    t_stop = ev(meta, "ramp_write") or ev(meta, "stop_write")
    # single Hall-timed samples swing about 20 %: time to speed uses the 0.1 s trailing mean
    tts, j, acc = None, 0, 0.0
    sm = []
    for i, (t, v) in enumerate(sp):
        acc += v
        while sp[j][0] < t - 0.1: acc -= sp[j][1]; j += 1
        sm.append((t, acc / (i - j + 1)))
    for i, (t, v) in enumerate(sm):
        if t_stop and t >= t_stop - 0.3:
            break
        if abs(v - ref) <= 0.1 * abs(ref) and all(abs(w - ref) <= 0.1 * abs(ref) for tt, w in sm[i:] if tt <= t + 0.3):
            tts = round(t - t0, 3); break
    edges = []
    for k in A.EDGES:
        xs = s.get(k, [])
        edges += [t for (t, v), (_, w) in zip(xs[1:], xs[:-1]) if v != w]
    last_edge = max((t for t in edges if t_stop and t >= t_stop), default=None)
    after = lambda k: [v for t, v in s.get(k, []) if t_stop and t >= t_stop]
    pre_bus = [v for t, v in s.get("VdcBus_V", []) if t_stop and t_stop - 1.0 <= t < t_stop]
    return {
        "tag": tag, "states": seq, "time_to_speed_s": tts,
        "stop_time_s": round(last_edge - t_stop, 3) if last_edge else None,
        "is_max_after_stop_A": round(max(after("Is_A"), default=float("nan")), 3),
        "bus_rise_after_stop_V": round(max(after("VdcBus_V"), default=0) - (sum(pre_bus) / len(pre_bus) if pre_bus else 0), 3),
        "peak_A": round(meta["post"]["peak_A"], 3), "reason": meta["post"]["reason"], "fault": meta["post"]["faultMtrNow"],
        "samples_speed": len(sp), "log_end_after_stop_s": round(sp[-1][0] - t_stop, 2) if t_stop and sp else None,
    }


def main():
    out = pathlib.Path(sys.argv[1])
    tags = sorted(p.name[:-10] for p in out.glob("*.meta.json"))
    res = [analyse(out, t) for t in tags]
    (out / "analysis.json").write_text(json.dumps(res, indent=1))
    for r in res:
        print(r["tag"], "to speed", r["time_to_speed_s"], "stop", r["stop_time_s"], "peak", r["peak_A"], "Is after stop", r["is_max_after_stop_A"],
              "bus rise", r["bus_rise_after_stop_V"], "reason", r["reason"], "fault", r["fault"])
        print("   states", " > ".join(f"{t}s {n}" for t, _, n in r["states"]))


if __name__ == "__main__":
    main()
