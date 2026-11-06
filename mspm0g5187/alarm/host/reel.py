#!/usr/bin/env python3
"""Play a decoy reel from the Mac's speaker at the board, and record when each item played.

  reel.py build OUTDIR            write the synthetic items (44.1 kHz WAVs) and reel.json
  reel.py play OUTDIR [--only a,b] [--volume 50]   play every item in order; writes played.jsonl
  reel.py score OUTDIR LISTEN_LOG    match the board's ALARM / IGNORED lines to what was playing

Items: held-out sounds from the bank's test split (ringtones, alert tones, speech voices, music) at
their original quality, plus synthetic look-alikes: continuous even beeping (oven timer, backup
beeper), microwave beeps, an alarm clock's beep groups, temporal-3 and temporal-4 with the timing
off by 35 % or more, the CO pattern with three or five pulses, a low-battery chirp, a whistled
tune, a kettle. Then the two hard cases: a real temporal-3 and temporal-4 pattern from the Mac's
speaker (an alarm on the TV). Expected: no ALARM during any item; the TV items give IGNORED once
an alarm has been learned (too quiet, or another tone), ALARM in generic mode.
"""
import argparse
import json
import os
import subprocess
import sys
import time
import wave

import numpy as np

SR = 44100
BANK = os.path.abspath("out/alarm/bank")


