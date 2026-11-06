#!/usr/bin/env python3
"""Home base for tool telemetry: where phones and gateways upload, and a fleet page to read it.
    python3 server.py [--port 8090] [--db out/tool-telemetry/homebase.sqlite]
Standard library only. POST /api/ingest takes PROTOCOL.md section 4 from any path (phone, modem line, Wi-Fi uplink) and
keeps one row per record, so the same record arriving twice, by two paths, is stored once. GET / is the fleet page;
GET /api/fleet, /api/events and /api/sources are what it reads. CORS is limited to listed origins because the phone posts
from the hub's page, so CORS answers only the hub's origin and this home base's own (never *). It binds 127.0.0.1 unless
--host says otherwise; beyond loopback every write needs --token, and /api/control is refused without one."""
import argparse
import json
import re
import os
import sys
import sqlite3
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "portability", "fielddata"))
import fd  # noqa: E402  (app schemas and decoding, portability/fielddata)

APPS = fd.load_apps()
SCHEMA = """
create table if not exists items(
  hub text, a text, k text, ident text, unix integer, seq integer, st integer, trig integer, oc integer, flt integer,
  run integer, rpm integer, soc integer, tc integer, ev_type integer, ev_v integer, via text, received real,
  primary key(hub, a, k, ident));
create table if not exists pulls(
  hub text, a text, pid integer, unix integer, dur_ms integer, rpm_avg integer, rpm_max integer, i_avg_ma integer,
  i_peak_ma integer, v_min_mv integer, temp_c integer, model_cls integer, via text, received real,
  primary key(hub, a, pid, dur_ms, rpm_avg));
create table if not exists records(
  hub text, a text, app_id integer, ident text, unix integer, seq integer, fields text, via text, received real,
  primary key(hub, a, app_id, ident));
create table if not exists tags(addr text primary key, hub text, name text, tool text, data text, via text, updated real);
create table if not exists posts(received real, via text, hub text, n_items integer, n_new integer);
create table if not exists hubble(device text, name text, network text, unix real, seq integer, rssi real,
  heard_lat real, heard_lon real, heard_acc real, payload text, msg text, received real, primary key(device, unix, seq));
create table if not exists installs(hub text, a text, ver integer, ok integer, err integer, ms integer, unix integer,
  via text, received real, primary key(hub, a, ver, unix, ok));
"""
lock = threading.Lock()
TOKEN = ""                              # set by --token or FRL_HOMEBASE_TOKEN; uploads must send it as X-Token
RAW_DIR = os.path.abspath("out/tool-telemetry/homebase-raw")
MODEL_DIR = os.path.abspath("out/tool-telemetry/models")   # ai/model_pkg.py writes model-v<N>.json here
HUB_URL = ""                            # --hub-url: the hub the app's Lock and Unlock buttons act through
OWN_HOSTS = set()                       # Host values this home base answers to (set at start, never from requests)
ALLOWED_ORIGINS = set()                 # pages allowed to send controls: the hub's app and this home base
BIND_HOST = "127.0.0.1"                 # the address this process listens on (set at start)
SECURE_HUB = False                      # the hub was built with SECURE=1 (a control token exists)


