#!/usr/bin/env python3
"""Radar, PIR and Wi-Fi presence side by side on one spoken script, scored per phase.

  compare_sensors.py [--radar http://127.0.0.1:8765/occupancy] [--pir /dev/cu.usbmodemXXXX1]
                     [--wifi-url URL | --wifi-file PATH] [--out DIR] [--short] [--quiet]

Each second it records what each sensor says (present or not) next to the truth from the script:
  leave (empty), come in and sit still, walk around, sit still at the far end, leave (empty).
Sources:
  radar  the room server's /occupancy JSON: "occupied" (people tracked plus people held still)
  pir    a serial port printing lines "PIR 0" / "PIR 1" (iwrl6432boost or mspm0g3519 PIR firmware)
  wifi   --wifi-url: JSON with "present" (true/false) or "class" ("present"/"empty") or "p" (probability, >= 0.5);
         --wifi-file: a CSV or log whose last line holds one of those words; the newest line counts
A sensor left out is skipped. Scores: share of seconds right in each phase (the first 10 s after a cue are not
scored, people are still moving), seconds from a cue to the first right answer (detect, clear). Writes
compare-<time>.csv and .json in --out (default the log folder of the day, else the current folder).
"""
import argparse, csv, json, os, re, subprocess, threading, time, urllib.request

SCRIPT = [  # (truth, name, seconds, spoken cue)
    (0, "empty", 180, "Comparison test. Please leave the room, or step out of view, and stay away until you hear the next step."),
    (1, "sit_still", 180, "Come in, sit down where you usually sit, and stay still. Breathe normally."),
    (1, "walking", 120, "Stand up and walk around the room at a normal pace."),
    (1, "still_far", 120, "Now sit or stand still at the far end of the room."),
    (0, "empty_again", 180, "Please leave the room again, and stay away until you hear the test is done."),
]
SETTLE = 10


def say(msg, quiet):
    print("SAY:", msg, flush=True)
    if not quiet:
        subprocess.run(["say", msg])


class Pir:
    def __init__(self, port):
        import serial
        self.s, self.state, self.seen = serial.Serial(port, 115200, timeout=0.2), None, 0
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        buf = b""
        while True:
            buf += self.s.read(256)
            *lines, buf = buf.split(b"\n")
            for l in lines:
                m = re.search(rb"PIR\s+([01])", l)
                if m:
                    self.state, self.seen = m.group(1) == b"1", time.time()

    def read(self):
        return self.state if time.time() - self.seen < 3 else None


def wifi_read(url=None, path=None):
    try:
        if url:
            j = json.loads(urllib.request.urlopen(url, timeout=1).read())
            if "ti_last" in j:  # cc35x1 presence service: the shipped model's class, every 2 s
                tl = j["ti_last"]
                return None if not tl or time.time() - tl["t"] > 6 else bool(tl["cls"])
            if "present" in j: return bool(j["present"])
            if "class" in j: return str(j["class"]).lower().startswith("pres")
            if "p" in j: return float(j["p"]) >= 0.5
        if path:
            with open(path, "rb") as f:
                f.seek(max(0, os.path.getsize(path) - 400))
                last = f.read().decode(errors="replace").strip().splitlines()[-1].lower()
            if "present" in last or "presence" in last: return True
            if "empty" in last or "absent" in last: return False
    except Exception:
        return None
    return None


def radar_read(url):
    try:
        j = json.loads(urllib.request.urlopen(url, timeout=1).read())
        return None if j.get("link") == "down" else bool(j.get("occupied"))
    except Exception:
        return None


def score(rows, names):
    out = {}
    for n in names:
        ph = {}
        for name in [s[1] for s in SCRIPT]:
            r = [x for x in rows if x["phase"] == name and x["since_cue"] >= SETTLE and x[n] is not None]
            right = sum(1 for x in r if x[n] == bool(x["truth"]))
            first = next((x["since_cue"] for x in rows if x["phase"] == name and x[n] is not None
                          and x[n] == bool(x["truth"])), None)
            ph[name] = {"right_pct": round(100 * right / len(r), 1) if r else None, "seconds_scored": len(r),
                        "first_right_s": first}
        allr = [x for x in rows if x["since_cue"] >= SETTLE and x[n] is not None]
        out[n] = {"overall_right_pct": round(100 * sum(1 for x in allr if x[n] == bool(x["truth"])) / len(allr), 1)
                  if allr else None, "phases": ph}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--radar", default="http://127.0.0.1:8765/occupancy")
    ap.add_argument("--pir"); ap.add_argument("--wifi-url"); ap.add_argument("--wifi-file")
    ap.add_argument("--out", default=os.path.expanduser(f"<scratch>/runs/{time.strftime('%Y-%m-%d')}-radar-logs"))
    ap.add_argument("--short", action="store_true", help="20 s phases, to check the wiring")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    pir = Pir(a.pir) if a.pir else None
    names = ["radar"] + (["pir"] if pir else []) + (["wifi"] if (a.wifi_url or a.wifi_file) else [])
    os.makedirs(a.out, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M")
    rows = []
    for truth, name, secs, cue in SCRIPT:
        secs = 20 if a.short else secs
        say(cue, a.quiet)
        t0 = time.time()
        while time.time() - t0 < secs:
            t = time.time()
            row = {"t": round(t, 1), "phase": name, "truth": truth, "since_cue": round(t - t0),
                   "radar": radar_read(a.radar)}
            if pir: row["pir"] = pir.read()
            if "wifi" in names: row["wifi"] = wifi_read(a.wifi_url, a.wifi_file)
            rows.append(row)
            print(f"{name:11s} {row['since_cue']:4d}s truth {truth} " +
                  " ".join(f"{n} {'-' if row[n] is None else int(row[n])}" for n in names), flush=True)
            time.sleep(max(0, 1 - (time.time() - t)))
    say("Test done. Thank you.", a.quiet)
    res = {"date": time.strftime("%Y-%m-%d %H:%M"), "sensors": names, "short": a.short, "scores": score(rows, names)}
    with open(os.path.join(a.out, f"compare-{stamp}.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    json.dump(res, open(os.path.join(a.out, f"compare-{stamp}.json"), "w"), indent=1)
    for n in names:
        s = res["scores"][n]
        print(f"{n}: {s['overall_right_pct']}% right; " +
              ", ".join(f"{k} {v['right_pct']}% (first right {v['first_right_s']} s)" for k, v in s["phases"].items()))
    print("WROTE", os.path.join(a.out, f"compare-{stamp}.json"))


if __name__ == "__main__":
    main()
