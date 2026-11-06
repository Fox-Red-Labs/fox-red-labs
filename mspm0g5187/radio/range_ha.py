#!/usr/bin/env python3
"""Live range test into Home Assistant: the CC1407P on the LP-MSPM0G5187 (radio_lab.c) pings the
LP-CC1354P10-1 (partner.c in echo mode) in rounds, and the packet error rate (PER), the signal at
each end and the round trip go to Home Assistant as five sensors, so a walk with the sender shows
where the link starts to fail.

    range_ha.py [--minutes 10] [--round 10] [--window 100] [--profile 13] [--mask -1]

Home Assistant address and token as for cc1354p10/radio-rx/ha_bridge.py (HA_URL and HA_TOKEN, or
this bench's tools/ha.py). Ports as for lab.py (LAB_G, LAB_P). Each round is one "G" command of
--round pings; the PER is over the last --window pings.
"""
import argparse, os, re, sys, time, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "cc1354p10", "radio-rx"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import lab  # noqa: E402
import ha_bridge  # noqa: E402

ENT = {
    "per": ("sensor.frl_range_packet_error_rate", "Fox Red Labs range packet error rate", "%", None),
    "here": ("sensor.frl_range_signal_at_cc1407p", "Fox Red Labs range signal at the CC1407P", "dBm", "signal_strength"),
    "there": ("sensor.frl_range_signal_at_cc1354p10", "Fox Red Labs range signal at the CC1354P10", "dBm", "signal_strength"),
    "rtt": ("sensor.frl_range_round_trip", "Fox Red Labs range round trip", "ms", "duration"),
    "pings": ("sensor.frl_range_pings", "Fox Red Labs range pings in window", None, None),
}
DONE = re.compile(r"DONE G,(\d+),(\d+),rtt_us_avg=(\d+),here_avg=(-?\d+),here_min=(-?\d+),here_max=(-?\d+),there_avg=(-?\d+)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=10); ap.add_argument("--round", type=int, default=10)
    ap.add_argument("--window", type=int, default=100); ap.add_argument("--profile", type=int, default=13)
    ap.add_argument("--mask", default="-1"); ap.add_argument("--log", default="range_ha.log")
    ap.add_argument("--url", default=os.environ.get("HA_URL")); ap.add_argument("--token", default=os.environ.get("HA_TOKEN"))
    a = ap.parse_args()
    if a.url and a.token:
        hac = ha_bridge._Direct(a.url, a.token)
    else:
        import ha as hac  # tools/ha.py
    log = open(a.log, "a")
    g, p = lab.Port("G", lab.G, log), lab.Port("P", lab.P, log)
    for t in g.run(f"P {a.profile} {a.mask}"):
        print("G", t)
    for t in p.run("E", 10):
        print("P", t)
    window = collections.deque(maxlen=a.window)
    end = time.time() + a.minutes * 60
    while time.time() < end:
        out = g.run(f"G {a.round} 30 400 50", 60)
        p.lines(0.1)
        m = next((DONE.search(t) for t in out if DONE.search(t)), None)
        if not m:
            print("no result:", out[-1:]); continue
        got, n = int(m.group(1)), int(m.group(2))
        window.extend([1] * got + [0] * (n - got))
        per = round(100 * (1 - sum(window) / len(window)), 1)
        vals = {"per": per, "pings": len(window)}
        if got:
            vals.update(here=int(m.group(4)), there=int(m.group(7)), rtt=round(int(m.group(3)) / 1000, 1))
        ms_total = 0
        for k, v in vals.items():
            eid, name, unit, dc = ENT[k]
            attrs = {"friendly_name": name, "state_class": "measurement"}
            if unit: attrs["unit_of_measurement"] = unit
            if dc: attrs["device_class"] = dc
            ms, _ = hac.req("POST", "/api/states/" + eid, {"state": v, "attributes": attrs})
            ms_total += ms
        print(f"{time.strftime('%H:%M:%S')} round {got}/{n} per={per}% over {len(window)} here={vals.get('here')} there={vals.get('there')} rtt={vals.get('rtt')} ms ({ms_total} ms to post)", flush=True)


if __name__ == "__main__":
    main()