def control(body):
    """Lock, unlock or stop one tool: sign here (the key never leaves the home base), hand it to the hub, wait for the
    tag's answer. The app's buttons call this."""
    import urllib.request
    sys.path.insert(0, os.path.join(HERE, "..", "proto"))
    sys.path.insert(0, os.path.join(HERE, "..", "ai"))
    sys.path.insert(0, HERE)
    import tt_cmd
    from model_pkg import load_key
    from sendcmd import STATE, RESULT, hub_token
    tag, cmd = str(body.get("tag", "")), str(body.get("cmd", ""))
    if cmd not in ("lock", "unlock", "stop") or not re.fullmatch(r"[0-9A-F]{2}(:[0-9A-F]{2}){5}", tag):
        return 400, {"ok": 0, "error": "tag and cmd (lock, unlock or stop) needed"}
    if not HUB_URL:
        return 400, {"ok": 0, "error": "start the home base with --hub-url"}
    with lock:
        st = json.load(open(STATE)) if os.path.exists(STATE) else {}
        n = max(int(time.time()), st.get(tag, 0) + 1)
        st[tag] = n
        json.dump(st, open(STATE, "w"))
    w = tt_cmd.sign(load_key(), tag, cmd, 0, n)
    req = urllib.request.Request(HUB_URL + "/api/cmd", data=("addr=%s&cmd=%s" % (tag, w.hex())).encode(), method="POST",
                                 headers={"X-Token": hub_token(), "Content-Type": "text/plain"})
    r = json.load(urllib.request.urlopen(req, timeout=10))
    if not r.get("ok"):
        return 502, {"ok": 0, "error": "hub: %s" % r.get("error")}
    for _ in range(30):
        time.sleep(1)
        s = json.load(urllib.request.urlopen(HUB_URL + "/api/cmd", timeout=10))
        if not s["pending"]:
            last = s["last"]
            return 200, {"ok": last["ok"], "result": RESULT[0] if last["ok"] else RESULT.get(last["err"], "code %d" % last["err"])}
    return 504, {"ok": 0, "error": "no answer from the hub in 30 s"}


def newest_model():
    """The highest-numbered signed model package, as ai/model_pkg.py wrote it, or None."""
    try:
        names = [n for n in os.listdir(MODEL_DIR) if re.fullmatch(r"model-v\d+\.json", n)]
    except OSError:
        return None
    if not names:
        return None
    best = max(names, key=lambda n: int(n[7:-5]))
    return json.load(open(os.path.join(MODEL_DIR, best)))


def ident(it):
    """What makes a record the same record wherever it came from."""
    if it.get("k") == "e":
        return "e:%s:%s:%s" % (it.get("id"), it.get("type"), it.get("v"))
    return "s:%s:%s:%s:%s" % (it.get("seq"), it.get("trig"), it.get("run"), it.get("oc"))


NUMERIC = ("i", "up", "unix", "seq", "st", "trig", "oc", "flt", "run", "rpm", "soc", "tc", "id", "type", "v",
           "pid", "dur", "ravg", "rmax", "iavg", "ipk", "vmin", "cls", "app", "ver", "ok", "err", "ms")
TAG_NUMERIC = ("app", "rssi", "age", "nl", "ne", "trig", "oc", "flt", "run", "rpm", "rpmx", "ipk", "vb", "tc", "st", "flags", "soc",
               "cyc", "bvb", "btc", "tup", "link", "rxok", "rxbad", "seen", "fw", "mv")


def clean(d, numeric, text, maxlen=40):
    """Keep integers as integers and short strings as strings; drop anything else (stored data is rendered later)."""
    out = {}
    f = d.get("f")
    if isinstance(f, dict):                 # generic field values: integer ids and integers only
        out["f"] = {str(int(k)): int(v) for k, v in list(f.items())[:16]
                    if str(k).isdigit() and 0 < int(k) < 256 and isinstance(v, (int, float)) and not isinstance(v, bool) and v == v}
    for k in numeric:
        v = d.get(k)
        if isinstance(v, bool) or not isinstance(v, (int, float)) or v != v:
            continue
        out[k] = int(v)
    for k in text:
        v = d.get(k)
        if isinstance(v, str):
            out[k] = v[:maxlen]
    return out


