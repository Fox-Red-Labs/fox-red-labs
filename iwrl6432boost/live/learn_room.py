#!/usr/bin/env python3
"""Learn a room from what the radar saw, with no walk: where people go, where they come and go, where they sit,
and what moves when nobody is there.

  learn_room.py <log folder or files...> [--out room-learned.json] [--cfg cfg/learned.cfg] [--base cfg/people-count-250ms.cfg]

Inputs: the all-day log from `radar_live.py --log DIR` (frames-*.jsonl), or raw recordings (*.bin, 4 frames a second).
Output (same fields radar_live's page draws, plus a heat grid):
  heat         0.25 m cells with how many track-frames fell in each (walkable area; furniture shows as holes)
  outline      hull of the cells people used, with the faintest 2 percent dropped
  doors        clusters where moving tracks begin and end, each with its start and end counts
  seats        clusters where a track stayed still 20 s or more, with total seconds
  interference cells with moving points while the room had no track for 60 s or more
  compare      the walked room.json next to this one, when one exists
With --cfg it also writes a config with the boundary boxes from the learned area. Nothing is sent to the radar.
"""
import glob, json, math, os, sys

import radar_live as rl

HERE = os.path.dirname(os.path.abspath(__file__))
CELL = 0.25


def load(paths):
    """Frames as dicts {t, tr:[[id,x,y,z,vx,vy]], p:[[x,y,d]]}, time-ordered, per source."""
    out = []
    for p in paths:
        if p.endswith(".jsonl"):
            out.append([json.loads(l) for l in open(p) if l.strip()])
        elif p.endswith(".bin"):
            fr, _ = rl.split_frames(open(p, "rb").read())
            seq, f0 = [], None
            for f in fr:
                d = rl.decode(f)
                f0 = d["frame"] if f0 is None else f0
                seq.append({"t": (d["frame"] - f0) / 4.0, "f": d["frame"],
                            "tr": [[t["id"], t["x"], t["y"], t["z"], t["vx"], t["vy"]] for t in d["tracks"]],
                            "p": [[q[0], q[1], q[3]] for q in d["points"] if abs(q[3]) > 0.1]})
            out.append(seq)
    return out


def cluster(pts, r):
    """Greedy clusters within r metres. pts: [(x, y, tag)]. Returns [{x, y, n, tags}] largest first."""
    cl = []
    for x, y, tag in pts:
        for c in cl:
            if math.hypot(c["x"] - x, c["y"] - y) < r:
                c["n"] += 1; c["sx"] += x; c["sy"] += y; c["tags"][tag] = c["tags"].get(tag, 0) + 1
                c["x"], c["y"] = c["sx"] / c["n"], c["sy"] / c["n"]
                break
        else:
            cl.append({"x": x, "y": y, "sx": x, "sy": y, "n": 1, "tags": {tag: 1}})
    return sorted(({"x": round(c["x"], 2), "y": round(c["y"], 2), "n": c["n"], "tags": c["tags"]} for c in cl),
                  key=lambda c: -c["n"])


