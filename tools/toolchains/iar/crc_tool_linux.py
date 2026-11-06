#!/usr/bin/env python3
"""Linux stand-in for the SimpleLink SDK's `crc_tool patch-image` (CC23xx and CC27xx post-build step).

TI ships crc_tool only as a macOS binary (tools/common/crc_tool/crc_tool, a PyInstaller build of the Python
beside it, using the LIEF library). The documented algorithm (tools/common/crc_tool/README.md):
  1. find symbols named <prefix>*_begin and <prefix>*_end in the ELF symbol table; pairs only
  2. for each pair, CRC-32 (zlib, the usual polynomial) over [begin, end] inclusive
  3. write the CRC little-endian into the 4 bytes at end+1, in ascending order of end
This file does steps 1 to 3 on a 32-bit little-endian ELF with the standard library only, patching the
bytes in place (the ELF layout does not change). One quirk of TI's code is kept on purpose: the CRC is
converted with (bit_length + 7) // 8 bytes, so a CRC whose top byte is 0 writes fewer than 4 bytes.

  crc_tool_linux.py patch-image --elf IN --symbol-prefix PREFIX [-o OUT | --output OUT] [--quiet|--verbose]

Same command line as the TI tool (only the patch-image use case). Exit 0 on success (a region outside every loadable segment is logged and skipped, as TI's tool does), 1 on a bad ELF or
overlapping CRC locations. The container wrapper in iar_harness.py bind-mounts this over the SDK's crc_tool.
"""
import argparse, struct, sys, zlib


def parse(d):
    if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
        raise SystemExit("error: not a 32-bit little-endian ELF")
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", d, 0x2E)
    phoff, = struct.unpack_from("<I", d, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", d, 0x2A)
    phs = [struct.unpack_from("<8I", d, phoff + i * phentsize) for i in range(phnum)]
    secs = [struct.unpack_from("<10I", d, shoff + i * shentsize) for i in range(shnum)]
    return phs, secs


def symbols(d, secs, prefix):
    out = {}
    for s in secs:
        if s[1] != 2:  # SHT_SYMTAB
            continue
        strtab = secs[s[6]]
        names = d[strtab[4]:strtab[4] + strtab[5]]
        for i in range(s[5] // 16):
            nm, val, size, info, other, shndx = struct.unpack_from("<IIIBBH", d, s[4] + i * 16)
            name = names[nm:names.index(b"\0", nm)].decode("latin1")
            if name.startswith(prefix):
                out[name] = val
    return out


def file_range(phs, va, n):
    """File offset of n bytes at virtual address va, inside one PT_LOAD segment; None when there is none."""
    for t, off, pva, ppa, fsz, msz, fl, al in phs:
        if t == 1 and pva <= va and va + n <= pva + fsz:
            return off + (va - pva)
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(prog="crc_tool")
    sub = ap.add_subparsers(dest="use_case", required=True)
    p = sub.add_parser("patch-image")
    p.add_argument("--elf", required=True)
    p.add_argument("-p", "--symbol-prefix", default="__crc_section")
    p.add_argument("-o", "--output", default="patched.out")
    g = p.add_mutually_exclusive_group(); g.add_argument("--quiet", action="store_true"); g.add_argument("--verbose", action="store_true")
    a = ap.parse_args(argv)
    d = bytearray(open(a.elf, "rb").read())
    phs, secs = parse(d)
    syms = symbols(d, secs, a.symbol_prefix)
    if not syms:
        print("warning: no symbols with prefix: %s found" % a.symbol_prefix, file=sys.stderr)
    pairs = {}
    for k, v in syms.items():
        if k.endswith("_begin"):
            stem = k[:-6]
            if stem + "_end" in syms:
                pairs[stem] = (v, syms[stem + "_end"])
    for k, (b, e) in pairs.items():
        if b > e:
            raise SystemExit("error: beginning comes before end for symbol: %s = (0x%x, 0x%x)" % (k, b, e))
    ks = list(pairs)
    for i in ks:
        for j in ks:
            if i < j and max(pairs[i][1] + 1, pairs[j][1] + 1) <= min(pairs[i][1] + 4, pairs[j][1] + 4):
                raise SystemExit("error: overlapping CRC write locations: %s, %s" % (i, j))
    done = 0
    for k in sorted(pairs, key=lambda x: pairs[x][1]):
        b, e = pairs[k]
        n = e + 1 - b
        o = file_range(phs, b, n)
        if o is None:   # TI's tool logs this and moves on (an application image that does not carry the CCFG)
            print("Can't find a segment associated with the virtual address 0x%08x" % (e + 1), file=sys.stderr)
            continue
        crc = zlib.crc32(bytes(d[o:o + n]))
        cb = crc.to_bytes((crc.bit_length() + 7) // 8, "little")
        w = file_range(phs, e + 1, max(len(cb), 1))
        if w is None:
            print("Can't find a segment associated with the virtual address 0x%08x" % (e + 1), file=sys.stderr)
            continue
        if not a.quiet:
            print("Updating CRC32 field @ address 0x%08x, based on data in the range [0x%08x, 0x%08x]. New value 0x%08x" % (e + 1, b, e, crc))
        d[w:w + len(cb)] = cb
        done += 1
    if not done and not a.quiet:
        print("warning: No CRC was inserted into file", file=sys.stderr)
    open(a.output, "wb").write(d)
    return 0


if __name__ == "__main__":
    sys.exit(main())