def ingest(db, body, raw=None):
    via, hub = str(body.get("via", "?"))[:12], str(body.get("hub", "?"))[:32]
    items = [clean(it, NUMERIC, ("k", "a")) for it in (body.get("items") or []) if isinstance(it, dict)]
    tags = [clean(t, TAG_NUMERIC, ("addr", "name", "tool", "hub")) for t in (body.get("tags") or []) if isinstance(t, dict)]
    new = 0
    now = time.time()
    hub_up = body.get("hub_up")
    if isinstance(hub_up, int) and not isinstance(hub_up, bool):    # a hub posting by itself has no clock: date items from its uptime
        for it in items:
            if not it.get("unix") and isinstance(it.get("up"), int):
                it["unix"] = int(round(now - (hub_up - it["up"])))
    # bronze: every upload kept exactly as it arrived, one JSON line per upload, a file per UTC day
    os.makedirs(RAW_DIR, exist_ok=True)
    with open(os.path.join(RAW_DIR, time.strftime("%Y-%m-%d", time.gmtime(now)) + ".jsonl"), "a") as f:
        f.write(json.dumps({"received": now, "via": via, "hub": hub, "body": raw}) + "\n")
    with lock:
        for it in items:
            if it.get("k") == "g" and it.get("a") and it.get("app") and it.get("f"):
                cur = db.execute("insert or ignore into records values(?,?,?,?,?,?,?,?,?)",
                                 (hub, it["a"], it["app"], "%s:%s" % (it.get("seq"), it.get("up")), int(it.get("unix") or 0),
                                  it.get("seq"), json.dumps(it["f"]), via, now))
                new += cur.rowcount
                continue
            if it.get("k") == "c" and it.get("a"):         # a hub's record of delivering a signed command: keep it raw
                continue
            if it.get("k") == "m" and it.get("a"):         # a hub's record of installing a model on a tag
                cur = db.execute("insert or ignore into installs values(?,?,?,?,?,?,?,?,?)",
                                 (hub, it["a"], it.get("ver"), it.get("ok"), it.get("err"), it.get("ms"),
                                  int(it.get("unix") or 0), via, now))
                new += cur.rowcount
                continue
            if it.get("k") == "p" and it.get("a"):
                cur = db.execute("insert or ignore into pulls values(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                                 (hub, it["a"], it.get("pid"), int(it.get("unix") or 0), it.get("dur"), it.get("ravg"),
                                  it.get("rmax"), it.get("iavg"), it.get("ipk"), it.get("vmin"), it.get("tc"),
                                  it.get("cls"), via, now))
                new += cur.rowcount
                continue
            if it.get("k") not in ("s", "e") or not it.get("a"):
                continue
            cur = db.execute(
                "insert or ignore into items values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (hub, it["a"], it["k"], ident(it), int(it.get("unix") or 0), it.get("seq"), it.get("st"), it.get("trig"),
                 it.get("oc"), it.get("flt"), it.get("run"), it.get("rpm"), it.get("soc"), it.get("tc"),
                 it.get("type"), it.get("v"), via, now))
            new += cur.rowcount
        for t in tags:
            if isinstance(t, dict) and t.get("addr"):
                db.execute("insert into tags values(?,?,?,?,?,?,?) on conflict(addr) do update set hub=excluded.hub, "
                           "name=excluded.name, tool=excluded.tool, data=excluded.data, via=excluded.via, "
                           "updated=excluded.updated", (t["addr"], hub, t.get("name"), t.get("tool"), json.dumps(t), via, now))
        db.execute("insert into posts values(?,?,?,?,?)", (now, via, hub, len(items), new))
        db.commit()
    return {"ok": 1, "received": len(items), "new": new, "duplicates": len(items) - new}


def fleet(db):
    out = []
    with lock:
        tags = db.execute("select addr, hub, name, tool, data, via, updated from tags").fetchall()
        for addr, hub, name, tool, data, via, upd in tags:
            last = db.execute("select unix, trig, oc, flt, run, st, soc from items where a=? and k='s' "
                              "order by unix desc, received desc limit 1", (addr,)).fetchone()
            n = db.execute("select count(*), sum(k='e'), min(unix), max(unix) from items where a=?", (addr,)).fetchone()
            mix = dict(db.execute("select model_cls, count(*) from pulls where a=? group by model_cls", (addr,)).fetchall())
            d = json.loads(data or "{}")
            out.append(dict(addr=addr, hub=hub, name=name, tool=tool or name, via=via, updated=upd,
                            trig=last[1] if last else d.get("trig"), oc=last[2] if last else d.get("oc"),
                            flt=last[3] if last else d.get("flt"), run=last[4] if last else d.get("run"),
                            st=last[5] if last else d.get("st"), soc=last[6] if last else d.get("soc"),
                            last_unix=last[0] if last else None, records=n[0], events=n[1] or 0, first=n[2], newest=n[3],
                            pulls=sum(v for k, v in mix.items() if k is not None and k < 4),
                            heavy=mix.get(2, 0), jam=mix.get(3, 0)))
    return out


