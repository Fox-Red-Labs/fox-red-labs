#!/usr/bin/env python3
"""Thermostat hub: a small MQTT 3.1/3.1.1 broker plus a forwarder to Home Assistant. Python 3 stdlib only.

  hub.py [port] [--csv PATH] [--no-ha] [-v]   run the broker on 0.0.0.0:<port> (default 1883)
  hub.py --selftest                           broker + raw-socket client on loopback, no board, no Home Assistant

Broker: CONNECT, SUBSCRIBE (+ and # wildcards), UNSUBSCRIBE, PUBLISH QoS 0 and 1 in, retained messages, PINGREQ, DISCONNECT.
Delivery to subscribers is QoS 0. A QoS 2 PUBLISH is logged and the connection is closed; a QoS 2 SUBSCRIBE is granted at 1.
Forwarder: every thermostat/presence|setpoint|temp message is POSTed to Home Assistant (/api/states/<entity>) through
tools/ha.py req(). thermostat/seq closes an update: the CSV gets one row
  seq, t_mqtt_seq_arrived (time.time() when the seq message reached the hub), t_ha_done (time.time() after the REST calls
  queued before it returned), ha_rest_ms (sum of those REST calls' own durations).
"""
import asyncio, csv, os, sys, time, queue, threading, tempfile, socket, struct, json, argparse, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
ENTITIES = {  # topic -> (entity_id, extra attributes)
    "thermostat/presence": ("binary_sensor.fxl_therm_presence", {"friendly_name": "Fox Red Labs thermostat presence", "device_class": "occupancy"}),
    "thermostat/setpoint": ("sensor.fxl_therm_setpoint", {"friendly_name": "Fox Red Labs thermostat setpoint", "device_class": "temperature", "unit_of_measurement": "°C"}),
    "thermostat/temp": ("sensor.fxl_therm_temp", {"friendly_name": "Fox Red Labs thermostat temperature", "device_class": "temperature", "unit_of_measurement": "°C"}),
}
VERBOSE = False
def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)

# ---------------------------------------------------------------- Home Assistant side
class Forwarder:
    """One worker thread keeps REST calls in order; thermostat/seq enqueues a marker that writes the CSV row."""
    def __init__(self, csv_path, ha=True):
        self.q = queue.Queue(); self.ha = ha; self.req = None
        if ha:
            spec = importlib.util.spec_from_file_location("fxl_ha", os.path.join(HERE, "..", "..", "tools", "ha.py"))
            mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod); self.req = mod.req
        self.f = open(csv_path, "w", newline=""); self.w = csv.writer(self.f)
        self.w.writerow(["seq", "t_mqtt_seq_arrived", "t_ha_done", "ha_rest_ms"]); self.f.flush()
        self.acc_ms = 0; self.rows = 0
        threading.Thread(target=self.run, daemon=True).start()
    def state(self, topic, payload):
        if topic in ENTITIES: self.q.put(("state", topic, payload))
    def seq(self, payload, t_arrived):
        self.q.put(("seq", payload, t_arrived))
    def run(self):
        while True:
            job = self.q.get()
            if job[0] == "state":
                ent, attrs = ENTITIES[job[1]]; val = job[2]
                if ent.startswith("binary_sensor"): val = "on" if val.upper() == "ON" else "off"
                if self.ha:
                    try:
                        ms, _ = self.req("POST", "/api/states/" + ent, {"state": val, "attributes": attrs}); self.acc_ms += ms
                    except Exception as e:
                        log("HA POST failed", ent, repr(e)[:120])
            else:
                _, p, t_arr = job
                t_done = time.time()
                try: seq = int(p)
                except ValueError: log("seq payload not an integer, no CSV row:", p); self.acc_ms = 0; continue
                self.w.writerow([seq, "%.6f" % t_arr, "%.6f" % t_done, self.acc_ms]); self.f.flush(); self.rows += 1
                if VERBOSE: log("seq", seq, "ha_rest_ms", self.acc_ms)
                self.acc_ms = 0

# ---------------------------------------------------------------- broker
def enc_len(n):
    out = bytearray()
    while True:
        b = n % 128; n //= 128
        out.append(b | (0x80 if n else 0))
        if not n: return bytes(out)
def packet(first, body): return bytes([first]) + enc_len(len(body)) + body
def enc_str(s): b = s.encode(); return struct.pack(">H", len(b)) + b

def topic_match(flt, topic):
    f = flt.split("/"); t = topic.split("/")
    if topic.startswith("$") and f[0] in ("+", "#"): return False
    for i, lv in enumerate(f):
        if lv == "#": return True
        if i >= len(t): return False
        if lv != "+" and lv != t[i]: return False
    return len(f) == len(t)