def tone(f, d, harm=(), am=0.0):
    t = np.arange(int(d * SR)) / SR
    y = np.sin(2 * np.pi * f * t)
    for h, g in harm:
        y += 10 ** (g / 20) * np.sin(2 * np.pi * f * h * t)
    if am:
        y *= 1 + am * np.sin(2 * np.pi * 7 * t)
    n = len(y)
    e = np.ones(n)
    k = min(int(0.005 * SR), n // 2)
    if k:
        e[:k] = np.linspace(0, 1, k)
        e[-k:] = np.linspace(1, 0, k)
    return y * e / (np.abs(y).max() + 1e-9)


def pattern(f, on, off, gap, k, total, harm=((2, -20),)):
    out = np.zeros(int(total * SR))
    t = 0.0
    while t < total:
        for i in range(k):
            s = t + i * (on + off)
            if s + on > total:
                break
            b = tone(f, on, harm)
            out[int(s * SR):int(s * SR) + len(b)] += b
        t += k * on + (k - 1) * off + gap
    return out


def whistle(total, rng):
    out, t = [], 0.0
    while t < total:
        d = rng.uniform(0.15, 0.7)
        n = int(d * SR)
        f = rng.choice([880, 988, 1175, 1319, 1568, 1760, 2093])
        fr = f * (1 + 0.02 * np.sin(2 * np.pi * 6 * np.arange(n) / SR))
        y = np.sin(2 * np.pi * np.cumsum(fr) / SR) * np.hanning(n)
        out.append(y)
        out.append(np.zeros(int(rng.uniform(0.03, 0.3) * SR)))
        t += d
    return np.concatenate(out)


def wav(path, y, gain=0.5):
    y = np.clip(y / (np.abs(y).max() + 1e-9) * gain, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((y * 32767).astype(np.int16).tobytes())


def build(out):
    os.makedirs(out, exist_ok=True)
    rng = np.random.default_rng(7)
    synth = {
        "oven_timer_0.5on_0.5off_3k": pattern(3100, 0.5, 0.5, 0.5, 1, 20),
        "backup_beeper_1k": pattern(1100, 0.5, 0.5, 0.5, 1, 15),
        "microwave_3_beeps": np.concatenate([pattern(2400, 0.6, 0.4, 30, 3, 3.0), np.zeros(SR * 5)]),
        "microwave_5_beeps_x3": np.concatenate([np.concatenate([pattern(3000, 0.3, 0.3, 30, 5, 3.0), np.zeros(SR * 6)]) for _ in range(3)]),
        "alarm_clock_4_beeps_1s": pattern(2800, 0.07, 0.07, 0.5, 4, 20),
        "t3_too_fast": pattern(3200, 0.25, 0.25, 0.75, 3, 20),
        "t3_too_slow": pattern(3200, 1.0, 1.0, 3.0, 3, 25),
        "t3_short_gap": pattern(3200, 0.5, 0.5, 0.5, 3, 20),
        "t4_three_pulses": pattern(3200, 0.1, 0.1, 5.0, 3, 25),
        "t4_five_pulses": pattern(3200, 0.1, 0.1, 5.0, 5, 25),
        "t4_short_gap": pattern(3200, 0.1, 0.1, 1.5, 4, 20),
        "low_battery_chirp": np.concatenate([np.concatenate([tone(3300, 0.05), np.zeros(int(SR * 8))]) for _ in range(3)]),
        "whistled_tune": whistle(20, rng),
        "kettle": tone(2600, 15, ((2, -25),), am=0.1) * np.linspace(0.2, 1, 15 * SR),
        "tv_smoke_alarm_t3": pattern(3150, 0.5, 0.5, 1.5, 3, 30),
        "tv_co_alarm_t4": pattern(3150, 0.1, 0.1, 5.0, 4, 30),
    }
    items = []
    for k, y in synth.items():
        p = os.path.join(out, k + ".wav")
        wav(p, y)
        items.append(dict(name=k, path=p, seconds=round(len(y) / SR, 1), kind="tv_alarm" if k.startswith("tv_") else "lookalike"))
    man = json.load(open(os.path.join(BANK, "manifest.json")))
    test = [m for m in man if m["split"] == "test"]
    for m in [m for m in test if m["kind"] == "tone"][:14]:
        items.append(dict(name=m["name"], path=m["src"], seconds=min(m["seconds"], 20), kind="bank_tone"))
    for m in [m for m in test if m["kind"] == "speech"][:3]:
        items.append(dict(name=m["name"], say=m["src"].split()[-1], seconds=20, kind="speech"))
    for m in [m for m in man if m["kind"] == "music"]:
        items.append(dict(name=m["name"], path=m["src"], seconds=45, kind="music"))
    order = [i for i in items if i["kind"] != "tv_alarm"] + [i for i in items if i["kind"] == "tv_alarm"]
    json.dump(order, open(os.path.join(out, "reel.json"), "w"), indent=1)
    print(f"{len(order)} items, {sum(i['seconds'] for i in order)/60:.1f} minutes")


def play(out, only, volume):
    items = json.load(open(os.path.join(out, "reel.json")))
    if only:
        items = [i for i in items if any(o in i["name"] for o in only.split(","))]
    if volume is not None:
        subprocess.run(["osascript", "-e", f"set volume output volume {volume}"])
    log = open(os.path.join(out, "played.jsonl"), "a")
    text = "Good evening and welcome to the news. Our top story tonight is the storm moving up the coast. " * 3
    for it in items:
        t0 = time.time()
        if "say" in it:
            subprocess.run(["say", "-v", it["say"], text], timeout=it["seconds"] + 30)
        else:
            subprocess.run(["afplay", "-t", str(it["seconds"]), it["path"]])
        t1 = time.time()
        rec = dict(name=it["name"], kind=it["kind"], start=t0, end=t1)
        log.write(json.dumps(rec) + "\n")
        log.flush()
        print(f"{time.strftime('%H:%M:%S', time.localtime(t0))} {it['kind']:10s} {it['name']} {t1-t0:.1f} s", flush=True)
        time.sleep(9)   # let the 8.26 s window clear before the next item


def score(out, listen_log):
    played = [json.loads(l) for l in open(os.path.join(out, "played.jsonl"))]
    ev = []
    for ln in open(listen_log):
        parts = ln.split(" ", 2)
        if len(parts) < 3 or not parts[2].startswith(("ALARM", "IGNORED")):
            continue
        t = time.mktime(time.strptime(parts[0] + " " + parts[1], "%Y-%m-%d %H:%M:%S"))
        ev.append((t, parts[2].strip()))
    rows = []
    for p in played:
        hits = [e for t, e in ev if p["start"] <= t <= p["end"] + 9]
        rows.append(dict(name=p["name"], kind=p["kind"], alarms=sum(h.startswith("ALARM") for h in hits),
                         ignored=sum(h.startswith("IGNORED") for h in hits), lines=hits[:2]))
    for r in rows:
        print(f"{r['kind']:10s} {r['name'][:40]:40s} ALARM {r['alarms']}  IGNORED {r['ignored']}  {r['lines'][:1]}")
    fa = sum(r["alarms"] > 0 for r in rows if r["kind"] != "tv_alarm")
    print(f"false trips: {fa} of {sum(r['kind'] != 'tv_alarm' for r in rows)} decoy items")
    json.dump(rows, open(os.path.join(out, "score.json"), "w"), indent=1)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["build", "play", "score"])
    ap.add_argument("outdir")
    ap.add_argument("log", nargs="?")
    ap.add_argument("--only")
    ap.add_argument("--volume", type=int)
    a = ap.parse_args()
    if a.cmd == "build":
        build(a.outdir)
    elif a.cmd == "play":
        play(a.outdir, a.only, a.volume)
    else:
        score(a.outdir, a.log)