def events(db, limit=100):
    with lock:
        rows = db.execute("select i.unix, i.a, coalesce(t.tool, t.name, i.a), i.ev_type, i.ev_v, i.via, i.hub from items i "
                          "left join tags t on t.addr=i.a where i.k='e' order by i.unix desc limit ?", (limit,)).fetchall()
    return [dict(unix=r[0], a=r[1], tool=r[2], type=r[3], v=r[4], via=r[5], hub=r[6]) for r in rows]


def devices(db):
    """Devices of generic apps, their latest record decoded with the app's schema."""
    out = []
    with lock:
        rows = db.execute("select a, app_id, hub, count(*), max(unix) from records group by a, app_id").fetchall()
        for a, app_id, hub, n, newest in rows:
            last = db.execute("select fields, unix, via from records where a=? and app_id=? order by unix desc, received desc limit 1",
                              (a, app_id)).fetchone()
            app = APPS.get(app_id, {"name": "app-%d" % app_id, "title": "App %d" % app_id, "fields": [], "headline": []})
            vals = fd.named(app, {int(k): v for k, v in json.loads(last[0]).items()}) if last else {}
            name = db.execute("select name from tags where addr=?", (a,)).fetchone()
            out.append(dict(addr=a, name=name[0] if name else a, app=app["title"], app_id=app_id, hub=hub, records=n,
                            newest=newest, via=last[2] if last else None, headline=app["headline"], fields=vals))
    return out


def sources(db):
    with lock:
        rows = db.execute("select via, count(*), sum(n_items), sum(n_new), max(received) from posts group by via").fetchall()
    return [dict(via=r[0], posts=r[1], items=r[2], new=r[3], last=r[4]) for r in rows]


def page_ok(h):
    """A request from this home base's own pages, the hub's app or a script: Host and any Origin on the fixed lists."""
    if (h.get("Host") or "") not in OWN_HOSTS:
        return False
    o = h.get("Origin")
    return not o or o in ALLOWED_ORIGINS


