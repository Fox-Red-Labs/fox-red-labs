#!/usr/bin/env python3
"""Host side of the E27 read-only QSPI dump (firmware: ../board/qspi_dump.c). Run it under the probe lock:
     python3 tools/frl/lock.py exec --serial M3010001 -- python3 -I ports/doom/m33-whx/tools/qspi_host.py ident
     ... qspi_host.py dump <out.bin>          all 128 blocks of 128 kB, each checked against the chip's CRC-32, 3 tries per block
     python3 -I qspi_host.py analyze <dump.bin> [blocks64k=28]    host only: used extent, blank 64 kB blocks, highest blank run, SHA-256
     ... qspi_host.py write <whx> <flash offset> <dumpA.bin>      (firmware: ../board/qspi_write.c) stage 128 kB at a time, program, read back
Each block is one DSLite load of the 4-byte request (restarts the program, which serves the request), one DSLite memory read of the
128 kB buffer plus the status struct. Nothing here writes the flash; the only writes are to the chip's RAM mailbox."""
import os, struct, subprocess, sys, tempfile, time, zlib
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
DSL = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
CCXML = os.path.join(REPO, "mspm33c321a", "MSPM33C321A_M3010001.ccxml")
BLK, NBLK, RAM, MBOX = 131072, 128, 0x30000000, 0x30030000
QD = RAM + BLK   # struct qd follows buf in .bss (checked against the map by the caller)

def run(args, **kw):
    return subprocess.run([DSL] + args, capture_output=True, text=True, **kw)

