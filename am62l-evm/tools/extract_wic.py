#!/usr/bin/env python3
"""Carve partitions out of a .wic.xz as sparse files, in one streaming pass, without expanding the image.

  extract_wic.py IMAGE.wic.xz OUTDIR [--list] [--parts 1,2]

Writes OUTDIR/part1.img (the FAT boot partition: tiboot3.bin, tispl.bin, u-boot.img, uEnv.txt) and
OUTDIR/part2.img (the ext4 rootfs, which also holds /boot/Image and the dtb; env bootpart=1:2 in
ti-u-boot board/ti/am62lx/am62lx.env). Zero blocks are skipped (seek), so a mostly empty rootfs stays small.
Runs on the Mac mini; put OUTDIR on an external drive, not the
internal disk. Run with `python3 -I`.
"""
import sys, struct, subprocess, os

img, out = sys.argv[1], sys.argv[2]
want = {1, 2}
if '--parts' in sys.argv:
    want = {int(x) for x in sys.argv[sys.argv.index('--parts') + 1].split(',')}
listonly = '--list' in sys.argv
os.makedirs(out, exist_ok=True)
p = subprocess.Popen(['xz', '-dc', img], stdout=subprocess.PIPE, bufsize=1 << 22)
head = p.stdout.read(1 << 20)
parts = []
if head[512:520] == b'EFI PART':
    lba, n, sz = struct.unpack_from('<QII', head, 512 + 72)
    ent = b''
    tbl = head[lba * 512:]
    for i in range(n):
        e = tbl[i * sz:(i + 1) * sz]
        if len(e) < 128 or e[:16] == bytes(16):
            continue
        first, last = struct.unpack_from('<QQ', e, 32)
        parts.append((i + 1, first, last - first + 1))
else:
    for i in range(4):
        e = head[446 + 16 * i:462 + 16 * i]
        t = e[4]; first, cnt = struct.unpack('<II', e[8:16])
        if t:
            parts.append((i + 1, first, cnt))
for n, first, cnt in parts:
    print(f"partition {n}: start sector {first}, {cnt} sectors, {cnt * 512 / 1048576:.1f} MiB, offset {first * 512}")
if listonly:
    p.kill(); sys.exit(0)
files = {}
for n, first, cnt in parts:
    if n in want:
        f = open(os.path.join(out, f"part{n}.img"), 'wb')
        f.truncate(cnt * 512)
        files[n] = (first * 512, cnt * 512, f)
pos = 0
buf = head
Z = bytes(1 << 16)
while buf:
    for n, (off, size, f) in files.items():
        lo, hi = max(pos, off), min(pos + len(buf), off + size)
        for a in range(lo, hi, 1 << 16):
            b = min(a + (1 << 16), hi)
            chunk = buf[a - pos:b - pos]
            if chunk != Z[:len(chunk)]:
                f.seek(a - off); f.write(chunk)
    pos += len(buf)
    if pos >= max([o + s for o, s, _ in files.values()] or [0]):
        break
    buf = p.stdout.read(1 << 22)
p.kill()
for n, (off, size, f) in files.items():
    f.close()
    print(f"wrote part{n}.img ({size} bytes apparent, {os.stat(f.name).st_blocks * 512} on disk)")