def hull(pts):
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts
    cr = lambda o, a, b: (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cr(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cr(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]


def learn(sources):
    heat, ends, stills, quiet = {}, [], [], {}
    frames = 0
    for seq in sources:
        life = {}  # id -> {"first", "last", "t0", "t1", "still_since"}
        empty_since = None
        recent, gone_at, gone_xy, last_pts_t = [], None, None, None
        for f in seq:
            # doors: the room goes fully empty (no moving points, no track) for 3 s after someone was there;
            # the exit is where the last moving points were, the entry where they come back
            if f["p"]:
                recent = [(t0, q) for t0, q in recent if f["t"] - t0 <= 1.0] + [(f["t"], q) for q in f["p"]]
                if gone_at is not None:
                    first = [q for q in f["p"]]
                    ends.append((sum(q[0] for q in first) / len(first), sum(q[1] for q in first) / len(first), "start"))
                    gone_at = None
                last_pts_t = f["t"]
            elif not f["tr"] and last_pts_t is not None and gone_at is None and f["t"] - last_pts_t >= 3 and recent:
                gone_at = f["t"]
                ends.append((sum(q[1][0] for q in recent) / len(recent), sum(q[1][1] for q in recent) / len(recent), "end"))
                recent = []
            frames += 1
            t, ids = f["t"], set()
            for tid, x, y, z, vx, vy in f["tr"]:
                ids.add(tid)
                k = (math.floor(x / CELL), math.floor(y / CELL))
                heat[k] = heat.get(k, 0) + 1
                sp = math.hypot(vx, vy)
                L = life.setdefault(tid, {"first": (x, y, sp), "t0": t, "still": None})
                L["last"], L["t1"] = (x, y, sp), t
                if sp < 0.15:
                    L["still"] = L["still"] or (t, x, y)
                else:
                    if L["still"] and t - L["still"][0] >= 20:
                        stills.append((L["still"][1], L["still"][2], round(t - L["still"][0])))
                    L["still"] = None
            for tid in [k for k in life if k not in ids]:
                L = life.pop(tid)
                if L["still"] and L["t1"] - L["still"][0] >= 20:
                    stills.append((L["still"][1], L["still"][2], round(L["t1"] - L["still"][0])))
            # quiet room: no track for 60 s, then count moving points per cell
            if f["tr"]:
                empty_since = None
            else:
                empty_since = empty_since if empty_since is not None else t
                if t - empty_since >= 60:
                    quiet.setdefault("frames", 0); quiet["frames"] += 1
                    for x, y, d in f["p"]:
                        k = (math.floor(x / 0.5), math.floor(y / 0.5))
                        quiet[k] = quiet.get(k, 0) + 1
        for L in life.values():
            if L["still"] and L["t1"] - L["still"][0] >= 20:
                stills.append((L["still"][1], L["still"][2], round(L["t1"] - L["still"][0])))

    if not heat:
        return None
    # walkable area: drop the faintest 2 percent of track-frames (stray ghosts)
    cells = sorted(heat.items(), key=lambda kv: kv[1])
    total, cut, acc = sum(heat.values()), 0, 0
    for k, v in cells:
        acc += v
        if acc > 0.02 * total:
            cut = v; break
    used = [k for k, v in heat.items() if v >= cut]
    corners = [((i + a) * CELL, (j + b) * CELL) for i, j in used for a in (0, 1) for b in (0, 1)]
    outline = [[round(x, 2), round(y, 2)] for x, y in hull(corners)]
    # doors: clusters of track starts and ends that have both kinds
    dc = [c for c in cluster(ends, 0.9) if c["tags"].get("start", 0) >= 1 and c["tags"].get("end", 0) >= 1
          and c["n"] >= 2]
    sc = cluster([(x, y, s) for x, y, s in stills], 0.8)
    seats = [{"x": c["x"], "y": c["y"], "seconds": sum(int(k) * v for k, v in c["tags"].items()), "visits": c["n"]}
             for c in sc]
    # a "door" on a seat is a seated person the tracker lost and found again, not a way in or out
    long_seats = [s for s in seats if s["seconds"] >= 60]
    lost_at_seat = [c for c in dc if any(math.hypot(c["x"] - s["x"], c["y"] - s["y"]) < 1.0 for s in long_seats)]
    dc = [c for c in dc if c not in lost_at_seat]
    qf = quiet.pop("frames", 0)
    interference = [{"x": (k[0] + 0.5) * 0.5, "y": (k[1] + 0.5) * 0.5, "per_frame": round(v / qf, 3)}
                    for k, v in quiet.items() if qf and v / qf >= 0.02]
    # a seated person the tracker dropped still moves a little: not clutter
    interference = [c for c in interference
                    if not any(math.hypot(c["x"] - s["x"], c["y"] - s["y"]) < 1.0 for s in long_seats)]
    xs = [p[0] for p in outline]; ys = [p[1] for p in outline]
    return {"source": "learned", "frames": frames, "track_frames": total,
            "heat": [[round((i + 0.5) * CELL, 3), round((j + 0.5) * CELL, 3), v] for (i, j), v in heat.items() if v >= cut],
            "outline": outline,
            "doors": [[c["x"], c["y"]] for c in dc], "door_detail": dc, "lost_at_seat": lost_at_seat,
            "door": [dc[0]["x"], dc[0]["y"]] if dc else None,
            "seats": seats, "seat": [seats[0]["x"], seats[0]["y"]] if seats else None,
            "interference": interference, "quiet_frames": qf,
            "extent": {"xmin": round(min(xs), 2), "xmax": round(max(xs), 2), "ymax": round(max(ys), 2)}}


def merge(room, prev):
    """Keep what was learned before: doors and seats are evidence counts, so a quiet stretch of log never
    erases a door. Clusters within 0.9 m (doors) or 0.8 m (seats) add their counts."""
    doors = [dict(c) for c in prev.get("door_detail", [])]
    for c in room["door_detail"]:
        pinned = next((d for d in doors if d.get("pinned") and math.hypot(d["x"] - c["x"], d["y"] - c["y"]) < 0.9), None)
        if pinned:  # a door someone placed by hand keeps its place; the new trips only add to its count
            pinned["n"] += c["n"]; continue
        m = next((d for d in doors if math.hypot(d["x"] - c["x"], d["y"] - c["y"]) < 0.9), None)
        if m:
            w = m["n"] + c["n"]
            m["x"], m["y"] = round((m["x"] * m["n"] + c["x"] * c["n"]) / w, 2), round((m["y"] * m["n"] + c["y"] * c["n"]) / w, 2)
            m["n"] = w
            for k, v in c["tags"].items():
                m["tags"][k] = m["tags"].get(k, 0) + v
        else:
            doors.append(dict(c))
    doors.sort(key=lambda c: -c["n"])
    room["door_detail"] = doors
    room["doors"] = [[c["x"], c["y"]] for c in doors]
    room["door"] = room["doors"][0] if doors else None
    seats = [dict(s) for s in prev.get("seats", [])]
    room["removed"] = prev.get("removed", [])
    room["door_detail"] = [d for d in room["door_detail"]
                           if not any(math.hypot(d["x"] - r[0], d["y"] - r[1]) < 0.9 for r in room["removed"])]
    room["doors"] = [[c["x"], c["y"]] for c in room["door_detail"]]
    for c in room["seats"]:
        m = next((s for s in seats if math.hypot(s["x"] - c["x"], s["y"] - c["y"]) < 0.8), None)
        if m:
            m["seconds"] = max(m["seconds"], c["seconds"]); m["visits"] = max(m["visits"], c["visits"])
        else:
            seats.append(dict(c))
    seats.sort(key=lambda s: -s["seconds"])
    room["seats"] = seats
    room["seat"] = [seats[0]["x"], seats[0]["y"]] if seats else None
    room["frames_before"] = prev.get("frames", 0) + prev.get("frames_before", 0)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = lambda k, d=None: sys.argv[sys.argv.index(k) + 1] if k in sys.argv else d
    out = opt("--out", os.path.join(HERE, "room-learned.json"))
    paths = []
    for a in args:
        if a in (opt("--out"), opt("--cfg"), opt("--base")):
            continue
        paths += sorted(glob.glob(os.path.join(a, "frames-*.jsonl"))) if os.path.isdir(a) else [a]
    # a setup starts a new epoch (the radar may have moved): learn only from frames after it, and start the
    # learned doors and seats from the setup's own until real trips arrive
    walked = os.path.join(HERE, "room.json")
    wroom = json.load(open(walked)) if os.path.exists(walked) else {}
    epoch = wroom.get("epoch", 0)
    sources = [[f for f in seq if f["t"] >= epoch] for seq in load(paths)] if epoch else load(paths)
    room = learn([s for s in sources if s])
    prev = json.load(open(out)) if os.path.exists(out) else {}
    if prev.get("epoch", 0) != epoch:
        prev = {}
    seed = {"door_detail": [{"x": d[0], "y": d[1], "n": 0, "tags": {}, "from_setup": True}
                            for d in (wroom.get("doors") or ([wroom["door"]] if wroom.get("door") else []))],
            "seats": [{"x": wroom["seat"][0], "y": wroom["seat"][1], "seconds": 60, "visits": 0, "from_setup": True}]
            if wroom.get("seat") else []}
    if not room:
        room = {"source": "setup", "frames": 0, "heat": [], "outline": wroom.get("outline", []), "doors": [],
                "door_detail": [], "seats": [], "interference": wroom.get("interference", []), "quiet_frames": 0,
                "extent": {"xmin": 0, "xmax": 0, "ymax": 0}}
    merge(room, prev if prev else seed)
    room["epoch"] = epoch
    walked = os.path.join(HERE, "room.json")
    if os.path.exists(walked):
        w = json.load(open(walked))
        room["compare"] = {"walked_doors": w.get("doors") or ([w["door"]] if w.get("door") else []),
                           "walked_seat": w.get("seat"), "walked_boxes": w.get("boxes")}
    json.dump(room, open(out, "w"), indent=1)
    e = room["extent"]
    print(f"LEARN_OK {room['frames']} frames, area x {e['xmin']} to {e['xmax']} m, out to {e['ymax']} m; "
          f"doors {room['doors']}; seats {[(s['x'], s['y'], s['seconds']) for s in room['seats']]}; "
          f"interference cells {len(room['interference'])} over {room['quiet_frames']} quiet frames")
    if opt("--cfg"):
        base = opt("--base", os.path.join(HERE, "cfg", "people-count-250ms.cfg"))
        f2 = lambda v: f"{v:.1f}"
        rep = {"boundaryBox": f"boundaryBox {f2(e['xmin'] - 0.2)} {f2(e['xmax'] + 0.2)} 0 {f2(e['ymax'] + 0.2)} -0.5 3",
               "presenceBoundaryBox": f"presenceBoundaryBox {f2(e['xmin'])} {f2(e['xmax'])} 0.3 {f2(e['ymax'])} 0 3"}
        lines = [rep.get(l.split()[0], l.rstrip("\n")) if l.strip() else l.rstrip("\n") for l in open(base)]
        open(opt("--cfg"), "w").write(f"% Learned by learn_room.py from {len(paths)} logs\n" + "\n".join(lines) + "\n")
        print("CFG", opt("--cfg"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
