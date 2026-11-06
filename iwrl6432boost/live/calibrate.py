#!/usr/bin/env python3
"""Spoken self-calibration for the IWRL6432BOOST people tracker: one person, about two minutes.

  calibrate.py [--port /dev/cu.usbmodemRI321] [--probe RI32] [--base cfg/people-count-250ms.cfg] [--quiet]

Steps, each spoken with the Mac's `say`:
  1 empty room 30 s   (the person steps behind the radar)  -> interference cells: points with nobody in view
  2 walk the walls 45 s                                    -> room outline from the track's path
  3 stand in the doorway 6 s                               -> entrance position
  4 sit in the usual seat 10 s                             -> seat position
Then writes cfg/calibrated.cfg (boundary, static and presence boxes from the room; static box kept off the door)
and room.json (outline, door, seat, interference cells), which radar_live.py draws. Needs pyserial.
"""
import argparse, json, math, os, subprocess, sys, time
import serial
import radar_live as rl

HERE = os.path.dirname(os.path.abspath(__file__))
CELL = 0.5


def say(msg, quiet=False):
    print("SAY:", msg, flush=True)
    if not quiet:
        subprocess.run(["say", msg])


def collect(s, buf, secs):
    """Read frames for secs seconds. Returns (frames, buf)."""
    out, t0 = [], time.time()
    while time.time() - t0 < secs:
        fr, buf = rl.split_frames(buf + s.read(4096))
        for f in fr:
            d = rl.decode(f)
            d["t"] = time.time()
            out.append(d)
    return out, buf


def is_still(f):
    return bool(f["tracks"]) and all((t["vx"] ** 2 + t["vy"] ** 2) ** 0.5 < 0.25 for t in f["tracks"])


def wait_for(s, buf, cond, need, timeout, reprompt, quiet):
    """Read until cond holds for `need` frames in a row. Repeat the prompt every 15 s. Returns (ok, buf)."""
    t0, run, nxt = time.time(), 0, time.time() + 15
    while time.time() - t0 < timeout:
        fr, buf = collect(s, buf, 0.3)
        for f in fr:
            run = run + 1 if cond(f) else 0
            if run >= need:
                return True, buf
        if time.time() > nxt:
            say(reprompt, quiet); nxt = time.time() + 15
    return False, buf


def moving(f):
    return [(p[0], p[1]) for p in f["points"] if abs(p[3]) > 0.1]


def learn_door(s, buf, quiet, wait=60):
    """Person walks out and back in. Door = mean of the last moving points before they vanish
    and the first after they return. Returns (door or None, frames, buf)."""
    rec, t0, last_seen, gone, back = [], time.time(), time.time(), None, None
    while time.time() - t0 < 60:
        fr, buf = collect(s, buf, 0.3)
        for f in fr:
            rec.append(f)
            if moving(f):
                last_seen = f["t"]
                if gone and back is None:
                    back = f["t"]
            elif gone is None and f["t"] - last_seen > 3 and len(rec) > 8:
                gone = last_seen
                say("Good. Now come back in.", quiet)
        if back and time.time() - back > 2:
            break
        if gone is None and time.time() - t0 > wait:
            break
    if not gone or not back:
        return None, rec, buf
    out_pts = [p for f in rec if gone - 1.5 <= f["t"] <= gone for p in moving(f)]
    in_pts = [p for f in rec if back <= f["t"] <= back + 1.5 for p in moving(f)]
    both = out_pts + in_pts
    if not both:
        return None, rec, buf
    return [round(sum(p[0] for p in both) / len(both), 2), round(sum(p[1] for p in both) / len(both), 2)], rec, buf

    """Convex hull, monotone chain."""
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts
    def cross(o, a, b): return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]


def hull(pts):
    """Convex hull, monotone chain."""
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts
    def cross(o, a, b): return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]


