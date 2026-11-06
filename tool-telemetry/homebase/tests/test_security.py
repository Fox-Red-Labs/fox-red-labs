"""M9: homebase CORS is never *, and /api/control needs a token when bound beyond loopback. Temp db only."""
import json, os, sqlite3, sys, threading, urllib.error, urllib.request
from http.server import ThreadingHTTPServer
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import server as S


def _up():
    db = sqlite3.connect(":memory:", check_same_thread=False)
    db.executescript(S.SCHEMA)
    S.H.db = db
    srv = ThreadingHTTPServer(("127.0.0.1", 0), S.H)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    port = srv.server_address[1]
    S.OWN_HOSTS.clear(); S.OWN_HOSTS.add("127.0.0.1:%d" % port)
    S.ALLOWED_ORIGINS.clear(); S.ALLOWED_ORIGINS.add("http://hub.example")
    return srv, "http://127.0.0.1:%d" % port


def _call(url, method="GET", headers=None, body=None):
    r = urllib.request.Request(url, data=body, headers=headers or {}, method=method)
    try:
        with urllib.request.urlopen(r, timeout=10) as x:
            return x.status, dict(x.headers), x.read()
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()


def test_default_bind_is_loopback():
    assert S.BIND_HOST == "127.0.0.1"


def test_no_wildcard_cors():
    srv, base = _up()
    try:
        code, h, _ = _call(base + "/api/fleet", headers={"Origin": "http://evil.example"})
        assert "Access-Control-Allow-Origin" not in h
        code, h, _ = _call(base + "/api/fleet", headers={"Origin": "http://hub.example"})
        assert h.get("Access-Control-Allow-Origin") == "http://hub.example"
        assert all(v != "*" for v in h.values())
    finally:
        srv.shutdown()


def test_control_refused_without_token_beyond_loopback():
    srv, base = _up()
    old = (S.BIND_HOST, S.TOKEN)
    try:
        S.BIND_HOST, S.TOKEN = "0.0.0.0", ""
        code, _, out = _call(base + "/api/control", "POST", {"Content-Type": "application/json"}, b"{}")
        assert code == 403 and b"--token" in out
        S.TOKEN = "tok"
        code, _, _ = _call(base + "/api/control", "POST", {"Content-Type": "application/json"}, b"{}")
        assert code == 401                                   # token set, but the request has none
    finally:
        S.BIND_HOST, S.TOKEN = old
        srv.shutdown()
