#!/usr/bin/env python3
"""Room state from radar frames, for one or more people: people count, occupied, door entries and exits,
seats in use, and the radar link. The same rules feed the page, Home Assistant, the event log and exports.

Rules (from the 2026-11-03 runs in iwrl6432boost/live/README.md and the review in
docs/research/radar-demo-review-2026-11-03.md):
  - frozen copy: a track with no points for 3 s while another track within 3 m has points is dropped
  - held person: a track that vanishes away from every door is a still person. Each is held at its spot on its own.
    A hold ends when a new track starts within 1.2 m (the same person found again, not an entry), or when no
    moving point comes within 1.0 m of the spot for `hold_limit` seconds (default 600)
  - exit: a vanished track whose last 2 s came within 1.5 m of a door, still moving at the end or ending within
    1.0 m of it. An exit waits 3 s: a new track starting within 1.5 m of it in that time is the same person
    (the tracker changed its id), so neither an exit nor an entry is counted
  - entry: a new track within 1.5 m of a door that is not the same person found again, or any new track
    while the room was empty (picked up late), counted at the nearest door
  - seats: each learned seat (60 s or more of stay) is in use while a person is within 0.8 m, with 3 s to turn on
    and 3 s to turn off
  - people = tracked people + held people; occupied = people > 0
  - link: the server marks it down when no frame arrives for 3 s; state then says so instead of a stale count
Timers run on radar time (frame counter times the frame period), with a reboot of the radar detected when the
counter goes backwards. Events go to events-YYYY-MM-DD.jsonl: in, out, occupied, empty, seat_on, seat_off,
hold, hold_end, link_down, link_up.
"""
import json, math, os, queue, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
D = lambda a, b: math.hypot(a[0] - b[0], a[1] - b[1])