class H(BaseHTTPRequestHandler):
    db: sqlite3.Connection
    timeout = 15                        # a client that stops sending cannot hold a thread

    def _send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        o = self.headers.get("Origin")
        if o and o in ALLOWED_ORIGINS:       # same-origin needs no header; only listed pages may read cross-origin
            self.send_header("Access-Control-Allow-Origin", o)
            self.send_header("Access-Control-Allow-Headers", "Content-Type, X-Token")
            self.send_header("Vary", "Origin")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self):
        self._send(204, b"")

    def do_GET(self):
        p = self.path.split("?")[0]
        if p in ("/", "/index.html"):
            self._send(200, open(os.path.join(HERE, "fleet.html"), "rb").read(), "text/html; charset=utf-8")
        elif p == "/app":
            page = open(os.path.join(HERE, "..", "app", "index.html"), "rb").read()
            self._send(200, page.replace(b"/*APPS*/[]", json.dumps(list(APPS.values())).encode()), "text/html; charset=utf-8")
        elif p == "/api/fleet":
            self._send(200, fleet(self.db))
        elif p == "/api/events":
            self._send(200, events(self.db))
        elif p == "/api/devices":
            self._send(200, devices(self.db))
        elif p == "/api/apps":
            self._send(200, list(APPS.values()))
        elif p == "/api/sources":
            self._send(200, sources(self.db))
        elif p == "/api/model":                      # the phone app and pushmodel.py carry this to a hub
            m = newest_model()
            self._send(200, m) if m else self._send(404, {"error": "no model yet: run ai/model_pkg.py"})
        elif p == "/api/hubble" and not page_ok(self.headers):   # locations: never to another site's page
            self._send(403, {"error": "not from this page"})
        elif p == "/api/hubble":                     # where Hubble heard each tag, newest first, and what it said
            with lock:
                rows = self.db.execute("select device, name, network, unix, seq, rssi, heard_lat, heard_lon, heard_acc, payload, msg "
                                       "from hubble order by unix desc limit 200").fetchall()
            self._send(200, [dict(device=r[0], name=r[1], network=r[2], unix=r[3], seq=r[4], rssi=r[5], heard_lat=r[6], heard_lon=r[7],
                                  heard_acc=r[8], payload=r[9], msg=json.loads(r[10]) if r[10] else None) for r in rows])
        elif p == "/api/installs":
            with lock:
                rows = self.db.execute("select a, ver, ok, err, ms, unix, hub, via from installs order by unix desc limit 200").fetchall()
            self._send(200, [dict(a=r[0], ver=r[1], ok=r[2], err=r[3], ms=r[4], unix=r[5], hub=r[6], via=r[7]) for r in rows])
        else:
            self._send(404, {"error": "no such path"})

    def do_POST(self):
        if self.path.split("?")[0] == "/api/control":
            import hmac
            # Fixed lists, never taken from the request: a page that rebinds its own name to this address still fails both.
            if (self.headers.get("Host") or "") not in OWN_HOSTS:
                return self._send(403, {"error": "unknown Host"})
            origin = self.headers.get("Origin")
            if origin and origin not in ALLOWED_ORIGINS:
                return self._send(403, {"error": "only the hub's app or the home base may send controls"})   # other web pages: no
            if not TOKEN and SECURE_HUB:            # a hub that wants a token never gets one on behalf of an unchecked caller
                return self._send(403, {"error": "this hub requires a token: start the home base with --token"})
            if not TOKEN and BIND_HOST not in ("127.0.0.1", "localhost", "::1"):
                return self._send(403, {"error": "listening beyond this machine: start the home base with --token to send controls"})
            if not TOKEN:                            # loopback bind: no token needed, but only from this machine or the local network
                ip = self.client_address[0]
                if not re.match(r"^(127\.|10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.|::1$)", ip):
                    return self._send(403, {"error": "from outside the local network, start the home base with --token"})
            elif not hmac.compare_digest(self.headers.get("X-Token") or "", TOKEN):
                return self._send(401, {"error": "token required"})
            if not (self.headers.get("Content-Type") or "").startswith("application/json"):
                return self._send(415, {"error": "send JSON"})   # a plain cross-site form cannot reach it
            try:
                body = json.loads(self.rfile.read(min(int(self.headers.get("Content-Length") or 0), 4096)))
            except ValueError:
                return self._send(400, {"error": "not JSON"})
            code, out = control(body if isinstance(body, dict) else {})
            return self._send(code, out)
        if self.path.split("?")[0] == "/api/hubble":     # from hubble_sync.py on this machine
            if TOKEN and not __import__("hmac").compare_digest(self.headers.get("X-Token") or "", TOKEN):
                return self._send(401, {"error": "token required"})
            if not TOKEN and not self.client_address[0].startswith("127."):
                return self._send(403, {"error": "from this machine only, or start with --token"})
            if not page_ok(self.headers) or not (self.headers.get("Content-Type") or "").startswith("application/json"):
                return self._send(403, {"error": "JSON from this machine's own scripts only"})
            try:
                recs = json.loads(self.rfile.read(min(int(self.headers.get("Content-Length") or 0), 2_000_000)))
            except ValueError:
                return self._send(400, {"error": "not JSON"})
            f = lambda v: float(v) if isinstance(v, (int, float)) and not isinstance(v, bool) else None
            new = 0
            with lock:
                for r in recs if isinstance(recs, list) else []:
                    if not isinstance(r, dict):
                        continue
                    m = r.get("msg") if isinstance(r.get("msg"), dict) else None
                    cur = self.db.execute("insert or ignore into hubble values(?,?,?,?,?,?,?,?,?,?,?,?)",
                                          (str(r.get("device", ""))[:40], str(r.get("name", ""))[:40], str(r.get("network", ""))[:12],
                                           f(r.get("unix")), f(r.get("seq")), f(r.get("rssi")), f(r.get("heard_lat")), f(r.get("heard_lon")),
                                           f(r.get("heard_acc")), re.sub(r"[^0-9a-f]", "", str(r.get("payload", "")))[:64],
                                           json.dumps({k: m[k] for k in m if isinstance(m[k], (int, float, bool)) or m[k] is None}) if m else None,
                                           time.time()))
                    new += cur.rowcount
                self.db.commit()
            return self._send(200, {"ok": 1, "new": new})
        if self.path.split("?")[0] != "/api/ingest":
            return self._send(404, {"error": "no such path"})
        if TOKEN and self.headers.get("X-Token") != TOKEN:
            return self._send(401, {"error": "token required"})
        try:
            n = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            return self._send(400, {"error": "bad Content-Length"})
        if n < 0 or n > 20_000_000:
            return self._send(413, {"error": "size out of range"})
        data = self.rfile.read(n)
        try:
            body = json.loads(data)
        except ValueError as e:
            os.makedirs(RAW_DIR, exist_ok=True)              # keep what failed, to fix the sender
            with open(os.path.join(RAW_DIR, "rejected.txt"), "ab") as f:
                f.write(b"%s %s\n" % (str(e).encode(), data[:20000]))
            return self._send(400, {"error": "not JSON: %s" % e})
        self._send(200, ingest(self.db, body, body))

    def log_message(self, format, *args):
        if "/api/ingest" in (args[0] if args else ""):
            super().log_message(format, *args)


