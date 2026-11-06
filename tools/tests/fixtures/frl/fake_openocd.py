#!/usr/bin/env python3
"""Fake OpenOCD for tools/tests/test_frl.py. No hardware. State in $FAKE_DIR:
boards.json {serial: {"deviceid": "0x...", "traceid": "0x...", "pattern": [ms...] or null, "busy": bool}},
flash-<serial>.bin (64 kB), calls.jsonl (one line per run: serial and commands)."""
import json, os, re, sys
from pathlib import Path

D = Path(os.environ["FAKE_DIR"])
argv = sys.argv[1:]
if "--version" in argv:
    print("xPack Open On-Chip Debugger 0.12.0+dev-02228-ge5888bda3-dirty (2025-10-04-22:59)", file=sys.stderr)
    sys.exit(0)
cmds = [argv[i + 1] for i, a in enumerate(argv) if a == "-c"]
serial = next((c.split()[2] for c in cmds if c.startswith("adapter serial")), None)
with open(D / "calls.jsonl", "a") as f:
    f.write(json.dumps({"serial": serial, "cmds": cmds, "PROBE_env": os.environ.get("PROBE")}) + "\n")
boards = json.loads((D / "boards.json").read_text())
b = boards.get(serial)
if not b:
    print("Error: No XDS110 found with serial number " + str(serial)); sys.exit(1)
if b.get("busy"):
    print("Error: unable to claim interface: LIBUSB_ERROR_BUSY"); sys.exit(1)
flash = D / f"flash-{serial}.bin"
if not flash.exists():
    flash.write_bytes(bytes(65536))


def elf_load(path):
    import struct
    data = Path(path).read_bytes()
    phoff, = struct.unpack_from("<I", data, 28); phentsize, phnum = struct.unpack_from("<HH", data, 42)
    out = []
    for k in range(phnum):
        t, off, _, pa, fs, _, _, _ = struct.unpack_from("<IIIIIIII", data, phoff + k * phentsize)
        if t == 1 and fs:
            out.append((pa, data[off:off + fs]))
    return out


def unbrace(s):
    return s[1:-1] if s.startswith("{") and s.endswith("}") else s


rc = 0
for c in cmds:
    m = re.match(r"mdw (0x[0-9A-Fa-f]+)(?: (\d+))?$", c)
    if m:
        addr, n = int(m.group(1), 16), int(m.group(2) or 1)
        if addr == 0x41C40000:
            words = [b["traceid"], b["deviceid"], "0x803cbb70"][:n]
            print(f"0x{addr:08x}: " + " ".join(f"{int(w, 16):08x}" for w in words) + " ")
        elif addr == 0x41C40004:
            print(f"0x{addr:08x}: {int(b['deviceid'], 16):08x} ")
        continue
    m = re.match(r"dump_image (\{.*\}|\S+) (0x[0-9A-Fa-f]+) (0x[0-9A-Fa-f]+)$", c)
    if m:
        addr, n = int(m.group(2), 16), int(m.group(3), 16)
        data = flash.read_bytes()[addr:addr + n] if addr < 0x10000 else bytes([0xA5]) * n
        Path(unbrace(m.group(1))).write_bytes(data); print(f"dumped {n} bytes"); continue
    m = re.match(r"program (\{.*\}|\S+)(?: (0x[0-9A-Fa-f]+))? verify$", c)
    if m:
        path = unbrace(m.group(1)); fl = bytearray(flash.read_bytes())
        if os.environ.get("FAKE_PROGRAM_HANG"):
            print("** Programming Started **"); sys.exit(1)
        if path.endswith(".bin"):
            data = Path(path).read_bytes(); a = int(m.group(2), 16); fl[a:a + len(data)] = data
        else:
            for a, data in elf_load(path):
                fl[a:a + len(data)] = data
            b["pattern"] = json.loads(os.environ.get("FAKE_IMAGE_PATTERN", "null")); boards[serial] = b
            (D / "boards.json").write_text(json.dumps(boards))
        flash.write_bytes(bytes(fl))
        print("** Programming Started **\n** Programming Finished **\n** Verify Started **\n** Verified OK **"); continue
    m = re.match(r"verify_image (\{.*\}|\S+)$", c)
    if m:
        fl = flash.read_bytes(); same = all(fl[a:a + len(d)] == d for a, d in elf_load(unbrace(m.group(1))))
        print("verified 1024 bytes in 0.1s" if same else "checksum mismatch - attempting binary compare\ndiff 0 address 0x00000000. Was 0x00 instead of 0x01\nMore than 128 errors, the rest are not shown.")
        rc = 0 if same else 1; continue
    if "read_memory" in c and "while" in c:
        print("E 1")
        pat = b.get("pattern"); t, lvl, out = 0, 1, ["T 0 1"]
        if pat:
            i = 0
            while t < 4000:
                t += pat[i % len(pat)]; i += 1; lvl = 1 - lvl
                if t < 4000:
                    out.append(f"T {t} {lvl}")
        print("\n".join(out)); print("N 17000 4000"); continue
print("shutdown command invoked")
sys.exit(rc)