class RoomState:
    def __init__(self, room_path, log_dir=None, frame_s=0.25, hold_limit=600.0):
        self.room_path, self.log_dir, self.frame_s, self.hold_limit = room_path, log_dir, frame_s, hold_limit
        self.room, self.room_mtime = {}, 0
        self.lock = threading.Lock()
        self.seen = {}          # track id -> {path, last, last_pts, ghost}
        self.held = []          # [{x, y, since, refreshed}]
        self.pending = []       # exits waiting 3 s: {x, y, at, door}
        self.seats_on = {}      # seat index -> {on, in_since, out_since}
        self.entries = self.exits = 0
        self.since = time.time()
        self.day = time.strftime("%Y-%m-%d")
        self.people, self.occupied, self.link = 0, False, "up"
        self.t_off, self.last_frame, self.last_now = 0.0, None, 0.0
        self.listeners, self._ev_f, self._ev_day = [], None, None
        self.seen_empty, self.quiet_since = False, None   # the room is known empty (not just at startup)

    # ---- room
    def _load_room(self):
        try:
            m = os.path.getmtime(self.room_path)
            if m != self.room_mtime:
                self.room, self.room_mtime = json.load(open(self.room_path)), m
        except (OSError, ValueError):
            pass

    def doors(self):
        r = self.room
        return r.get("doors") or ([r["door"]] if r.get("door") else [])

    def seats(self):
        r = self.room
        if r.get("seats"):
            return [[s["x"], s["y"]] for s in r["seats"] if s.get("seconds", 0) >= 60]
        return [r["seat"]] if r.get("seat") else []

    # ---- events
    def _event(self, kind, **kw):
        ev = {"t": round(time.time(), 2), "event": kind, **kw}
        if self.log_dir:
            day = time.strftime("%Y-%m-%d")
            try:
                if self._ev_day != day:
                    if self._ev_f:
                        self._ev_f.close()
                    os.makedirs(self.log_dir, exist_ok=True)
                    self._ev_f, self._ev_day = open(os.path.join(self.log_dir, f"events-{day}.jsonl"), "a"), day
                self._ev_f.write(json.dumps(ev) + "\n"); self._ev_f.flush()
            except OSError as e:
                self.log_error = str(e)[:80]
        return ev

    def _notify(self, changed):
        if changed:
            st = self._state()
            for fn in self.listeners:
                fn(st, changed)

    def set_link(self, up):
        with self.lock:
            new = "up" if up else "down"
            if new == self.link:
                return
            self.link = new
            self._notify([self._event("link_up" if up else "link_down")])

    # ---- frames
    def _clock(self, frame):
        if self.last_frame is not None and frame < self.last_frame:   # the radar restarted: keep time moving
            self.t_off = self.last_now + self.frame_s - frame * self.frame_s
        self.last_frame = frame
        self.last_now = frame * self.frame_s + self.t_off
        return self.last_now

    def update(self, msg):
        with self.lock:
            return self._update(msg)

    def _update(self, msg):
        self._load_room()
        now, doors, seats = self._clock(msg["frame"]), self.doors(), self.seats()
        if time.strftime("%Y-%m-%d") != self.day:
            self.day, self.entries, self.exits, self.since = time.strftime("%Y-%m-%d"), 0, 0, time.time()
        door_of = lambda p, r=1.5: next((i for i, d in enumerate(doors) if D(p, d) < r), None)
        pts = {}
        for p in msg["points"]:
            if len(p) > 5 and p[5] is not None:
                pts[p[5]] = pts.get(p[5], 0) + 1
        moving = [(p[0], p[1]) for p in msg["points"] if abs(p[3]) > 0.1]
        changed = []
        # a knock: 10 or more moving points spread over 3 m of range in one frame, twice within 3 s
        # (2026-11-03: two such bursts all day, both bumps; walking gave one single frame)
        if len(moving) >= 10:
            rs = [math.hypot(a, b) for a, b in moving]
            if max(rs) - min(rs) > 3:
                self.jolts = [t for t in getattr(self, "jolts", []) if now - t <= 3] + [now]
                if len(self.jolts) >= 2 and now - getattr(self, "moved_at_radar", -99) > 30:
                    self.moved_at_radar, self.moved_at = now, time.strftime("%H:%M")
                    changed.append(self._event("moved"))
        live = {t["id"] for t in msg["tracks"]}

        # new tracks: the same person found again (a hold, or an id change at a door), or an entry
        for t in msg["tracks"]:
            if t["id"] in self.seen:
                continue
            xy = (t["x"], t["y"])
            self.seen[t["id"]] = {"path": [], "last_pts": now, "ghost": False}
            h = min(self.held, key=lambda h: D(xy, (h["x"], h["y"])), default=None)
            pe = next((e for e in self.pending if D(xy, (e["x"], e["y"])) < 1.5), None)
            if h and D(xy, (h["x"], h["y"])) < 1.2:
                self.held.remove(h); changed.append(self._event("hold_end", why="found", x=h["x"], y=h["y"]))
            elif pe:
                self.pending.remove(pe)        # id changed near the door: not an exit and not an entry
            else:
                d = door_of(xy)
                if d is None and self.people == 0 and doors and self.seen_empty:
                    # nobody was in the room, so whoever this is came in, even if the tracker found them late
                    d = min(range(len(doors)), key=lambda i: D(xy, doors[i]))
                    changed.append(self._event("in", door=d, x=t["x"], y=t["y"], late=True)); self.entries += 1
                elif d is not None:
                    self.entries += 1; changed.append(self._event("in", door=d, x=t["x"], y=t["y"]))
        for t in msg["tracks"]:
            v = self.seen[t["id"]]
            v["last"] = t; v["path"] = (v["path"] + [t])[-8:]
            if pts.get(t["id"]):
                v["last_pts"] = now

        # frozen copies
        for a in msg["tracks"]:
            va = self.seen[a["id"]]
            if not va["ghost"] and now - va["last_pts"] >= 3 and any(
                    b["id"] != a["id"] and pts.get(b["id"]) and D((a["x"], a["y"]), (b["x"], b["y"])) < 3
                    for b in msg["tracks"]):
                va["ghost"] = True

        # vanished tracks: an exit (after 3 s) or a held person
        for k in [k for k in self.seen if k not in live]:
            v = self.seen.pop(k)
            if v["ghost"] or "last" not in v:
                continue
            last = (v["last"]["x"], v["last"]["y"])
            near = [i for i in range(len(doors)) if any(D((p["x"], p["y"]), doors[i]) < 1.5 for p in v["path"])]
            # tracks pass through a door and slide on before they end, so: the path came near a door, and the
            # track was still moving at the end or stopped right at the door (someone slowing to open it)
            moving_end = math.hypot(v["last"]["vx"], v["last"]["vy"]) > 0.2
            exit_door = next((i for i in near if moving_end or D(last, doors[i]) < 1.0), None)
            if exit_door is not None:
                self.pending.append({"x": last[0], "y": last[1], "at": now, "door": exit_door})
            else:
                self.held.append({"x": last[0], "y": last[1], "since": now, "refreshed": now})
                changed.append(self._event("hold", x=round(last[0], 2), y=round(last[1], 2)))

        for e in [e for e in self.pending if now - e["at"] >= 3]:
            self.pending.remove(e); self.exits += 1; changed.append(self._event("out", door=e["door"]))

        # holds: refreshed by small motion nearby, ended after hold_limit without any
        for h in list(self.held):
            if any(D(m, (h["x"], h["y"])) < 1.0 for m in moving):
                h["refreshed"] = now
            if now - h["refreshed"] >= self.hold_limit:
                self.held.remove(h); changed.append(self._event("hold_end", why="timeout", x=h["x"], y=h["y"]))

        real = [t for t in msg["tracks"] if not self.seen[t["id"]]["ghost"]]
        people_xy = ([(t["x"], t["y"]) for t in real] + [(h["x"], h["y"]) for h in self.held]
                     + [(e["x"], e["y"]) for e in self.pending])  # at the door, exit not yet confirmed
        self.people_xy = people_xy
        n = len(people_xy)
        if n != self.people:
            self.people = n; changed.append({"event": "count", "people": n})
        if (n > 0) != self.occupied:
            self.occupied = n > 0; changed.append(self._event("occupied" if n else "empty"))

        for i, s in enumerate(seats):
            st = self.seats_on.setdefault(i, {"on": False, "in": None, "out": None})
            at = any(D(p, s) < 0.8 for p in people_xy)
            away = not any(D(p, s) < 1.0 for p in people_xy)
            st["in"] = (st["in"] or now) if at else None
            st["out"] = (st["out"] or now) if away else None
            if not st["on"] and at and now - st["in"] >= 3:
                st["on"] = True; changed.append(self._event("seat_on", seat=i))
            if st["on"] and away and now - st["out"] >= 3:
                st["on"] = False; changed.append(self._event("seat_off", seat=i))
        for i in [i for i in self.seats_on if i >= len(seats)]:
            del self.seats_on[i]

        # the room counts as seen empty after a door exit emptied it, or 10 s with no track and no movement
        if n == 0 and not msg["tracks"] and not moving:
            self.quiet_since = self.quiet_since if self.quiet_since is not None else now
            if now - self.quiet_since >= 10 or any(e.get("event") == "out" for e in changed):
                self.seen_empty = True
        else:
            self.quiet_since = None
        if n == 0 and any(e.get("event") == "out" for e in changed):
            self.seen_empty = True
        self._notify(changed)
        return self._state()

    def _state(self):
        used = sum(1 for s in self.seats_on.values() if s["on"])
        return {"link": self.link, "occupied": self.occupied and self.link == "up", "people": self.people,
                "tracked": self.people - len(self.held) - len(self.pending), "held": len(self.held), "entries": self.entries,
                "exits": self.exits, "since": time.strftime("%H:%M", time.localtime(self.since)),
                "seats_used": used, "seats": len(self.seats_on), "seat": used > 0, "doors": len(self.doors()),
                "moved_at": getattr(self, "moved_at", None),
                "held_xy": [[round(h["x"], 2), round(h["y"], 2), round(self.last_now - h["since"])] for h in self.held],
                "door_xy": [[round(e["x"], 2), round(e["y"], 2)] for e in self.pending]}

    def state(self):
        with self.lock:
            return self._state()

    def clear_moved(self):
        with self.lock:
            self.moved_at = None

    def reset_counts(self):
        with self.lock:
            self.entries = self.exits = 0; self.since = time.time()
            return self._state()