def main():
    global RAW_DIR, TOKEN, MODEL_DIR, HUB_URL, SECURE_HUB, BIND_HOST
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8090)
    ap.add_argument("--host", default="127.0.0.1", help="0.0.0.0 to take uploads from phones on the network (then set a token)")
    ap.add_argument("--token", default=os.environ.get("FRL_HOMEBASE_TOKEN", ""), help="uploads must carry it in X-Token")
    ap.add_argument("--db", default=os.path.abspath("out/tool-telemetry/homebase.sqlite"))
    ap.add_argument("--raw", default=RAW_DIR, help="folder for the raw uploads (bronze), one JSON-lines file a day")
    ap.add_argument("--models", default=MODEL_DIR, help="folder of signed model packages (ai/model_pkg.py --out)")
    ap.add_argument("--hub-url", default="", help="the hub the app's Lock and Unlock buttons act through, e.g. http://<hub address>")
    a = ap.parse_args()
    RAW_DIR = a.raw
    MODEL_DIR = a.models
    HUB_URL = a.hub_url.rstrip("/")
    import socket
    names = {"localhost", "127.0.0.1", socket.gethostname()}
    try:                                     # this machine's own address on the local network
        u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); u.connect(("192.0.2.1", 9)); names.add(u.getsockname()[0]); u.close()
    except OSError:
        pass
    OWN_HOSTS.update("%s:%d" % (n, a.port) for n in names)
    ALLOWED_ORIGINS.update({HUB_URL, "http://10.0.0.3"} | {"http://" + h for h in OWN_HOSTS})
    ALLOWED_ORIGINS.discard("")
    sys.path.insert(0, HERE)
    from sendcmd import hub_token
    SECURE_HUB = bool(hub_token())
    os.makedirs(os.path.dirname(a.db), exist_ok=True)
    db = sqlite3.connect(a.db, check_same_thread=False)
    db.executescript(SCHEMA)
    H.db = db
    TOKEN = a.token
    BIND_HOST = a.host
    if a.host != "127.0.0.1" and not TOKEN:
        print("warning: listening on %s with no --token: anyone on that network can upload" % a.host, flush=True)
    print("home base on http://%s:%d  (db %s, token %s)" % (a.host, a.port, a.db, "set" if TOKEN else "off"), flush=True)
    ThreadingHTTPServer((a.host, a.port), H).serve_forever()


if __name__ == "__main__":
    main()