def still_spot(frames):
    """Mean position of the track seen most in the window."""
    pos = {}
    for f in frames:
        for t in f["tracks"]:
            pos.setdefault(t["id"], []).append((t["x"], t["y"]))
    if not pos:
        return None
    p = max(pos.values(), key=len)
    return [round(sum(a for a, _ in p) / len(p), 2), round(sum(b for _, b in p) / len(p), 2)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodemRI321")
    ap.add_argument("--probe", default="RI32")
    ap.add_argument("--base", default=os.path.join(HERE, "cfg", "people-count-250ms.cfg"))
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--mount", choices=["desk", "wall", "ceiling"], default="desk",
                    help="where the radar is: desk (flat, about 1 m), wall (2 m, tilted 15 degrees down), ceiling (2.7 m, facing down)")
    ap.add_argument("--height", type=float, help="radar height above the floor in metres (measured)")
    ap.add_argument("--tilt", type=float, help="down tilt in degrees (90 = facing the floor)")
    ap.add_argument("--from", dest="src", help="rebuild the room from a saved runs/*-calibrate.json, no walking")
    a = ap.parse_args()
    q = a.quiet
    if a.base == os.path.join(HERE, "cfg", "people-count-250ms.cfg") and a.mount != "desk":
        a.base = os.path.join(HERE, "cfg", {"wall": "wall-2m-tilt15.cfg", "ceiling": "ceiling-2.7m.cfg"}[a.mount])
    if a.height is not None or a.tilt is not None:  # write the measured pose into a copy of the base config
        lines = open(a.base).read().split("\n")
        cur = next(l for l in lines if l.startswith("sensorPosition")).split()
        h = a.height if a.height is not None else float(cur[3]); t = a.tilt if a.tilt is not None else float(cur[5])
        posed = os.path.join(HERE, "cfg", f"posed-{a.mount}.cfg")
        open(posed, "w").write("\n".join(f"sensorPosition 0 0 {h} 0 {t}" if l.startswith("sensorPosition") else l for l in lines))
        a.base = posed

    if a.src:
        steps = json.load(open(a.src))
        empty, walk, door, seat = steps["empty"], steps["walk"], steps["door"], steps["seat"]
        steps.setdefault("doors", [])
    else:
        say("Radar setup. This takes about two minutes. Starting the radar.", q)
        bad = rl.configure(a.port, a.base, a.probe)
        if bad:
            print("CONFIG_ERRORS", bad)
            return 1
        s = serial.Serial(a.port, 115200, timeout=0.05)
        buf = b""
        _, buf = collect(s, buf, 2)

        say("Step one. Please step behind the radar, out of its view, and stay there until you hear the next step.", q)
        ok, buf = wait_for(s, buf, lambda f: not f["tracks"], 8, 45,
                           "I can still see someone. Please step behind the radar or out of the room.", q)
        if not ok:
            say("The room never looked empty, so I will skip the empty room step.", q)
        empty, buf = collect(s, buf, 30) if ok else ([], buf)

        say("Step two. Walk slowly around the edge of the room in front of the radar, into every corner you can reach, then once across the middle.", q)
        ok, buf = wait_for(s, buf, lambda f: bool(f["tracks"]) or len(moving(f)) >= 3, 2, 40,
                           "I cannot see you yet. Walk into the room in front of the radar.", q)
        walk, buf = collect(s, buf, 45) if ok else ([], buf)

        say("Step three. Walk out through the door you use most, and wait outside until I ask you to come back.", q)
        door1, door, buf = learn_door(s, buf, q)
        say("Step four. If there is a second door, walk out through it the same way. If not, stay where you are.", q)
        door2, door_b, buf = learn_door(s, buf, q, wait=20)
        door += door_b
        say("Last step. Sit down where people usually sit, and stay still.", q)
        ok, buf = wait_for(s, buf, is_still, 6, 40, "Please sit down and stay still.", q)
        seat, buf = collect(s, buf, 10)
        say("Thank you. Working out the room.", q)
        s.close()
        steps = {"empty": empty, "walk": walk, "door": door, "seat": seat, "doors": [d for d in (door1, door2) if d]}
        os.makedirs(os.path.join(HERE, "runs"), exist_ok=True)
        json.dump(steps, open(os.path.join(HERE, "runs", time.strftime("%Y-%m-%d-%H%M") + "-calibrate.json"), "w"))
    for k in ("empty", "walk", "door", "seat"):
        v = steps[k]
        print(f"STEP {k}: frames {len(v)}, with points {sum(1 for f in v if f['points'])}, "
              f"with tracks {sum(1 for f in v if f['tracks'])}", flush=True)
    print("DOORS", steps["doors"], flush=True)

    # 1 interference: cells that light up with nobody in view
    cells = {}
    for f in empty:
        for p in f["points"]:
            k = (math.floor(p[0] / CELL), math.floor(p[1] / CELL))
            cells[k] = cells.get(k, 0) + 1
    n_empty = max(1, len(empty))
    interference = [{"x": (k[0] + 0.5) * CELL, "y": (k[1] + 0.5) * CELL, "per_frame": round(v / n_empty, 2)}
                    for k, v in cells.items() if v / n_empty >= 0.05]
    empty_tracks = sum(1 for f in empty if f["tracks"])

    # 2 room outline from the walked path
    path = [(t["x"], t["y"]) for f in walk for t in f["tracks"]]
    if len(path) < 10:  # the tracker dropped the walker: use the moving points instead
        path = [p for f in walk for p in moving(f)]
    if len(path) < 10:
        say("I could not follow you along the walls. Please run the setup again.", q)
        print("CAL_FAIL walk points", len(path))
        return 2
    doors = steps["doors"] or ([still_spot(door)] if still_spot(door) else [])
    door_xy, seat_xy = (doors[0] if doors else None), still_spot(seat)
    mp = sorted(p for f in walk for p in moving(f))
    if len(mp) > 20:  # moving points cover what the tracker dropped; trim 2 percent at each end for noise
        k = max(1, len(mp) // 50); xs_m = sorted(p[0] for p in mp)[k:-k]; ys_m = sorted(p[1] for p in mp)[k:-k]
        path_ext = path + [(xs_m[0], ys_m[0]), (xs_m[-1], ys_m[-1])]
    else:
        path_ext = path
    pts = path_ext + [tuple(p) for p in doors + ([seat_xy] if seat_xy else [])]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    xmin, xmax, ymax = min(xs) - 0.4, max(xs) + 0.4, max(ys) + 0.4
    outline = hull([(round(x, 2), round(y, 2)) for x, y in path + [p for p in mp[::3]] + [tuple(p) for p in doors]])

    # static box: the room inset 0.3 m. Trim the far end only when the door is within 1 m of it,
    # so a person who leaves there does not linger; never trim past the seat.
    sx0, sx1, sy0, sy1 = xmin + 0.3, xmax - 0.3, 0.5, ymax - 0.3
    for dxy in doors:  # keep a strip at each door free, so a person who leaves there does not linger
        if ymax - dxy[1] < 1.2:
            sy1 = min(sy1, dxy[1] - 0.8)
        elif dxy[0] > sx1 - 1.0:
            sx1 = min(sx1, dxy[0] - 0.8)
        elif dxy[0] < sx0 + 1.0:
            sx0 = max(sx0, dxy[0] + 0.8)
    if seat_xy:
        sx0, sx1 = min(sx0, seat_xy[0] - 0.5), max(sx1, seat_xy[0] + 0.5)
        sy0, sy1 = min(sy0, max(0.3, seat_xy[1] - 0.5)), max(sy1, seat_xy[1] + 0.5)
    f2 = lambda v: f"{v:.1f}"
    lines = {
        "boundaryBox": f"boundaryBox {f2(xmin)} {f2(xmax)} 0 {f2(ymax)} -0.5 3",
        "staticBoundaryBox": f"staticBoundaryBox {f2(sx0)} {f2(sx1)} {f2(sy0)} {f2(sy1)} 0 3",
        "presenceBoundaryBox": f"presenceBoundaryBox {f2(xmin)} {f2(xmax)} 0.3 {f2(ymax)} 0 3",
    }
    out = []
    for raw in open(a.base):
        key = raw.split()[0] if raw.strip() else ""
        out.append(lines[key] + "\n" if key in lines else raw)
    hdr = (f"% Written by calibrate.py {time.strftime('%Y-%m-%d %H:%M')} from {os.path.basename(a.base)}: "
           f"room {f2(xmax - xmin)} m wide, {f2(ymax)} m deep; door {door_xy}; seat {seat_xy}\n")
    cfg_out = os.path.join(HERE, "cfg", "calibrated.cfg")
    open(cfg_out, "w").write(hdr + "".join(out))
    room = {"date": time.strftime("%Y-%m-%d %H:%M"), "epoch": round(time.time()), "mount": a.mount, "outline": outline, "door": door_xy, "doors": doors, "seat": seat_xy,
            "interference": interference, "boxes": lines, "empty_frames": len(empty),
            "empty_frames_with_tracks": empty_tracks, "walk_frames": len(walk),
            "walk_frames_with_track": sum(1 for f in walk if f["tracks"])}
    json.dump(room, open(os.path.join(HERE, "room.json"), "w"), indent=1)
    for k, v in lines.items():
        print(v)
    print(f"CAL_OK room {f2(xmax - xmin)} x {f2(ymax)} m, door {door_xy}, seat {seat_xy}, "
          f"interference cells {len(interference)}, empty frames with a track {empty_tracks} of {len(empty)}")

    cover = sum(1 for f in walk if f["tracks"]) / max(1, len(walk))
    if cover < 0.6:
        say(f"I saw you for {round(cover * 100)} percent of the walk, so parts of the room may be missing from the map.", q)
    say("Restarting the radar with your room.", q)
    bad = rl.configure(a.port, cfg_out, None)
    say("Setup done." if not bad else "Setup finished, but the radar refused the new settings.", q)
    return 0 if not bad else 3


if __name__ == "__main__":
    sys.exit(main())