class HAPoster:
    """Posts the room state to Home Assistant from one thread: the newest state on change and every 30 s."""
    ENT = {"occupied": ("binary_sensor.radar_room_occupied", "occupancy", None),
           "people": ("sensor.radar_room_people", None, "people"),
           "entries": ("sensor.radar_room_entries", None, "entries"),
           "exits": ("sensor.radar_room_exits", None, "exits"),
           "seat": ("binary_sensor.radar_room_seat", "occupancy", None),
           "link": ("binary_sensor.radar_room_link", "connectivity", None)}

    def __init__(self):
        sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
        import ha  # URL and token from the home-assistant MCP entry, read at run time
        self.ha, self.q = ha, queue.Queue(maxsize=1)
        self.ok = self.fail = 0; self.last = None; self.last_error = ""
        threading.Thread(target=self._run, daemon=True).start()

    def post(self, st):
        for k, (ent, dev, unit) in self.ENT.items():
            v = st[k] == "up" if k == "link" else st[k]
            attrs = {"friendly_name": "Radar room " + k, "source": "IWRL6432BOOST"}
            if dev: attrs["device_class"] = dev
            if unit: attrs["unit_of_measurement"] = unit
            body = {"state": ("on" if v else "off") if isinstance(v, bool) else v, "attributes": attrs}
            for attempt in range(2):
                try:
                    self.ha.req("POST", f"/api/states/{ent}", body); self.ok += 1; break
                except Exception as e:
                    self.fail += 1; self.last_error = str(e)[:80]; time.sleep(1)

    def on_change(self, st, changed):
        try:
            self.q.get_nowait()
        except queue.Empty:
            pass
        self.q.put_nowait(st)

    def _run(self):
        while True:
            try:
                st = self.q.get(timeout=30)
            except queue.Empty:
                st = self.last
            if st:
                self.last = st; self.post(st)