class Broker:
    def __init__(self, fwd):
        self.fwd = fwd; self.retained = {}; self.clients = set()
    async def read_packet(self, r):
        b = await r.readexactly(1); mult, n = 1, 0
        while True:
            c = (await r.readexactly(1))[0]; n += (c & 127) * mult
            if not c & 128: break
            mult *= 128
            if mult > 128 ** 3: raise ValueError("bad remaining length")
        if n > 1 << 20: raise ValueError("packet too large")
        return b[0], (await r.readexactly(n) if n else b"")
    def deliver(self, topic, payload, retain_flag=0):
        for c in list(self.clients):
            if any(topic_match(f, topic) for f in c.filters):
                try: c.w.write(packet(0x30 | retain_flag, enc_str(topic) + payload))
                except Exception: pass
    async def handle(self, r, w):
        peer = w.get_extra_info("peername"); cli = type("C", (), {})(); cli.w = w; cli.filters = {}; cli.id = "?"
        try:
            first, body = await self.read_packet(r)
            if first >> 4 != 1: log(peer, "first packet is not CONNECT"); return
            nlen = struct.unpack(">H", body[:2])[0]; name = body[2:2 + nlen]; i = 2 + nlen
            level = body[i]; flags = body[i + 1]; i += 4  # skip keepalive
            if name not in (b"MQTT", b"MQIsdp"):
                w.write(packet(0x20, b"\x00\x01")); await w.drain(); return
            cl = struct.unpack(">H", body[i:i + 2])[0]; cli.id = body[i + 2:i + 2 + cl].decode("utf8", "replace")
            log(peer, "CONNECT", cli.id, "proto", name.decode(), level, "will_qos", (flags >> 3) & 3)
            w.write(packet(0x20, b"\x00\x00")); await w.drain(); self.clients.add(cli)
            while True:
                first, body = await self.read_packet(r); typ = first >> 4
                if typ == 3:
                    qos = (first >> 1) & 3; retain = first & 1; tl = struct.unpack(">H", body[:2])[0]
                    topic = body[2:2 + tl].decode("utf8", "replace"); i = 2 + tl
                    if qos == 2: log(peer, cli.id, "PUBLISH QoS 2 to", topic, "refused, closing"); return
                    if qos == 1: pid = body[i:i + 2]; i += 2; w.write(packet(0x40, pid))
                    payload = body[i:]
                    if retain:
                        if payload: self.retained[topic] = payload
                        else: self.retained.pop(topic, None)
                    self.deliver(topic, payload)
                    if topic == "thermostat/seq": self.fwd.seq(payload.decode("utf8", "replace").strip(), time.time())
                    elif topic in ENTITIES: self.fwd.state(topic, payload.decode("utf8", "replace").strip())
                    if VERBOSE: log("PUBLISH", cli.id, topic, payload[:60])
                elif typ == 8:
                    pid = body[:2]; i = 2; granted = bytearray(); new = []
                    while i < len(body):
                        fl = struct.unpack(">H", body[i:i + 2])[0]; flt = body[i + 2:i + 2 + fl].decode("utf8", "replace"); q = body[i + 2 + fl]; i += 3 + fl
                        if q == 2: log(peer, cli.id, "SUBSCRIBE", flt, "asked QoS 2, granted 1")
                        granted.append(min(q, 1)); cli.filters[flt] = 1; new.append(flt)
                    w.write(packet(0x90, pid + bytes(granted)))
                    for flt in new:
                        for t, p in list(self.retained.items()):
                            if topic_match(flt, t): w.write(packet(0x31, enc_str(t) + p))
                elif typ == 10:
                    pid = body[:2]; i = 2
                    while i < len(body):
                        fl = struct.unpack(">H", body[i:i + 2])[0]; cli.filters.pop(body[i + 2:i + 2 + fl].decode("utf8", "replace"), None); i += 2 + fl
                    w.write(packet(0xB0, pid))
                elif typ == 12: w.write(packet(0xD0, b""))
                elif typ == 14: return
                elif typ in (4, 5, 6, 7, 11, 13): pass
                else: log(peer, cli.id, "unsupported packet type", typ, "closing"); return
                await w.drain()
        except (asyncio.IncompleteReadError, ConnectionError): pass
        except Exception as e: log(peer, cli.id, "error", repr(e))
        finally:
            self.clients.discard(cli); log(peer, cli.id, "closed")
            try: w.close()
            except Exception: pass

# ---------------------------------------------------------------- self test
class RawClient:
    def __init__(self, port, cid):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=3); self.cid = cid
        self.s.sendall(packet(0x10, enc_str("MQTT") + b"\x04\x02\x00\x3c" + enc_str(cid)))
        assert self.read() == (0x20, b"\x00\x00"), "CONNACK"
    def read(self):
        h = self.s.recv(1)
        if not h: return None
        mult, n = 1, 0
        while True:
            c = self.s.recv(1)[0]; n += (c & 127) * mult
            if not c & 128: break
            mult *= 128
        body = b""
        while len(body) < n: body += self.s.recv(n - len(body))
        return h[0], body
    def pub(self, topic, payload, retain=False, qos=0):
        body = enc_str(topic) + (b"\x00\x07" if qos else b"") + payload.encode()
        self.s.sendall(packet(0x30 | (qos << 1) | int(retain), body))
        if qos == 1: assert self.read() == (0x40, b"\x00\x07"), "PUBACK"
    def sub(self, flt, qos=0):
        self.s.sendall(packet(0x82, b"\x00\x09" + enc_str(flt) + bytes([qos]))); return self.read()
    def msg(self):
        f, b = self.read(); assert f >> 4 == 3, f; tl = struct.unpack(">H", b[:2])[0]; return b[2:2 + tl].decode(), b[2 + tl:].decode(), f & 1

