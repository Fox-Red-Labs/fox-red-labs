#!/usr/bin/env python3
"""Small image tools for the CC2340R5 OAD run. Python standard library only.

  images.py ccfg <file.hex> ...                 print the CCFG fields that can lock the chip
  images.py merge <out.hex> <in.hex | in.bin@0xADDR> ...   join images into one Intel HEX; refuses overlaps
  images.py haskey <file.hex> <keys.c>          check the MCUboot image holds the ECDSA P-256 public key from keys.c

CCFG layout from TI's hw_ccfg.h (cc23x0r5): permission nibble 0xA = allow. Same checks as
ports/bthome/zephyr/check_ccfg.py, without the intelhex package."""
import binascii, re, struct, sys

CCFG = 0x4E020000
PERMS = ["allowReturnToFactory", "allowFakeStby", "allowToolsClientMode", "allowChipErase",
         "allowFlashProgram", "allowFlashVerify", "allowEnergyTrace", "allowDebugPort"]
CRCS = [("bootCfg", 0x000, 0x00B, 0x00C), ("main CCFG", 0x010, 0x74B, 0x74C),
        ("user record", 0x750, 0x7CB, 0x7CC), ("debug config", 0x7D0, 0x7FB, 0x7FC)]


def read_hex(path):
    """Intel HEX to {address: byte}. Handles record types 00, 01, 02, 04, 05."""
    mem, upper = {}, 0
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise SystemExit(f"{path}:{n}: not an Intel HEX line")
        b = bytes.fromhex(line[1:])
        if sum(b) & 0xFF:
            raise SystemExit(f"{path}:{n}: bad record checksum")
        ln, addr, typ, data = b[0], (b[1] << 8) | b[2], b[3], b[4:4 + b[0]]
        if typ == 0:
            for i, v in enumerate(data):
                mem[upper + addr + i] = v
        elif typ == 1:
            break
        elif typ == 2:
            upper = int.from_bytes(data, "big") << 4
        elif typ == 4:
            upper = int.from_bytes(data, "big") << 16
    return mem


def segments(mem):
    out, start, prev = [], None, None
    for a in sorted(mem):
        if start is None:
            start = a
        elif a != prev + 1:
            out.append((start, prev))
            start = a
        prev = a
    if start is not None:
        out.append((start, prev))
    return out


def write_hex(mem, path):
    lines, upper = [], None

    def rec(a16, typ, data):
        b = bytes([len(data), a16 >> 8, a16 & 0xFF, typ]) + data
        return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()

    for start, end in segments(mem):
        a = start
        while a <= end:
            n = min(16, end - a + 1, 0x10000 - (a & 0xFFFF))
            if a >> 16 != upper:
                upper = a >> 16
                lines.append(rec(0, 4, upper.to_bytes(2, "big")))
            lines.append(rec(a & 0xFFFF, 0, bytes(mem[a + i] for i in range(n))))
            a += n
    open(path, "w").write("\n".join(lines + [":00000001FF"]) + "\n")


def ccfg(paths):
    bad = 0
    for p in paths:
        mem = read_hex(p)
        print(p)
        print("  segments:", ", ".join(f"0x{a:08X}-0x{b:08X}" for a, b in segments(mem)))
        if CCFG + 0x18 not in mem:
            print("  no CCFG in this file: the chip keeps whatever CCFG it already has")
            continue
        w = lambda off: struct.unpack("<I", bytes(mem.get(CCFG + off + i, 0xFF) for i in range(4)))[0]
        p_word = w(0x18)
        print(f"  permissions word 0x4E020018 = 0x{p_word:08X}")
        for i, name in enumerate(PERMS):
            v = (p_word >> (4 * i)) & 0xF
            ok = v == 0xA
            if name in ("allowDebugPort", "allowChipErase") and not ok:
                bad += 1
            print(f"    {name:22s} 0x{v:X} {'allow' if ok else 'NOT ALLOW'}")
        print(f"  write/erase protect: main 0-31 0x{w(0x20):08X}, main 32-255 0x{w(0x24):08X}, aux 0x{w(0x28):08X} (1 = not protected)")
        print(f"  debug authorization 0x4E0207D0 = 0x{mem.get(CCFG + 0x7D0, 0xFF):02X}")
        for name, a, b, at in CRCS:
            calc = binascii.crc32(bytes(mem.get(CCFG + i, 0xFF) for i in range(a, b + 1))) & 0xFFFFFFFF
            have = w(at)
            print(f"  CRC {name:12s} in image 0x{have:08X}, computed 0x{calc:08X} {'match' if have == calc else 'MISMATCH'}")
    if bad:
        raise SystemExit(f"STOP: {bad} lock field(s) not set to allow. Do not flash.")


def merge(out, inputs):
    mem = {}
    for spec in inputs:
        if "@" in spec:
            path, addr = spec.rsplit("@", 1)
            data = open(path, "rb").read()
            part = {int(addr, 0) + i: v for i, v in enumerate(data)}
        else:
            path, part = spec, read_hex(spec)
        clash = mem.keys() & part.keys()
        if clash:
            raise SystemExit(f"{path} overlaps earlier inputs at 0x{min(clash):08X} ({len(clash)} bytes)")
        mem.update(part)
        print(f"  {path}: {len(part)} bytes, " + ", ".join(f"0x{a:08X}-0x{b:08X}" for a, b in segments(part)))
    write_hex(mem, out)
    print(f"wrote {out}: {len(mem)} bytes")


def haskey(hexpath, keys_c):
    src = open(keys_c).read()
    m = re.search(r"MCUBOOT_SIGN_EC256.*?root_pub_der\[\]\s*=\s*\{(.*?)\}", src, re.S)
    der = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(1)))
    point = der[-65:]  # uncompressed P-256 point, 0x04 || X || Y
    mem = read_hex(hexpath)
    blob = bytearray(max(mem) + 1 - min(mem))
    base = min(mem)
    for a, v in mem.items():
        blob[a - base] = v
    at = blob.find(point)
    print(f"keys.c EC P-256 public key (DER {len(der)} bytes), SHA-256 {__import__('hashlib').sha256(der).hexdigest()}")
    print(f"  point found in {hexpath} at 0x{base + at:08X}" if at >= 0 else f"  point NOT found in {hexpath}")
    return at >= 0


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "ccfg":
        ccfg(args)
    elif cmd == "merge":
        merge(args[0], args[1:])
    elif cmd == "haskey":
        sys.exit(0 if haskey(args[0], args[1]) else 1)
    else:
        raise SystemExit(__doc__)
