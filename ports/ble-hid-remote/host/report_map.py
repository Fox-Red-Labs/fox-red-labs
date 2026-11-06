#!/usr/bin/env python3
"""Check the HID report map and report bytes before the board is touched.

Reads the report map array and the HID_CC_* bit defines straight from the
firmware sources (app/hid_remote.c and app/hid_remote.h), decodes the map item
by item with the short-item rules of the USB HID 1.11 spec, and checks:

  - it is one Consumer Control application collection with report ID 1,
  - the input report is exactly 8 bits: five 1-bit buttons, 3 bits of padding,
  - the button usages are, in order, CD B5 B6 E9 EA,
  - each HID_CC_* bit in the header lands on the usage its comment names.

Then it decodes example report bytes the way a host would.

  python3 host/report_map.py            # decode and check, exit 1 on any mismatch
  python3 host/report_map.py 01 00 02   # also decode these report bytes
  python3 host/report_map.py --map-hex  # print the map bytes as hex (for comparing with a GATT read)
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
C_FILE = HERE / "app" / "hid_remote.c"
H_FILE = HERE / "app" / "hid_remote.h"

CONSUMER_USAGES = {
    0x01: "Consumer Control",
    0xB5: "Scan Next Track",
    0xB6: "Scan Previous Track",
    0xCD: "Play/Pause",
    0xE9: "Volume Increment",
    0xEA: "Volume Decrement",
}
EXPECTED_USAGES = [0xCD, 0xB5, 0xB6, 0xE9, 0xEA]

# (type, tag) -> name, short items only (HID 1.11 section 6.2.2)
ITEM_NAMES = {
    (0, 0x8): "Input", (0, 0x9): "Output", (0, 0xB): "Feature",
    (0, 0xA): "Collection", (0, 0xC): "End Collection",
    (1, 0x0): "Usage Page", (1, 0x1): "Logical Minimum", (1, 0x2): "Logical Maximum",
    (1, 0x7): "Report Size", (1, 0x8): "Report ID", (1, 0x9): "Report Count",
    (2, 0x0): "Usage", (2, 0x1): "Usage Minimum", (2, 0x2): "Usage Maximum",
}


def read_map(c_text):
    m = re.search(r"hidReportMap\[\]\s*=\s*\{(.*?)\};", c_text, re.S)
    if not m:
        raise SystemExit("report map array not found in " + str(C_FILE))
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", body))


def read_bits(h_text):
    bits = {}
    for name, shift, usage in re.findall(
            r"#define\s+(HID_CC_\w+)\s+\(1u\s*<<\s*(\d+)\)\s*/\*\s*usage\s+0x([0-9A-Fa-f]{2})", h_text):
        bits[name] = (int(shift), int(usage, 16))
    return bits


def items(data):
    i = 0
    while i < len(data):
        prefix = data[i]
        if prefix == 0xFE:
            raise SystemExit("long item at %d: not expected" % i)
        size = {0: 0, 1: 1, 2: 2, 3: 4}[prefix & 3]
        typ, tag = (prefix >> 2) & 3, prefix >> 4
        raw = data[i + 1:i + 1 + size]
        if len(raw) != size:
            raise SystemExit("map ends inside an item at byte %d" % i)
        yield i, typ, tag, int.from_bytes(raw, "little") if size else 0, raw
        i += 1 + size


def decode(data, verbose=True):
    """Walk the map, return the input fields as a list of (bit_offset, size, usage or None)."""
    g = {"page": None, "size": 0, "count": 0, "id": 0}
    usages, fields, depth, ids, bit = [], [], 0, set(), 0
    for off, typ, tag, val, raw in items(data):
        name = ITEM_NAMES.get((typ, tag), "type %d tag %x" % (typ, tag))
        note = ""
        if (typ, tag) == (1, 0x0):
            g["page"] = val
            note = "Consumer" if val == 0x0C else ""
        elif (typ, tag) == (1, 0x7):
            g["size"] = val
        elif (typ, tag) == (1, 0x9):
            g["count"] = val
        elif (typ, tag) == (1, 0x8):
            g["id"] = val
            ids.add(val)
        elif (typ, tag) == (2, 0x0):
            usages.append(val)
            note = CONSUMER_USAGES.get(val, "") if g["page"] == 0x0C else ""
        elif (typ, tag) == (0, 0xA):
            depth += 1
            usages = []  # every main item clears the local items (HID 1.11, 6.2.2.8)
            note = {0: "Physical", 1: "Application", 2: "Logical"}.get(val, "")
        elif (typ, tag) == (0, 0xC):
            depth -= 1
            usages = []
        elif (typ, tag) == (0, 0x8):
            const = bool(val & 1)
            note = "Constant" if const else ("Data, %s, %s" % ("Variable" if val & 2 else "Array",
                                                                 "Relative" if val & 4 else "Absolute"))
            for k in range(g["count"]):
                u = None if const else (usages[k] if k < len(usages) else (usages[-1] if usages else None))
                fields.append((bit, g["size"], u))
                bit += g["size"]
            usages = []
        if verbose:
            print("  %02d  %-24s %-18s %s" % (off, raw_hex(data, off, len(raw)), name + (" %d" % val if raw else ""), note))
    if depth != 0:
        raise SystemExit("collections not closed")
    return fields, ids, bit


def raw_hex(data, off, n):
    return " ".join("%02X" % b for b in data[off:off + 1 + n])


def report_names(byte, fields):
    out = [CONSUMER_USAGES.get(u, "0x%02X" % u) for (b, s, u) in fields if u is not None and (byte >> b) & 1]
    return ", ".join(out) if out else "nothing pressed"


def main(argv):
    c_text, h_text = C_FILE.read_text(), H_FILE.read_text()
    data = read_map(c_text)
    if "--map-hex" in argv:
        print(data.hex())
        return 0
    print("report map, %d bytes, from %s" % (len(data), C_FILE.relative_to(HERE)))
    fields, ids, bits = decode(data)
    errors = []
    if data[:6] != bytes([0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01]):
        errors.append("map does not start with Usage Page Consumer, Usage Consumer Control, Collection Application")
    if ids != {1}:
        errors.append("report IDs %s, expected {1}" % sorted(ids))
    if bits != 8:
        errors.append("input report is %d bits, expected 8 (one byte)" % bits)
    used = [u for (_, s, u) in fields if u is not None]
    if used != EXPECTED_USAGES:
        errors.append("usages %s, expected %s" % ([hex(u) for u in used], [hex(u) for u in EXPECTED_USAGES]))
    if any(s != 1 for (_, s, u) in fields if u is not None):
        errors.append("a button field is wider than 1 bit")
    hbits = read_bits(h_text)
    if len(hbits) != 5:
        errors.append("found %d HID_CC_* defines with usage comments, expected 5" % len(hbits))
    by_bit = {b: u for (b, s, u) in fields if u is not None}
    for name, (shift, usage) in sorted(hbits.items(), key=lambda kv: kv[1][0]):
        got = by_bit.get(shift)
        ok = got == usage
        print("  %-20s bit %d -> %-20s %s" % (name, shift, CONSUMER_USAGES.get(got, str(got)), "ok" if ok else "MISMATCH"))
        if not ok:
            errors.append("%s is bit %d, the map puts 0x%s there" % (name, shift, "%02X" % got if got is not None else "nothing"))
    if not re.search(r"hidReportRef\[2\]\s*=\s*\{\s*HID_REPORT_ID_CC\s*,\s*HID_REPORT_TYPE_IN\s*\}", c_text) \
            or not re.search(r"#define\s+HID_REPORT_ID_CC\s+1\b", h_text):
        errors.append("Report Reference is not {1, input}")
    # what the buttons send, as the host decodes it
    print("report bytes the firmware sends:")
    for label, b in [("left down", 0x01), ("left up", 0x00), ("right down", 0x02), ("right up", 0x00)]:
        print("  %-11s %02X  -> %s" % (label, b, report_names(b, fields)))
    for arg in [a for a in argv if not a.startswith("-")]:
        b = int(arg, 16)
        print("  given       %02X  -> %s" % (b, report_names(b, fields)))
    if errors:
        for e in errors:
            print("FAIL:", e)
        return 1
    print("OK: consumer control map, report ID 1, 1 byte, 5 buttons in the expected order")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