def read_ram(n):   # DSLite counts the range length in 32-bit words, not bytes
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        r = run(["memory", "-c", CCXML, "-r", "0x%x,%d" % (RAM, n // 4), "-s", "8", "-o", f.name])
        if r.returncode: raise RuntimeError("memory read failed: " + r.stdout[-300:] + r.stderr[-300:])
        return open(f.name, "rb").read()

def request(r):
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        f.write(struct.pack("<I", r)); f.flush()
        x = run(["load", "-c", CCXML, "-f", "%s,0x%x" % (f.name, MBOX)])
        if x.returncode: raise RuntimeError("load failed: " + x.stdout[-300:] + x.stderr[-300:])

def parse(b):
    q = b[BLK:]
    magic, err = struct.unpack_from("<II", q, 0)
    d = dict(magic=magic, err=err, id=q[8:11].hex(), sr1=q[12], cfg=q[13], sfdp=q[16:80], fast64=q[80:144], norm64=q[144:208],
             norm_eq_fast=struct.unpack_from("<I", q, 208)[0])
    d["req"], d["done"], d["done_blk"], d["crc"], d["ticks"], d["rd_err"] = struct.unpack_from("<6I", q, 212)
    return d

seq = int(time.time()) & 0xFFFF
def get_block(blk):
    global seq
    for attempt in range(3):
        seq = (seq + 1) & 0xFFFF or 1
        r = 0xA5000000 | (seq << 8) | blk
        request(r)
        for _ in range(6):
            b = read_ram(BLK + 256)
            d = parse(b)
            if d["done"] == r: break
            time.sleep(1)
        else:
            print("block %d: no answer (try %d)" % (blk, attempt + 1), file=sys.stderr); continue
        data = b[:BLK]
        if d["rd_err"] == 0 and d["done_blk"] == blk and zlib.crc32(data) == d["crc"]:
            return data, d
        print("block %d: CRC or read error (try %d): rd_err %d crc chip %08x host %08x" % (blk, attempt + 1, d["rd_err"], d["crc"], zlib.crc32(data)), file=sys.stderr)
    raise SystemExit("block %d failed 3 tries; stop" % blk)

def analyze(path, need):
    import hashlib
    d = open(path, "rb").read(); n = len(d) // 65536
    used = [i for i in range(n) if d[i * 65536:(i + 1) * 65536].count(255) != 65536]
    print("file %d bytes, sha256 %s" % (len(d), hashlib.sha256(d).hexdigest()))
    print("64 kB blocks %d, not all 0xFF: %d -> %s" % (n, len(used), used))
    for i in used:
        b = d[i * 65536:(i + 1) * 65536]; last = max(j for j in range(65536) if b[j] != 255)
        print("  block %2d (0x%06x): last non-FF byte at +0x%04x" % (i, i * 65536, last))
    top = (max(used) + 1) * 65536 if used else 0
    print("video/data extent: first %d bytes (0x%x) of the flash" % (top, top))
    blank = [i not in set(used) for i in range(n)]
    best = None
    for i in range(n - need, -1, -1):
        if all(blank[i:i + need]): best = i; break
    print("highest blank run of %d blocks: %s" % (need, "none" if best is None else "starts at block %d = offset 0x%06x, ends 0x%06x" % (best, best * 65536, (best + need) * 65536)))
    return best

NM = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmnm"
WROUT = os.path.abspath("out/boosterpack-speedrun/m33-qspi-write/fw/ticlang/qspi_flash_readback_DMA.out")
def sym(out, name):
    for l in subprocess.check_output([NM, out]).decode().splitlines():
        p = l.split()
        if len(p) == 3 and p[2] == name: return int(p[0], 16)
    raise SystemExit("symbol %s not in %s" % (name, out))

def wstatus():
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        r = run(["memory", "-c", CCXML, "-r", "0x%x,%d" % (sym(WROUT, "wr"), 16), "-s", "8", "-o", f.name])
        if r.returncode: raise RuntimeError("memory read failed: " + r.stdout[-300:])
        v = struct.unpack("<16I", open(f.name, "rb").read())
    return dict(zip("magic err id sr1 sr1_after done done_addr done_len pages blank_ok already verify_bad ticks".split(), v))

def write_whx(whx, off, dumpA):
    STG = 0x30010000; CH = 131072
    data = open(whx, "rb").read(); d = open(dumpA, "rb").read()
    assert off % 65536 == 0 and off + len(data) <= len(d)
    assert d[off:off + len(data)].count(255) == len(data), "target range is not blank in the dump: stop"
    print("target 0x%x..0x%x is blank in the dump; writing %d bytes in %d chunks" % (off, off + len(data), len(data), (len(data) + CH - 1) // CH))
    st = wstatus(); print("write firmware:", {k: hex(v) for k, v in st.items() if k in ("magic", "err", "id", "sr1")})
    assert st["magic"] == 0xD00D0002 and st["id"] == 0xC22018 and st["sr1"] & 0x3C == 0, "write firmware refused (ID, protect bits)"
    seq = 1
    for i in range((len(data) + CH - 1) // CH):
        chunk = data[i * CH:(i + 1) * CH]; seq += 1
        req = 0xA5000000 | seq
        blob = chunk + b"\xff" * (CH - len(chunk)) + struct.pack("<4I", off + i * CH, len(chunk), zlib.crc32(chunk), req)
        with tempfile.NamedTemporaryFile(suffix=".bin") as f:
            f.write(blob); f.flush()
            x = run(["load", "-c", CCXML, "-f", "%s,0x%x" % (f.name, STG)])
            if x.returncode: raise SystemExit("load failed: " + x.stdout[-300:])
        for _ in range(20):
            time.sleep(1); st = wstatus()
            if st["done"] == req: break
        else: raise SystemExit("chunk %d: no answer" % i)
        print("chunk %2d at 0x%06x len %6d: err %d blank_ok %d pages %d verify_bad %d sr1_after %#x  %.1f s on chip" % (i, off + i * CH, len(chunk), st["err"], st["blank_ok"], st["pages"], st["verify_bad"], st["sr1_after"], st["ticks"] / 160e6), flush=True)
        if st["err"] or st["verify_bad"] or not st["blank_ok"]: raise SystemExit("chunk %d failed: %s" % (i, st))
    print("write done")

def main():
    cmd = sys.argv[1]
    if cmd == "analyze": analyze(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 28); return
    if cmd == "write": write_whx(sys.argv[2], int(sys.argv[3], 0), sys.argv[4]); return
    if cmd == "ident":
        b = read_ram(BLK + 256); d = parse(b)
        for k in ("magic", "err", "id", "sr1", "cfg", "norm_eq_fast"): print(k, hex(d[k]) if isinstance(d[k], int) else d[k])
        print("sfdp", d["sfdp"].hex()); print("fast64", d["fast64"].hex())
    elif cmd == "dump":
        out = sys.argv[2]; t0 = time.time(); crcs = []
        with open(out, "wb") as f:
            for blk in range(NBLK):
                data, d = get_block(blk); f.write(data); crcs.append(d["crc"])
                print("block %3d ok crc %08x read %.1f ms  (%.0f s)" % (blk, d["crc"], d["ticks"] / 160e3, time.time() - t0), flush=True)
        open(out + ".crc32", "w").write("".join("%d %08x\n" % (i, c) for i, c in enumerate(crcs)))
        print("dump done", out, "%.0f s" % (time.time() - t0))
main()
