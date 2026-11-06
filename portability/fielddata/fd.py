"""Field data, computer side: app schemas, the generic frame and line (mirrors c/fd_frame.c), and the record envelope.
Standard library only. See README.md."""
import glob
import json
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
FD_VT = 0x21


def load_apps(folder=os.path.join(HERE, "apps")):
    """{app id: schema dict} from apps/*.json."""
    apps = {}
    for p in sorted(glob.glob(os.path.join(folder, "*.json"))):
        a = json.load(open(p))
        apps[a["id"]] = a
    return apps


def checksum(body):
    x = 0
    for c in body.encode():
        x ^= c
    return "%02X" % x


def make_line(app, n, fields):
    """fields: {field id: integer}. Returns '$FD,n=..,app=..,1=..*HH\\n'."""
    body = "FD,n=%d,app=%d" % (n & 0xFFFF, app) + "".join(",%d=%d" % (k, v) for k, v in fields.items())
    return "$%s*%s\n" % (body, checksum(body))


def line_for(app_schema, n, **named):
    """Build a line from field names: line_for(presence, 3, state='moving', range_cm=240)."""
    by_name = {f["name"]: f for f in app_schema["fields"]}
    out = {}
    for k, v in named.items():
        f = by_name[k]
        out[f["id"]] = f["values"].index(v) if f["type"] == "enum" and isinstance(v, str) else int(v)
    return make_line(app_schema["id"], n, out)


def parse_line(s):
    """Returns (app, n, {id: value}). Raises ValueError('framing'|'checksum'|'not fd')."""
    s = s.rstrip("\r\n")
    if len(s) < 7 or s[0] != "$" or s[-3] != "*":
        raise ValueError("framing")
    body = s[1:-3]
    try:
        want = int(s[-2:], 16)
    except ValueError:
        raise ValueError("framing")
    if int(checksum(body), 16) != want:
        raise ValueError("checksum")
    if body[:2] != "FD" or (len(body) > 2 and body[2] != ","):
        raise ValueError("not fd")
    app, n, fields = 0, -1, {}
    for part in body[3:].split(",") if len(body) > 3 else []:
        if "=" not in part:
            raise ValueError("framing")
        k, v = part.split("=", 1)
        try:
            vi = int(v, 10)
            if not v.lstrip("-").isdigit() or not -2**31 <= vi <= 2**31 - 1:
                raise ValueError
        except ValueError:
            continue
        if k == "n":
            n = vi & 0xFFFF
        elif k == "app":
            app = vi & 0xFF
        elif k.isdigit() and 0 < int(k) < 256:
            fields[int(k)] = vi
    if not app:
        raise ValueError("framing")
    return app, n, dict(list(fields.items())[:16])


def _width(v):
    return 1 if -128 <= v <= 127 else 2 if -32768 <= v <= 32767 else 4


def build_ad(app, seq, fields):
    """The manufacturer-data AD structure, as fd_build_ad."""
    body = struct.pack("<HBBBBB", 0xFFFF, ord("F"), ord("R"), FD_VT, app, seq & 0xFF)
    for k, v in fields.items():
        w = _width(v)
        body += bytes([k, w]) + int(v).to_bytes(w, "little", signed=True)
    return bytes([len(body) + 1, 0xFF]) + body


def decode_msd(m):
    """Manufacturer data from the company ID on. Returns (app, seq, {id: value}) or None."""
    if len(m) < 7 or m[:2] != b"\xff\xff" or m[2:4] != b"FR" or m[4] != FD_VT:
        return None
    app, seq, i, out = m[5], m[6], 7, {}
    while i + 2 <= len(m):
        k, w = m[i], m[i + 1]
        if w not in (1, 2, 4) or i + 2 + w > len(m):
            return None
        out[k] = int.from_bytes(m[i + 2:i + 2 + w], "little", signed=True)
        i += 2 + w
    return app, seq, out


def named(app_schema, fields):
    """{id: raw} -> {name: value}, enums as their word."""
    out = {}
    for f in app_schema["fields"]:
        if f["id"] in fields:
            v = fields[f["id"]]
            out[f["name"]] = f["values"][v] if f["type"] == "enum" and 0 <= v < len(f["values"]) else v
    return out


def envelope(app_schema, device, hub, kind, ident, event_time, ingest_time, source, fields, label=None):
    """The one record shape every app's data is stored and exported in (README.md, "The record")."""
    return {"app": app_schema["name"], "app_id": app_schema["id"], "schema": app_schema["schema"], "device": device,
            "hub": hub, "kind": kind, "id": ident, "event_time": event_time, "ingest_time": ingest_time,
            "source": source, "fields": fields, "label": label}
