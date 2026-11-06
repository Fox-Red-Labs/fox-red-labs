#!/usr/bin/env python3
"""Weather station to Home Assistant: reads the CC1354P10 receiver's packets (serial port, or a log file another
program is writing) and posts temperature, humidity, light and signal strength as four sensor entities through
HA's REST API (/api/states), every N seconds. Give the address and a long-lived token with --url and --token
(or HA_URL and HA_TOKEN); without them it uses tools/ha.py, which reads this bench's Claude Code settings. Nothing secret is printed.
usage: ha_bridge.py --port /dev/cu.usbmodemLS45013A1 [--url http://homeassistant.local:8123 --token <token>] [--every 10]
       ha_bridge.py --follow <rx.log> [--every 10]
Entities are not stored by an integration, so they disappear when Home Assistant restarts until the next post."""
import argparse, os, re, sys, time, termios, select
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../tools"))
import json, urllib.request, urllib.error  # noqa: E402

import ipaddress, urllib.parse  # noqa: E402

class _NoRedirect(urllib.request.HTTPRedirectHandler):
    """Never follow a redirect: it would carry the token to whatever host the reply names."""
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise urllib.error.HTTPError(req.full_url, code, f"redirect to {newurl} refused (token not forwarded)", headers, fp)

def _local(host):
    if host in ("localhost",) or host.endswith((".local", ".lan", ".ts.net")):
        return True
    try:
        ip = ipaddress.ip_address(host)
        # private, loopback, or the 100.64.0.0/10 range mesh VPNs use (their tunnels are encrypted end to end)
        return ip.is_private or ip.is_loopback or ip in ipaddress.ip_network("100.64.0.0/10")
    except ValueError:
        return False

class _Direct:
    """The same req() as tools/ha.py, for a given address and token. https anywhere; plain http only to a
    local-network address (homeassistant.local, a private IP), since http sends the token unencrypted."""
    def __init__(self, url, token):
        u = urllib.parse.urlparse(url)
        if u.scheme not in ("http", "https") or not u.hostname:
            sys.exit(f"--url must be http(s)://host[:port], got {url!r}")
        if u.scheme == "http" and not _local(u.hostname):
            sys.exit(f"refusing plain http to {u.hostname}: the token would cross the network unencrypted; use https")
        self.base, self.auth = url.rstrip("/"), "Bearer " + token
        self.opener = urllib.request.build_opener(_NoRedirect)
    def req(self, method, path, data=None):
        r = urllib.request.Request(self.base + path, method=method, headers={"Authorization": self.auth, "Content-Type": "application/json"},
                                   data=json.dumps(data).encode() if data is not None else None)
        t0 = time.time()
        with self.opener.open(r, timeout=30) as resp: body = resp.read().decode()
        return round((time.time() - t0) * 1000), json.loads(body) if body else None
ha = None

ENT = {
    "temp": ("sensor.frl_radio_temperature", "Fox Red Labs radio temperature", "°C", "temperature"),
    "rh": ("sensor.frl_radio_humidity", "Fox Red Labs radio humidity", "%", "humidity"),
    "lux": ("sensor.frl_radio_light", "Fox Red Labs radio light", "lx", "illuminance"),
    "rssi": ("sensor.frl_radio_signal", "Fox Red Labs radio signal", "dBm", "signal_strength"),
}
PKT = re.compile(r"pkt \d+ len=\d+ rssi=(-?\d+) status=(0x..) data=([0-9a-f]+)")
TXT = re.compile(r"n=(\d+) t=(-?[\d.]+)C rh=([\d.]+)% lux=([\d.]+)")

def lines_from_port(port):
    fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0; a[0] = 0; a[1] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a); part = b""
    while True:
        r, _, _ = select.select([fd], [], [], 1.0)
        if r:
            try: part += os.read(fd, 4096)
            except BlockingIOError: pass
            while b"\n" in part:
                l, part = part.split(b"\n", 1); yield l.decode("utf8", "replace")

def lines_from_file(path):
    f = open(path); f.seek(0, 2)
    while True:
        l = f.readline()
        if l: yield l
        else: time.sleep(0.5)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--port"); ap.add_argument("--follow"); ap.add_argument("--every", type=float, default=10)
    ap.add_argument("--url", default=os.environ.get("HA_URL"))
    ap.add_argument("--token", default=os.environ.get("HA_TOKEN"), help="prefer HA_TOKEN: a command-line token is visible to other users in the process list")
    a = ap.parse_args()
    global ha
    if a.url and a.token: ha = _Direct(a.url, a.token)
    else:
        import ha as _ha  # tools/ha.py
        ha = _ha
    src = lines_from_port(a.port) if a.port else lines_from_file(a.follow)
    last = 0.0; posted = 0
    for l in src:
        m = PKT.search(l)
        if not m or m.group(2) != "0x00": continue
        try:
            t = TXT.search(bytes.fromhex(m.group(3)).decode("ascii", "replace"))
        except ValueError:   # a cut-off line, as the serial bridge replays when the port opens
            continue
        if not t or time.time() - last < a.every: continue
        last = time.time()
        vals = {"temp": t.group(2), "rh": t.group(3), "lux": t.group(4), "rssi": m.group(1)}
        ms_total = 0
        for k, v in vals.items():
            eid, name, unit, dc = ENT[k]
            ms, _ = ha.req("POST", "/api/states/" + eid, {"state": v, "attributes": {
                "friendly_name": name, "unit_of_measurement": unit, "device_class": dc, "state_class": "measurement",
                "seq": int(t.group(1)), "source": "LP-MSPM0G5187 + BP-CC1407P-9-FE at 915 MHz, received by LP-CC1354P10-1"}})
            ms_total += ms
        posted += 1
        print(f"{time.strftime('%H:%M:%S')} posted seq={t.group(1)} t={vals['temp']} rh={vals['rh']} lux={vals['lux']} rssi={vals['rssi']} ({ms_total} ms)", flush=True)

if __name__ == "__main__":
    main()
