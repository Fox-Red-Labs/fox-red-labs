#!/usr/bin/env python3
"""Check the built image advertises as a HID device and has the bond settings the remote needs.
Reads SysConfig's generated ti_ble_config.c and ti_ble_config.h in the build folder. No board needed.
  python3 host/verify_build.py BUILD_DIR
"""
import re
import sys

SYMS = {  # names SysConfig writes into the advertising arrays (values from TI's gap.h)
    "GAP_ADTYPE_FLAGS": 0x01, "GAP_ADTYPE_16BIT_COMPLETE": 0x03, "GAP_ADTYPE_LOCAL_NAME_COMPLETE": 0x09,
    "GAP_ADTYPE_POWER_LEVEL": 0x0A, "GAP_ADTYPE_PERIPHERAL_CONN_INTERVAL_RANGE": 0x12, "GAP_ADTYPE_APPEARANCE": 0x19,
    "GAP_ADTYPE_FLAGS_GENERAL": 0x02, "GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED": 0x04,
}


def array(c, name):
    m = re.search(name + r"\[[^\]]*\]\s*=\s*\{(.*?)\};", c, re.S)
    if not m:
        raise SystemExit(name + " not found")
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", m.group(1), flags=re.S)
    out = []
    for tok in [t.strip() for t in body.split(",") if t.strip()]:
        lo = re.fullmatch(r"LO_UINT16\((.+)\)", tok)
        hi = re.fullmatch(r"HI_UINT16\((.+)\)", tok)
        expr = (lo or hi).group(1) if (lo or hi) else tok
        for k, v in SYMS.items():
            expr = re.sub(r"\b" + k + r"\b", str(v), expr)
        if re.search(r"'.'", expr):  # name characters
            expr = re.sub(r"'(.)'", lambda m: str(ord(m.group(1))), expr)
        val = int(eval(expr, {"__builtins__": {}}))  # only digits, hex, + - * / | ( ) left here
        out.append(val & 0xFF if lo else (val >> 8) & 0xFF if hi else val & 0xFF)
    return bytes(out)


def ad_structs(data):
    i, out = 0, {}
    while i < len(data) and data[i]:
        n = data[i]
        out[data[i + 1]] = data[i + 2:i + 1 + n]
        i += 1 + n
    return out


bdir = sys.argv[1]
c = open(bdir + "/ti_ble_config.c").read()
h = open(bdir + "/ti_ble_config.h").read()
adv, scan = array(c, "advData1"), array(c, "scanResData1")
a, s = ad_structs(adv), ad_structs(scan)
checks = [
    ("flags LE general, no BR/EDR", a.get(0x01) == bytes([0x06])),
    ("appearance 0x03C0 (generic HID), low byte first", a.get(0x19) == bytes([0xC0, 0x03])),
    ("16-bit UUIDs 0x1812 HID and 0x180F battery", a.get(0x03) == bytes([0x12, 0x18, 0x0F, 0x18])),
    ("name FRL Remote in scan response", s.get(0x09) == b"FRL Remote"),
    ("preferred interval 15 to 30 ms", s.get(0x12) == bytes([12, 0, 24, 0])),
    ("advertising packet fits 31 bytes", len(adv) <= 31),
]
m = re.search(r"gapBondParams_t gapBondParams\s*=\s*\{(.*?)\};", c, re.S)
bond = dict(re.findall(r"\.(\w+)\s*=\s*([^,\n]+)", m.group(1))) if m else {}
for field, want in [("pairMode", "GAPBOND_PAIRING_MODE_WAIT_FOR_REQ"), ("mitm", "false"),
                    ("ioCap", "GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT"), ("bonding", "true"),
                    ("secureConnection", "GAPBOND_SECURE_CONNECTION_ALLOW")]:
    got = bond.get(field, "").strip()
    checks.append(("bond %s = %s" % (field, got or "missing"), got == want))
print("adv  " + adv.hex())
print("scan " + scan.hex())
ok = True
for name, good in checks:
    print(("ok    " if good else "WRONG ") + name)
    ok &= bool(good)
sys.exit(0 if ok else 1)
