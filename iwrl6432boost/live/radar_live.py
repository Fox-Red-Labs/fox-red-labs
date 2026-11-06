#!/usr/bin/env python3
"""Live point cloud, people tracks and presence from the IWRL6432BOOST, in a browser.

  radar_live.py [--port /dev/cu.usbmodemRI321] [--cfg cfg/people-count-250ms.cfg] [--no-config]
                [--probe RI32] [--http 8765] [--record out.bin] [--replay in.bin]
  radar_live.py --selftest

Resets the radar through its XDS110 probe, sends the config, then reads frames and serves
http://127.0.0.1:8765/ (top-down map, points, tracks with trails, presence, counts).
Frame layout read from mmWave L SDK 05.05.04.02 source (mmwave_demo.h, trackerproc.h, GTRACK_3D):
  301 points: float xyzUnit, dopplerUnit, snrUnit, noiseUnit; uint16 num[2]; then int16 x,y,z,doppler; uint8 snr,noise
  308 tracks: per track uint32 tid; float pos[3], vel[3], acc[3], ec[16], g, confidence (112 bytes)
  309 index: one uint8 per point, the track id (253, 254, 255 = none)
  315 presence: uint8 overall, then one bit pair per zone
Needs pyserial. Only stdlib otherwise.
"""
import argparse, json, math, os, queue, struct, subprocess, sys, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

HERE = os.path.dirname(os.path.abspath(__file__))
MAGIC = b"\x02\x01\x04\x03\x06\x05\x08\x07"
def find_xds_reset():
    """The XDS110 reset tool: XDS110RESET, else the newest CCS on macOS or Linux. None means no probe reset
    (recovery then only sends the config again; a hung chip needs a USB replug)."""
    import glob
    if os.environ.get("XDS110RESET"):
        return os.environ["XDS110RESET"]
    tail = "ccs/ccs_base/common/uscif/xds110/xds110reset"
    hits = sorted(glob.glob("/Applications/ti/ccs*/" + tail) + glob.glob(os.path.expanduser("~/ti/ccs*/") + tail)
                  + glob.glob("/opt/ti/ccs*/" + tail))
    return hits[-1] if hits else None


XDS = find_xds_reset()


def find_port(probe):
    """The radar's application UART (XDS110 interface 0) for probe serial `probe`, on macOS or Linux."""
    import serial.tools.list_ports as lp
    ports = [p for p in lp.comports() if p.vid == 0x0451 and p.pid == 0xBEF3 and (not probe or p.serial_number == probe)]
    if not ports:
        sys.exit(f"no XDS110 with serial {probe!r}: pass --port, or --probe with the serial on the radar's probe")
    if len({p.serial_number for p in ports}) > 1:
        sys.exit("more than one XDS110 connected: pass --probe <serial> (or --port)")
    by_if = [p for p in ports if (p.location or "").endswith(":1.0")]  # Linux names the interface
    return (by_if or sorted(ports, key=lambda p: p.device))[0].device
HDR = struct.Struct("<8sIIIIIIII")  # magic, version, totalLen, platform, frame, cpu time, numObj, numTLV, subframe
TRK = 112


def split_frames(buf):
    """Yield (frame_bytes, rest). Frames start with MAGIC and carry their total length."""
    out = []
    while True:
        i = buf.find(MAGIC)
        if i < 0:
            return out, buf[-7:]
        buf = buf[i:]
        if len(buf) < HDR.size:
            return out, buf
        total = HDR.unpack_from(buf)[2]
        if total < HDR.size or total > 65536:
            buf = buf[8:]
            continue
        if len(buf) < total:
            return out, buf
        out.append(buf[:total])
        buf = buf[total:]


