#!/usr/bin/env python3
"""The 13 bytes a tool tag sends through Hubble (Bluetooth to satellite or to phones): who it is, whether it works,
how much it has been used, its battery, and the last site it was at. Standard library only.
Hubble takes payloads of 0, 4, 9 or 13 bytes (HUBBLE_BLE_MAX_DATA_LEN is 13 in its SDK); Hubble adds the device
identity and a sequence number itself, so none of these bytes are spent on them.

    byte 0      layout (high 4 bits, 1 today) and flags (low 4 bits): fault now, a jam or overcurrent since the last
                report, battery low, used in the last 24 hours
    bytes 1-3   trigger pulls, lifetime, 24 bits
    bytes 4-5   motor run time, lifetime, tenths of an hour (6,553 hours)
    byte 6      overcurrent trips, lifetime, stops at 255
    byte 7      battery charge, percent (255 unknown)
    byte 8      battery health, percent of design capacity (255 unknown)
    bytes 9-12  last site: latitude and longitude, 16 bits each (about 300 m by 450 m at 40 degrees north),
                0x8000 when the tag has never been told where it is
The 9-byte form is bytes 0 to 8 (no position); the 4-byte form is bytes 0 to 3 (alive, flags, pulls).

    python3 tool-telemetry/proto/hubble_payload.py            runs the self-test
    python3 tool-telemetry/proto/hubble_payload.py <hex>      decodes one payload"""
import random
import struct
import sys

FLAGS = ("fault", "jam_since_last", "battery_low", "used_24h")
NO_POS = -32768


def encode(trig, run_h, oc, soc=None, soh=None, lat=None, lon=None, fault=False, jam=False, batt_low=False, used=False,
           size=13):
    flags = fault | jam << 1 | batt_low << 2 | used << 3
    b = bytes([0x10 | flags]) + struct.pack("<I", min(int(trig), 0xFFFFFF))[:3]
    if size == 4:
        return b
    b += struct.pack("<HBBB", min(int(round(run_h * 10)), 0xFFFF), min(int(oc), 255),
                     255 if soc is None else max(0, min(100, int(soc))), 255 if soh is None else max(0, min(100, int(soh))))
    if size == 9:
        return b
    la = NO_POS if lat is None else max(-32767, min(32767, int(round(lat * 32767 / 90))))
    lo = NO_POS if lon is None else max(-32767, min(32767, int(round(lon * 32767 / 180))))
    return b + struct.pack("<hh", la, lo)


def decode(b):
    if len(b) not in (4, 9, 13):
        raise ValueError("Hubble payloads are 4, 9 or 13 bytes here, not %d" % len(b))
    if b[0] >> 4 != 1:
        raise ValueError("unknown layout %d" % (b[0] >> 4))
    d: dict = {f: bool(b[0] >> i & 1) for i, f in enumerate(FLAGS)}
    d["trig"] = struct.unpack("<I", b[1:4] + b"\0")[0]
    if len(b) >= 9:
        run10, oc, soc, soh = struct.unpack_from("<HBBB", b, 4)
        d.update(run_h=run10 / 10, oc=oc, soc=None if soc == 255 else soc, soh=None if soh == 255 else soh)
    if len(b) == 13:
        la, lo = struct.unpack_from("<hh", b, 9)
        d["lat"] = None if la == NO_POS else round(la * 90 / 32767, 4)
        d["lon"] = None if lo == NO_POS else round(lo * 180 / 32767, 4)
    return d


def selftest():
    r = random.Random(3)
    n = 0
    for _ in range(20000):
        size = r.choice((4, 9, 13))
        kw: dict = dict(trig=r.randint(0, 0xFFFFFF), run_h=r.randint(0, 65535) / 10, oc=r.randint(0, 255),
                  soc=r.choice((None, r.randint(0, 100))), soh=r.choice((None, r.randint(0, 100))),
                  lat=r.choice((None, r.uniform(-90, 90))), lon=r.choice((None, r.uniform(-180, 180))),
                  fault=r.random() < .5, jam=r.random() < .5, batt_low=r.random() < .5, used=r.random() < .5)
        b = encode(size=size, **kw)
        assert len(b) == size
        d = decode(b)
        assert d["trig"] == kw["trig"] and all(d[f] == kw[k] for f, k in zip(FLAGS, ("fault", "jam", "batt_low", "used")))
        if size >= 9:
            assert abs(d["run_h"] - kw["run_h"]) < 0.051 and d["oc"] == kw["oc"] and d["soc"] == kw["soc"] and d["soh"] == kw["soh"]
        if size == 13:
            for k, span in (("lat", 90), ("lon", 180)):
                if kw[k] is None:
                    assert d[k] is None
                else:
                    assert abs(d[k] - kw[k]) <= span / 32767 + 1e-4
        n += 1
    # saturation: counts past the field's range stop at its top instead of wrapping
    assert decode(encode(2 ** 30, 1e6, 1000))["trig"] == 0xFFFFFF and decode(encode(0, 1e6, 1000))["oc"] == 255
    print("OK: %d payloads encode and decode; counters saturate instead of wrapping" % n)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(decode(bytes.fromhex(sys.argv[1])))
    else:
        selftest()
