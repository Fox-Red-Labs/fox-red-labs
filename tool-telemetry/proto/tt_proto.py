"""Tool telemetry wire formats, version 1, host side. Mirrors tt_proto.c; see ../PROTOCOL.md.

Used by the simulators, the home base, the tests, and anything on a computer that reads a tag or a gateway."""
import struct

COMPANY = 0xFFFF
VT_LEGACY, VT_EXT = 0x11, 0x12
EV_NAMES = {1: "trig", 2: "oc", 3: "flt", 4: "toolboot", 5: "tagboot"}
EV_CODES = {"trig": 1, "oc": 2, "flt": 3, "boot": 4}
F_RUN, F_FAULT, F_BATLOW, F_LINK, F_BAT, F_EXT = 1, 2, 4, 8, 16, 32


def checksum(body: str) -> str:
    x = 0
    for c in body.encode():
        x ^= c
    return "%02X" % x


def make_line(typ: str, n: int, **kv) -> str:
    """'$TS,n=1,trig=3*HH\\n'. Keys keep the order given."""
    body = "%s,n=%d" % (typ, n & 0xFFFF)
    if kv:
        body += "," + ",".join("%s=%s" % (k, v) for k, v in kv.items())
    return "$%s*%s\n" % (body, checksum(body))


def new_tool():
    return dict(id="", trig=0, run=0, oc=0, flt=0, rpm=0, rpmx=0, ipk=0, vb=0, tc=0, st=0,
                has_bat=0, soc=0xFF, cyc=0, bvb=0, btc=0)


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def parse_line(s: str, tool: dict):
    """Returns (line dict, new tool dict). Raises ValueError('framing'|'checksum')."""
    s = s.rstrip("\r\n")
    if len(s) < 7 or len(s) > 160 or s[0] != "$" or s[-3] != "*":
        raise ValueError("framing")
    body, hh = s[1:-3], s[-2:]
    try:
        want = int(hh, 16)
    except ValueError:
        raise ValueError("framing")
    if int(checksum(body), 16) != want:
        raise ValueError("checksum")
    typ = body[:2]
    if len(body) > 2 and body[2] != ",":
        raise ValueError("framing")
    line = dict(type=typ, n=-1, ev=0, v=0, cmd="", duty=0)
    if typ[1:2] == "P":
        line["pull"] = dict(id=0, dur10=0, ravg=0, rmax=0, iavg=0, ipk=0, vmin=0, tc=0, cls=255)
    t = dict(tool)
    rest = body[3:]
    for part in rest.split(",") if rest else []:
        if "=" not in part:
            raise ValueError("framing")
        k, v = part.split("=", 1)
        if k == "id":
            t["id"] = v[:8]
            continue
        if k == "ev":
            line["ev"] = EV_CODES.get(v, 0)
            continue
        if k == "cmd":
            line["cmd"] = v[:7]
            continue
        try:
            num = int(v, 10)
            if not (v.lstrip("-").isdigit()) or abs(num) > 0xFFFFFFFF:
                raise ValueError
        except ValueError:
            continue
        if k == "n":
            line["n"] = num & 0xFFFF
        elif k == "v":
            line["v"] = _clamp(num, 0, 65535)
        elif k == "duty":
            line["duty"] = _clamp(num, 0, 100)
        elif typ[1] == "P":
            pl = line.setdefault("pull", dict(id=0, dur10=0, ravg=0, rmax=0, iavg=0, ipk=0, vmin=0, tc=0, cls=255))
            if k == "dur":
                q = num + 5                     # same as C: (num + 5) / 10, truncating toward zero
                pl["dur10"] = _clamp(abs(q) // 10 * (1 if q >= 0 else -1), 0, 65535)
            elif k in ("ravg", "rmax", "iavg", "ipk", "vmin"):
                pl[k] = _clamp(num, 0, 65535)
            elif k == "tc":
                pl["tc"] = _clamp(num, -128, 127)
            elif k == "cls":
                pl["cls"] = 255 if num < 0 or num > 3 else num
        elif typ[1] == "B":
            if k == "soc":
                t["soc"], t["has_bat"] = _clamp(num, 0, 100), 1
            elif k == "cyc":
                t["cyc"], t["has_bat"] = _clamp(num, 0, 65535), 1
            elif k == "vb":
                t["bvb"], t["has_bat"] = _clamp(num, 0, 65535), 1
            elif k == "tc":
                t["btc"], t["has_bat"] = _clamp(num, -128, 127), 1
        elif k in ("trig", "run"):
            t[k] = num & 0xFFFFFFFF
        elif k in ("oc", "flt", "ipk", "vb"):
            t[k] = _clamp(num, 0, 65535)
        elif k == "rpm":
            t["rpm"] = _clamp(num, 0, 65535)
            t["rpmx"] = max(t["rpmx"], t["rpm"])
        elif k == "tc":
            t["tc"] = _clamp(num, -128, 127)
        elif k == "st":
            t["st"] = num & 3
    return line, t


def flags(t, link_alive, ext_on):
    f = 0
    if t["st"] == 1:
        f |= F_RUN
    if t["st"] == 2:
        f |= F_FAULT
    if t["has_bat"]:
        f |= F_BAT
        if t["soc"] < 20:
            f |= F_BATLOW
    if link_alive:
        f |= F_LINK
    if ext_on:
        f |= F_EXT
    return f


def build_legacy(seq, t, fl):
    trig = min(t["trig"], 0xFFFFFF)
    msd = struct.pack("<HBBBB", COMPANY, ord("F"), ord("R"), VT_LEGACY, seq & 0xFF)
    msd += struct.pack("<I", trig)[:3] + struct.pack("<HHIBBb", t["oc"], t["flt"], t["run"], fl,
                                                      t["soc"] if t["has_bat"] else 0xFF, t["tc"])
    return bytes([2, 1, 6, len(msd) + 1, 0xFF]) + msd


PULL_FMT = "<HHHHHHHbB"
PULL_KEYS = ("id", "dur10", "ravg", "rmax", "iavg", "ipk", "vmin", "tc", "cls")


def build_ext(seq, t, events=(), tag=None, name="", pulls=()):
    """events: list of dicts id,type,t,v (newest first). pulls: dicts with PULL_KEYS (newest first).
    tag: dict up,link,rx_ok,rx_bad,fw and optionally mv (model version)."""
    out = bytes([2, 1, 6])
    name = name[:16]
    if name:
        out += bytes([len(name) + 1, 0x09]) + name.encode()
    msd = struct.pack("<HBBBB", COMPANY, ord("F"), ord("R"), VT_EXT, seq & 0xFF)
    msd += bytes([1, 12]) + struct.pack("<IHHI", t["trig"], t["oc"], t["flt"], t["run"])
    msd += bytes([2, 10]) + struct.pack("<HHHHbB", t["rpm"], t["rpmx"], t["ipk"], t["vb"], t["tc"], t["st"])
    if t["has_bat"]:
        msd += bytes([3, 6]) + struct.pack("<BHHb", t["soc"], t["cyc"], t["bvb"], t["btc"])
    if t["id"]:
        msd += bytes([4, len(t["id"])]) + t["id"].encode()
    events = list(events)[:8]
    if events:
        msd += bytes([5, 9 * len(events)]) + b"".join(
            struct.pack("<HBIH", e["id"], e["type"], e["t"], e["v"]) for e in events)
    pulls = list(pulls)[:3]
    if pulls:
        msd += bytes([7, 16 * len(pulls)]) + b"".join(struct.pack(PULL_FMT, *(p[k] for k in PULL_KEYS)) for p in pulls)
    if tag is not None:
        msd += bytes([6, 12]) + struct.pack("<IHHBBH", tag["up"], tag["link"], tag["rx_ok"], tag["rx_bad"], tag["fw"], tag.get("mv", 0))
    return out + bytes([len(msd) + 1, 0xFF]) + msd


def decode_msd(m: bytes):
    """Manufacturer data starting at the company ID. Returns a dict or None."""
    if len(m) < 6 or struct.unpack_from("<H", m)[0] != COMPANY or m[2:4] != b"FR":
        return None
    vt, seq = m[4], m[5]
    f: dict = dict(vt=vt, seq=seq)
    p = m[6:]
    if vt == VT_LEGACY:
        if len(p) < 14:
            return None
        trig = p[0] | p[1] << 8 | p[2] << 16
        oc, flt, run, fl, soc, tc = struct.unpack_from("<HHIBBb", p, 3)
        f.update(trig=trig, oc=oc, flt=flt, run=run, flags=fl, soc=soc, tc=tc,
                 st=2 if fl & F_FAULT else 1 if fl & F_RUN else 0)
        return f
    if vt != VT_EXT:
        return None
    i = 0
    while len(p) - i >= 2:
        ty, ln = p[i], p[i + 1]
        v = p[i + 2:i + 2 + ln]
        if len(v) < ln:
            return None
        if ty == 1 and ln >= 12:
            f.update(zip(("trig", "oc", "flt", "run"), struct.unpack_from("<IHHI", v)))
        elif ty == 2 and ln >= 10:
            f.update(zip(("rpm", "rpmx", "ipk", "vb", "tc", "st"), struct.unpack_from("<HHHHbB", v)))
        elif ty == 3 and ln >= 6:
            f.update(zip(("soc", "cyc", "bvb", "btc"), struct.unpack_from("<BHHb", v)))
        elif ty == 4:
            f["tool"] = v[:8].decode("ascii", "replace")
        elif ty == 5:
            f["events"] = [dict(zip(("id", "type", "t", "v"), struct.unpack_from("<HBIH", v, 9 * k)))
                           for k in range(min(ln // 9, 8))]
        elif ty == 7:
            f["pulls"] = [dict(zip(PULL_KEYS, struct.unpack_from(PULL_FMT, v, 16 * k))) for k in range(min(ln // 16, 3))]
        elif ty == 6 and ln >= 10:
            f.update(zip(("tup", "link", "rxok", "rxbad", "fw"), struct.unpack_from("<IHHBB", v)))
            f["mv"] = struct.unpack_from("<H", v, 10)[0] if ln >= 12 else 0
        i += 2 + ln
    return f


def decode_adv(data: bytes):
    """Walk AD structures. Returns a dict (with 'name' if present) or None if no tool-telemetry data."""
    i, name = 0, None
    found: "dict | None" = None
    while i < len(data):
        ln = data[i]
        if ln == 0 or i + 1 + ln > len(data):
            break
        ty, v = data[i + 1], data[i + 2:i + 1 + ln]
        if ty in (8, 9):
            name = v.decode("utf-8", "replace")
        elif ty == 0xFF and found is None:
            found = decode_msd(v)
        i += 1 + ln
    if found is not None and name:
        found["name"] = name
    return found