def selftest():
    csvp = os.path.join(tempfile.mkdtemp(prefix="hubtest"), "hub.csv")
    res = {}
    async def main():
        fwd = Forwarder(csvp, ha=False); br = Broker(fwd)
        srv = await asyncio.start_server(br.handle, "127.0.0.1", 0); port = srv.sockets[0].getsockname()[1]
        def client_work():
            ok = []
            def check(name, cond): ok.append(bool(cond)); print(("PASS " if cond else "FAIL ") + name, flush=True)
            live = RawClient(port, "live"); sa = live.sub("thermostat/#"); check("SUBACK granted 0", sa == (0x90, b"\x00\x09\x00"))
            pub = RawClient(port, "fw")
            for t in ("binary_sensor/fxl_therm_presence", "sensor/fxl_therm_setpoint", "sensor/fxl_therm_temp"):
                pub.pub("homeassistant/%s/config" % t, '{"name":"%s"}' % t, retain=True)
            pub.pub("thermostat/presence", "ON", retain=True); pub.pub("thermostat/setpoint", "21.5", retain=True)
            pub.pub("thermostat/temp", "20.8", retain=True); pub.pub("thermostat/seq", "1234", retain=True, qos=1)
            got = [live.msg() for _ in range(4)]
            check("live subscriber got 4 states in order", [g[0] for g in got] == ["thermostat/presence", "thermostat/setpoint", "thermostat/temp", "thermostat/seq"])
            check("payloads", [g[1] for g in got] == ["ON", "21.5", "20.8", "1234"])
            late = RawClient(port, "late"); late.sub("thermostat/+")
            lg = sorted(late.msg() for _ in range(4)); check("late subscriber got 4 retained via +", [(x[0], x[1], x[2]) for x in lg] == sorted([(g[0], g[1], 1) for g in got]))
            disc = RawClient(port, "disc"); disc.sub("homeassistant/#")
            check("retained discovery via # (3 configs)", len({disc.msg()[0] for _ in range(3)}) == 3)
            pub.s.sendall(b"\xc0\x00"); check("PINGRESP", pub.read() == (0xD0, b""))
            pub.pub("thermostat/seq", "", retain=True)
            late2 = RawClient(port, "late2"); late2.sub("thermostat/seq"); late2.s.settimeout(0.5)
            try: late2.read(); check("empty retained payload clears", False)
            except socket.timeout: check("empty retained payload clears", True)
            q2 = RawClient(port, "q2"); q2.s.sendall(packet(0x34, enc_str("thermostat/seq") + b"\x00\x01x"))
            check("QoS 2 PUBLISH refused (connection closed)", q2.read() is None)
            pub.s.sendall(b"\xe0\x00")
            res["ok"] = all(ok)
        await asyncio.get_running_loop().run_in_executor(None, client_work)
        await asyncio.sleep(0.2); srv.close()
        rows = list(csv.DictReader(open(csvp)))
        good = len(rows) == 1 and rows[0]["seq"] == "1234" and float(rows[0]["t_ha_done"]) >= float(rows[0]["t_mqtt_seq_arrived"]) and rows[0]["ha_rest_ms"] == "0"
        print(("PASS " if good else "FAIL ") + "CSV row for seq 1234: " + json.dumps(rows), flush=True)
        res["ok"] = res.get("ok") and good
    asyncio.run(main())
    print("SELFTEST", "PASS" if res.get("ok") else "FAIL"); return 0 if res.get("ok") else 1

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("port", nargs="?", type=int, default=1883)
    ap.add_argument("--csv", default="hub.csv"); ap.add_argument("--no-ha", action="store_true")
    ap.add_argument("--selftest", action="store_true"); ap.add_argument("-v", action="store_true")
    a = ap.parse_args(); global VERBOSE; VERBOSE = a.v
    if a.selftest: sys.exit(selftest())
    fwd = Forwarder(a.csv, ha=not a.no_ha); br = Broker(fwd)
    async def run():
        srv = await asyncio.start_server(br.handle, "0.0.0.0", a.port)
        log("hub listening on 0.0.0.0:%d, csv %s, HA %s" % (a.port, a.csv, "off" if a.no_ha else "on")); await srv.serve_forever()
    try: asyncio.run(run())
    except KeyboardInterrupt: pass
if __name__ == "__main__": main()