# ---- tests with no radar: one and two people
def _frames():
    n = [0]
    def fr(tracks, extra_moving=()):
        n[0] += 1
        pts = [[t["x"], t["y"], 0, 0.3, 20, t["id"]] for t in tracks] + [[x, y, 0, 0.3, 20, None] for x, y in extra_moving]
        return {"frame": n[0], "points": pts, "tracks": tracks}
    return fr


T = lambda i, x, y: {"id": i, "x": x, "y": y, "z": 0, "vx": 0, "vy": 0, "conf": 1}


def _events(d):
    return [json.loads(l)["event"] for l in open(os.path.join(d, f"events-{time.strftime('%Y-%m-%d')}.jsonl"))]


def selftest():
    import tempfile
    room = {"doors": [[2.0, 5.0]], "seats": [{"x": 0.3, "y": 1.0, "seconds": 600}, {"x": -1.5, "y": 2.0, "seconds": 300}]}

    # one person: in, sit, lost by the tracker, found again, out
    d = tempfile.mkdtemp(); rp = os.path.join(d, "room.json"); json.dump(room, open(rp, "w"))
    rs, fr = RoomState(rp, d), _frames()
    s = rs.update(fr([T(1, 2.0, 4.8)]))
    for y in (4.0, 3.0, 2.0) + (1.0,) * 14:
        s = rs.update(fr([T(1, 0.3, y)]))
    assert s["entries"] == 1 and s["seats_used"] == 1
    for _ in range(20):
        s = rs.update(fr([]))
    assert s["people"] == 1 and s["held"] == 1 and s["occupied"], s
    s = rs.update(fr([T(2, 0.35, 1.05)]))                     # found again at the seat
    assert s["entries"] == 1 and s["held"] == 0 and s["people"] == 1
    for y in (2.0, 3.0, 4.0, 4.6, 4.9):
        s = rs.update(fr([T(2, 1.8, y)]))
    for _ in range(16):
        s = rs.update(fr([]))
    assert s["exits"] == 1 and not s["occupied"] and s["people"] == 0, s
    ev = _events(d)
    assert ev[:5] == ["in", "occupied", "seat_on", "hold", "hold_end"] and ev[-2:] == ["out", "empty"] and "seat_off" in ev, ev

    # two people: A sits and is lost; B walks in and out; A is still counted and held after B leaves
    d = tempfile.mkdtemp(); rp = os.path.join(d, "room.json"); json.dump(room, open(rp, "w"))
    rs, fr = RoomState(rp, d), _frames()
    for y in (4.8, 3.5, 2.5) + (1.0,) * 10:
        s = rs.update(fr([T(1, 0.3, y)]))
    for _ in range(4):
        s = rs.update(fr([]))                                  # A lost while seated
    for y in (4.8, 4.0, 3.0, 2.2):
        s = rs.update(fr([T(5, 1.5, y)]))                      # B comes in
    assert s["people"] == 2 and s["entries"] == 1, s           # A was there at startup (not an entry); B came in
    for y in (3.0, 4.0, 4.7, 4.95):
        s = rs.update(fr([T(5, 1.9, y)]))
    for _ in range(16):
        s = rs.update(fr([]))                                  # B leaves through the door
    assert s["exits"] == 1 and s["people"] == 1 and s["held"] == 1 and s["occupied"], s

    # the tracker changes B's id at the door: no extra exit or entry
    for y in (4.8, 4.0):
        s = rs.update(fr([T(7, 1.9, y)]))                      # B back in (entry 2)
    s = rs.update(fr([]))                                      # id dropped at 4.0, ends away from the door: held
    e0, x0 = s["entries"], s["exits"]
    s = rs.update(fr([T(8, 1.9, 3.9)]))                        # found again: the hold there ends
    assert s["entries"] == e0 and s["exits"] == x0 and s["people"] == 2, s

    # a hold with no motion ends after the limit
    d = tempfile.mkdtemp(); rp = os.path.join(d, "room.json"); json.dump(room, open(rp, "w"))
    rs, fr = RoomState(rp, d, hold_limit=5), _frames()
    for y in (3.0, 2.0, 1.5):
        rs.update(fr([T(1, 0.0, y)]))
    for _ in range(30):
        s = rs.update(fr([]))
    assert s["people"] == 0 and not s["occupied"] and "hold_end" in _events(d), s

    # a radar restart (frame counter back to 1) keeps timers moving forward
    rs, fr = RoomState(rp, None), _frames()
    for _ in range(10):
        rs.update(fr([]))
    t1 = rs.last_now
    rs.update({"frame": 1, "points": [], "tracks": []})
    assert rs.last_now > t1
    print("SELFTEST_OK")


if __name__ == "__main__":
    selftest()
