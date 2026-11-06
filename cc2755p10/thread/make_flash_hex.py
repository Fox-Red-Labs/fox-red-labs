#!/usr/bin/env python3
"""Make a DSLite-ready .hex from an ot-ti ELF for the LP-EM-CC2755P10, with a CCFG and SCFG whose CRC32s are filled.
   python3 cc2755p10/thread/make_flash_hex.py <ot-cli-ftd.out> <ccfg.bin> <scfg.bin> <out.hex>
ot-ti's GCC build leaves the CCFG and SCFG CRC32 fields at 0 (TI Arm Clang builds fill them after linking). A CC27xx with
invalid CRCs keeps its debug port closed (TRM SWCU195A 9.1.3), so this script does not flash the ELF's own configuration.
It takes main flash from the ELF and the CCFG and SCFG from known-good files (the board's shipped CCFG and the SDK default
SCFG, both restored and read back on 2026-10-09), and refuses unless the ELF's configuration matches them in every byte
except the CRC fields and the CCFG user record (the ELF zero-fills it; the known-good CCFG leaves it blank).
It also refuses main flash that reaches the HSM area (0xE8000)."""
import struct, sys, zlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "portability" / "crossbuild"))
import run_cc23 as r

elf, ccfg_p, scfg_p, out = sys.argv[1:5]
d = open(elf, "rb").read()
assert d[:4] == b"\x7fELF" and d[4] == 1, "not a 32-bit ELF"
phoff, = struct.unpack_from("<I", d, 0x1C); phsz, phnum = struct.unpack_from("<HH", d, 0x2A)
segs = []
for i in range(phnum):
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from("<8I", d, phoff + i * phsz)
    if t == 1 and fs: segs.append((pa, d[off:off + fs]))
cfg = {pa: b for pa, b in segs if pa >= 0x4E000000}
main = sorted((pa, b) for pa, b in segs if pa < 0x4E000000)
end = max(pa + len(b) for pa, b in main)
assert end <= 0xE8000, "main flash reaches the HSM area"
ccfg, scfg = open(ccfg_p, "rb").read(), open(scfg_p, "rb").read()
assert len(ccfg) == 2048 and len(scfg) == 1024
skip_c = set(range(0xC, 0x10)) | set(range(0x74C, 0x750)) | set(range(0x7FC, 0x800)) | set(range(0x750, 0x7D0))
ec, es = cfg[0x4E020000], cfg[0x4E040000]
dc = [hex(i) for i in range(2048) if ec[i] != ccfg[i] and i not in skip_c]
ds = [hex(i) for i in range(1024) if es[i] != scfg[i] and not 228 <= i < 232]
assert not dc and not ds, f"ELF configuration differs from the known-good files: CCFG {dc[:8]} SCFG {ds[:8]}"
u = lambda b, o: struct.unpack_from("<I", b, o)[0]
assert u(ccfg, 0xC) == zlib.crc32(ccfg[:0xC]) and u(ccfg, 0x74C) == zlib.crc32(ccfg[0x10:0x74C]) and u(ccfg, 0x7FC) == zlib.crc32(ccfg[0x7D0:0x7FC])
assert u(scfg, 228) == zlib.crc32(scfg[:228])
assert ccfg[0x7D0] == 0x5A and u(ccfg, 0x18) == 0xAAAAAAAA, "CCFG must be debug-open with every permission allowed"
open(out, "w").write(r.ihex(main + [(0x4E020000, ccfg), (0x4E040000, scfg)]))
print(f"{out}: main flash to {end:#x} from {elf}; CCFG {ccfg_p}; SCFG {scfg_p}")