def decode(fr):
    _, _, total, plat, fnum, _, nobj, ntlv, _ = HDR.unpack_from(fr)
    o, off = {"frame": fnum, "points": [], "tracks": [], "presence": None, "zones": [], "tlvs": []}, HDR.size
    idx = None
    for _ in range(ntlv):
        if off + 8 > len(fr):
            break
        t, n = struct.unpack_from("<II", fr, off)
        b = fr[off + 8: off + 8 + n]
        off += 8 + n
        o["tlvs"].append(t)
        if t == 1:  # pointCloud 1 on the 05.04 image: float x, y, z, doppler per point
            for j in range(n // 16):
                px, py, pz, pd = struct.unpack_from("<4f", b, 16 * j)
                o["points"].append([round(px, 3), round(py, 3), round(pz, 3), round(pd, 3), None])
        elif t == 7:  # side info: int16 snr, int16 noise per point, units of 0.1 dB
            side = [struct.unpack_from("<hh", b, 4 * j)[0] / 10 for j in range(n // 4)]
            for p, sn in zip(o["points"], side):
                p[4] = sn
        elif t == 301 and n >= 20:
            xu, du, su, nu, n0, n1 = struct.unpack_from("<ffffHH", b)
            k = (n - 20) // 10
            for j in range(k):
                x, y, z, d, s, nz = struct.unpack_from("<hhhhBB", b, 20 + 10 * j)
                o["points"].append([round(x * xu, 3), round(y * xu, 3), round(z * xu, 3),
                                    round(d * du, 3), round(s * su, 1)])
        elif t == 308:
            for j in range(n // TRK):
                v = struct.unpack_from("<I9f", b, j * TRK)
                conf = struct.unpack_from("<f", b, j * TRK + TRK - 4)[0]
                o["tracks"].append({"id": v[0], "x": round(v[1], 3), "y": round(v[2], 3), "z": round(v[3], 3),
                                    "vx": round(v[4], 3), "vy": round(v[5], 3), "conf": round(conf, 2)})
        elif t == 309:
            idx = list(b)
        elif t == 315 and n >= 1:
            o["presence"] = b[0]
            zb = b[1:]
            o["zones"] = [(zb[z // 4] >> (2 * (z % 4))) & 3 for z in range(len(zb) * 4)]
    if idx and len(idx) == len(o["points"]):
        for p, tid in zip(o["points"], idx):
            p.append(tid if tid < 250 else None)
    return o


def selftest():
    pts = struct.pack("<ffffHH", 0.01, 0.1, 0.5, 0.5, 1, 0) + struct.pack("<hhhhBB", 100, 250, -20, -3, 40, 2)
    trk = struct.pack("<I9f", 7, 1.0, 2.5, 0.2, 0.1, -0.3, 0, 0, 0, 0) + b"\0" * (16 * 4 + 4) + struct.pack("<f", 0.9)
    tl = (struct.pack("<II", 301, len(pts)) + pts + struct.pack("<II", 308, len(trk)) + trk
          + struct.pack("<II", 309, 1) + bytes([7]) + struct.pack("<II", 315, 2) + bytes([1, 0b01]))
    total = HDR.size + len(tl)
    fr = HDR.pack(MAGIC, 0x0105, total, 0xA6432, 42, 0, 1, 4, 0) + tl
    frames, rest = split_frames(b"junk" + fr + fr[:10])
    assert len(frames) == 1 and rest == fr[:10], (len(frames), rest)
    d = decode(frames[0])
    assert d["frame"] == 42 and d["points"] == [[1.0, 2.5, -0.2, -0.3, 20.0, 7]], d["points"]
    assert d["tracks"][0]["id"] == 7 and d["tracks"][0]["y"] == 2.5 and d["tracks"][0]["conf"] == 0.9
    assert d["presence"] == 1 and d["zones"][:2] == [1, 0]
    p1 = struct.pack("<4f", 0.5, 1.5, 0.25, -0.5)
    tl1 = struct.pack("<II", 1, 16) + p1 + struct.pack("<II", 7, 4) + struct.pack("<hh", 123, 40)
    fr1 = HDR.pack(MAGIC, 0x0105, HDR.size + len(tl1), 0xA6432, 43, 0, 1, 2, 0) + tl1
    assert decode(fr1)["points"] == [[0.5, 1.5, 0.25, -0.5, 12.3]], decode(fr1)["points"]
    print("SELFTEST_OK")


class Hub:
    log_dir = None

    def log(self, msg):
        """One compact line a frame: time, frame, tracks [id,x,y,z,vx,vy], moving points [x,y,doppler]."""
        day = time.strftime("%Y-%m-%d")
        if getattr(self, "_day", None) != day:
            os.makedirs(self.log_dir, exist_ok=True)
            if getattr(self, "_f", None):
                self._f.close()
            self._f = open(os.path.join(self.log_dir, f"frames-{day}.jsonl"), "a"); self._day = day
            cutoff = time.time() - 14 * 86400   # keep 14 days of frames; events and recordings stay
            for n in os.listdir(self.log_dir):
                fp = os.path.join(self.log_dir, n)
                if n.startswith("frames-20") and os.path.getmtime(fp) < cutoff:
                    os.remove(fp)
        r2 = lambda v: round(v, 2)
        line = {"t": round(time.time(), 2), "f": msg["frame"],
                "tr": [[t["id"], r2(t["x"]), r2(t["y"]), r2(t["z"]), r2(t["vx"]), r2(t["vy"])] for t in msg["tracks"]],
                "p": [[r2(p[0]), r2(p[1]), r2(p[3])] for p in msg["points"] if abs(p[3]) > 0.1]}
        self._f.write(json.dumps(line, separators=(",", ":")) + "\n"); self._f.flush()

    room_state = None
    link = "up"

    def __init__(self):
        self.subs, self.lock, self.last, self.stats = [], threading.Lock(), None, {"frames": 0, "since": time.time()}
        self.health = {"bad_frames": 0, "lost_frames": 0, "recoveries": 0, "frame_age_s": 0.0,
                       "last_error": "", "last_recovery": ""}
        self.learn_lock = threading.Lock()

    def set_link(self, up):
        self.link = "up" if up else "down"
        if self.room_state:
            self.room_state.set_link(up)
        if not up:  # tell open pages too
            self.push_raw({"link": "down", "frame": None, "points": [], "tracks": [], "tlvs": []})

    def push_raw(self, msg):
        with self.lock:
            for q in list(self.subs):
                try:
                    q.put_nowait(msg)
                except queue.Full:
                    pass

    def push(self, msg):
        if self.room_state:
            try:
                msg["room_state"] = self.room_state.update(msg)
            except Exception as e:
                msg["room_state"] = {"error": str(e)[:80]}
        if self.log_dir:
            try:
                self.log(msg)
            except OSError as e:
                self.health["last_error"] = "log: " + str(e)[:70]
        self.last = msg
        self.stats["frames"] += 1
        with self.lock:
            for q in list(self.subs):
                try:
                    q.put_nowait(msg)
                except queue.Full:
                    pass

    def sub(self):
        q = queue.Queue(maxsize=50)
        with self.lock:
            self.subs.append(q)
        return q

    def unsub(self, q):
        with self.lock:
            if q in self.subs:
                self.subs.remove(q)


def configure(port, cfg, probe):
    import serial
    if probe and XDS and os.path.exists(XDS):
        subprocess.run([XDS, "-s", probe], capture_output=True, timeout=30)
        time.sleep(6)
    s = serial.Serial(port, 115200, timeout=0.1)
    s.reset_input_buffer()
    bad = []
    for raw in open(cfg):
        line = raw.strip()
        if not line or line.startswith("%") or line.startswith("baudRate"):
            continue
        for attempt in range(2):  # a line sent while a frame drains can be lost: resend once
            s.write((line + "\n").encode())
            t, b = time.time(), b""
            while time.time() - t < 2.0:
                b += s.read(4096)
                if b"Done" in b or b"rror" in b:
                    break
            if b"Done" in b or b"rror" in b:
                break
        if line.startswith("sensorStop"):
            time.sleep(0.5)
            s.reset_input_buffer()
        if b"Done" not in b:
            bad.append((line, b.decode(errors="replace").strip()[-120:]))
    s.close()
    return bad


def reader(hub, port, record, replay, skip=0, cfg=None, probe=None):
    buf, rec = b"", open(record, "ab") if record else None
    if replay:
        data = open(replay, "rb").read()
        while True:
            frames, _ = split_frames(data)
            for fr in frames[skip:]:
                hub.push(decode(fr))
                time.sleep(0.25)
    import serial
    # watchdog: no frame for 3 s marks the link down; 8 s brings a probe reset and the config again.
    # A USB unplug, a hung chip or a failed start all end up here instead of a frozen state.
    s, last_frame, last_fix, prev_no = None, time.time(), 0.0, None
    while True:
        try:
            if s is None:
                s = serial.Serial(port, 115200, timeout=0.05)
            chunk = s.read(4096)
        except (serial.SerialException, OSError) as e:
            hub.health["last_error"] = str(e)[:80]
            if s:
                try: s.close()
                except Exception: pass
            s = None
            chunk = b""
            time.sleep(1)
        now = time.time()
        if chunk:
            if rec:
                rec.write(chunk); rec.flush()
            frames, buf = split_frames(buf + chunk)
            for fr in frames:
                try:
                    d = decode(fr)
                except (struct.error, IndexError, ValueError):
                    hub.health["bad_frames"] += 1
                    continue
                if prev_no is not None and d["frame"] > prev_no + 1:
                    hub.health["lost_frames"] += d["frame"] - prev_no - 1
                prev_no = d["frame"]
                last_frame = now
                if hub.link != "up":
                    hub.set_link(True)
                hub.push(d)
        age = now - last_frame
        hub.health["frame_age_s"] = round(age, 1)
        if age > 3 and hub.link == "up":
            hub.set_link(False)
        if age > 8 and cfg and now - last_fix > 30:
            last_fix = now
            hub.health["recoveries"] += 1
            if s:
                try: s.close()
                except Exception: pass
            s = None
            try:
                bad = configure(port, cfg, probe)
                hub.health["last_recovery"] = time.strftime("%H:%M:%S") + (" ok" if not bad else f" refused {bad[0][0]}")
            except Exception as e:
                hub.health["last_recovery"] = time.strftime("%H:%M:%S") + " failed " + str(e)[:60]
            buf, prev_no = b"", None


def serve(hub, httpport, bind=None):
    page = open(os.path.join(HERE, "index.html"), "rb").read()

    class H(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            if self.path.startswith("/events"):
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                q = hub.sub()
                try:
                    while True:
                        try:
                            m = q.get(timeout=5)
                            self.wfile.write(b"data: " + json.dumps(m).encode() + b"\n\n")
                        except queue.Empty:
                            self.wfile.write(b": keepalive\n\n")
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    pass
                finally:
                    hub.unsub(q)
            elif self.path.startswith(("/review", "/api/day", "/export.csv", "/api/learn")):
                import review, urllib.parse
                q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
                day = (q.get("d") or [time.strftime("%Y-%m-%d")])[0][:10]
                if self.path.startswith("/review"):
                    body, ctype = open(os.path.join(HERE, "review.html"), "rb").read(), "text/html; charset=utf-8"
                elif not hub.log_dir:
                    body, ctype = b'{"error": "start radar_live.py with --log DIR"}', "application/json"
                elif self.path.startswith("/api/day"):
                    body, ctype = json.dumps(review.summary(hub.log_dir, day)).encode(), "application/json"
                elif self.path.startswith("/export.csv"):
                    body, ctype = review.events_csv(hub.log_dir, day).encode(), "text/csv"
                else:
                    if hub.learn_lock.acquire(blocking=False):
                        try:
                            p = subprocess.run([sys.executable, os.path.join(HERE, "learn_room.py"), hub.log_dir],
                                               capture_output=True, text=True, timeout=600)
                            out = p.stdout.strip()[-300:]
                        finally:
                            hub.learn_lock.release()
                    else:
                        out = "a learning pass is already running"
                    body, ctype = json.dumps({"out": out}).encode(), "application/json"
                self.send_response(200)
                self.send_header("Content-Type", ctype)
                if ctype == "text/csv":
                    self.send_header("Content-Disposition", f'attachment; filename="radar-room-{day}.csv"')
                self.end_headers()
                self.wfile.write(body)
            elif self.path.startswith("/occupancy"):
                if self.path.startswith("/occupancy/reset") and hub.room_state:
                    hub.room_state.reset_counts()
                if self.path.startswith("/occupancy/moved-ok") and hub.room_state:
                    hub.room_state.clear_moved()
                body = json.dumps({**(hub.room_state.state() if hub.room_state else {}), "health": hub.health}).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(body)
            elif self.path.startswith("/state") or self.path.startswith("/room"):
                rp = os.path.join(HERE, "room-learned.json" if self.path.startswith("/room-learned") else "room.json")
                if self.path.startswith("/room"):
                    body = open(rp, "rb").read() if os.path.exists(rp) else b"{}"
                else:
                    body = json.dumps({"last": hub.last, **hub.stats}).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(body)
            elif self.path in ("/radar.js",) or (self.path.startswith("/cfg/") and self.path.endswith(".cfg")
                                                  and ".." not in self.path):
                fp = os.path.join(HERE, self.path.lstrip("/"))
                if not os.path.exists(fp):
                    self.send_response(404); self.end_headers(); return
                self.send_response(200)
                self.send_header("Content-Type", "text/javascript" if fp.endswith(".js") else "text/plain")
                self.end_headers()
                self.wfile.write(open(fp, "rb").read())
            else:
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                self.wfile.write(open(os.path.join(HERE, "index.html"), "rb").read())

    def do_POST(self):
        """/api/room: {"doors": [[x, y], ...], "seats": [[x, y], ...]} from the page's Adjust mode.
        Doors and seats placed by hand are pinned, so learning adds to them but never moves them."""
        if not self.path.startswith("/api/room"):
            self.send_response(404); self.end_headers(); return
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0)) or 0) or b"{}")
            rp = hub.room_state.room_path if hub.room_state else os.path.join(HERE, "room-learned.json")
            room = json.load(open(rp)) if os.path.exists(rp) else {}
            old = {(round(d["x"], 2), round(d["y"], 2)): d for d in room.get("door_detail", [])}
            dd = []
            for x, y in body.get("doors", []):
                near = min(old.values(), key=lambda d: math.hypot(d["x"] - x, d["y"] - y), default=None)
                n = near["n"] if near and math.hypot(near["x"] - x, near["y"] - y) < 1.5 else 0
                dd.append({"x": round(x, 2), "y": round(y, 2), "n": n, "tags": near["tags"] if n else {}, "pinned": True})
            gone = [[d["x"], d["y"]] for d in room.get("door_detail", [])
                    if not any(math.hypot(d["x"] - x, d["y"] - y) < 1.5 for x, y in body.get("doors", []))]
            room["removed"] = room.get("removed", []) + gone
            room["door_detail"], room["doors"] = dd, [[d["x"], d["y"]] for d in dd]
            room["door"] = room["doors"][0] if dd else None
            if "seats" in body:
                room["seats"] = [{"x": round(x, 2), "y": round(y, 2), "seconds": 3600, "visits": 1, "pinned": True}
                                 for x, y in body["seats"]]
                room["seat"] = room["seats"][0] and [room["seats"][0]["x"], room["seats"][0]["y"]] if room["seats"] else None
            json.dump(room, open(rp, "w"), indent=1)
            out = json.dumps({"ok": True, "doors": room["doors"], "seats": len(room.get("seats", []))}).encode()
            self.send_response(200)
        except Exception as e:
            out = json.dumps({"ok": False, "error": str(e)[:100]}).encode()
            self.send_response(400)
        self.send_header("Content-Type", "application/json"); self.end_headers(); self.wfile.write(out)

    H.do_POST = do_POST
    if bind:  # also on this host's LAN address, so a phone or laptop on the network can open the page
        threading.Thread(target=ThreadingHTTPServer((bind, httpport), H).serve_forever, daemon=True).start()
    ThreadingHTTPServer(("127.0.0.1", httpport), H).serve_forever()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="auto", help="the radar's UART; auto finds it from --probe (macOS or Linux)")
    ap.add_argument("--cfg", default=os.path.join(HERE, "cfg", "people-count-250ms.cfg"))
    ap.add_argument("--probe", default="RI32", help="serial of the radar's XDS110 probe")
    ap.add_argument("--bind", help="also serve on this host's LAN address, <LAN address>")
    ap.add_argument("--no-config", action="store_true")
    ap.add_argument("--http", type=int, default=8765)
    ap.add_argument("--record")
    ap.add_argument("--replay")
    ap.add_argument("--skip", type=int, default=0, help="replay: start this many frames in")
    ap.add_argument("--log", help="folder for the all-day frame log (frames-YYYY-MM-DD.jsonl)")
    ap.add_argument("--learn-every", type=int, default=0, help="seconds between room-learning passes over the log")
    ap.add_argument("--room", default=os.path.join(HERE, "room.json"),
                    help="room for door, seat and occupancy rules (room.json from the setup, or room-learned.json)")
    ap.add_argument("--mount", choices=["desk", "wall", "ceiling"], default="desk", help="how the page draws the room")
    ap.add_argument("--ha", action="store_true", help="post occupancy, people, entries, exits and seat to Home Assistant")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if a.port == "auto" and not a.replay:
        a.port = find_port(a.probe)
        print("radar port", a.port, "| probe reset", XDS or "none", flush=True)
    if not a.no_config and not a.replay:
        bad = configure(a.port, a.cfg, a.probe)
        print("CONFIG_OK" if not bad else f"CONFIG_ERRORS {bad}", flush=True)
    hub = Hub()
    hub.log_dir = a.log
    hub.health["mount"] = a.mount
    import room_state
    hub.room_state = room_state.RoomState(a.room, a.log)
    if a.ha:
        poster = room_state.HAPoster()
        hub.room_state.listeners.append(poster.on_change)
        poster.post(hub.room_state.state())
        print(f"HA posts ok {poster.ok} failed {poster.fail}", flush=True)
    threading.Thread(target=reader, args=(hub, a.port, a.record, a.replay, a.skip, a.cfg, a.probe), daemon=True).start()
    if a.log and a.learn_every:
        def learner():
            while True:
                time.sleep(a.learn_every)
                if hub.learn_lock.acquire(blocking=False):
                    try:
                        subprocess.run([sys.executable, os.path.join(HERE, "learn_room.py"), a.log],
                                       capture_output=True, timeout=600)
                    finally:
                        hub.learn_lock.release()
        threading.Thread(target=learner, daemon=True).start()
    print(f"open http://127.0.0.1:{a.http}/" + (f" or http://{a.bind}:{a.http}/" if a.bind else ""), flush=True)
    serve(hub, a.http, a.bind)


if __name__ == "__main__":
    sys.exit(main())
