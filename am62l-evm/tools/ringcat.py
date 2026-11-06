#!/usr/bin/env python3
"""Print the console ring that Zephyr on A53_1 writes in on-chip SRAM (E242, image build-sram-core1-ring). Run on the board under Linux, as root.
Layout at 0x70813000 (apps/core1-hello/src/ring_console.c): u32 magic 0x52524C46, u32 head (bytes written, only grows), u32 size (4080), u32 reserved, data[4080].
Usage: ringcat.py [--addr 0x70813000] [--once] [--interval 0.2] [--dev /dev/mem]
Reads /dev/mem through mmap. If the kernel has CONFIG_STRICT_DEVMEM and refuses this range (Operation not permitted), use the kernel-module route in the plan."""
import argparse, mmap, os, struct, sys, time

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--addr", type=lambda s: int(s, 0), default=0x70813000)
    ap.add_argument("--dev", default="/dev/mem", help="memory device or a file holding the ring at offset --addr (testing)")
    ap.add_argument("--once", action="store_true", help="print what is there and exit")
    ap.add_argument("--interval", type=float, default=0.2)
    a = ap.parse_args()
    page = os.sysconf("SC_PAGE_SIZE")
    base = a.addr & ~(page - 1)
    off = a.addr - base
    length = ((off + 4096 + page - 1) // page) * page
    fd = os.open(a.dev, os.O_RDONLY | os.O_SYNC)
    try:
        m = mmap.mmap(fd, length, mmap.MAP_SHARED, mmap.PROT_READ, offset=base)
    except OSError as e:
        sys.exit(f"mmap of {a.dev} at {base:#x} failed: {e} (STRICT_DEVMEM, or the range is claimed; see the plan)")
    seen = 0
    announced = False
    while True:
        magic, head, size, _ = struct.unpack_from("<IIII", m, off)
        if magic != 0x52524C46 or size != 4080:
            if not announced:
                print(f"[ringcat] no ring yet at {a.addr:#x} (magic {magic:#010x})", file=sys.stderr)
                announced = True
            if a.once:
                return 1
        else:
            if head - seen > size:  # lost bytes: we were too slow, skip to the oldest still in the ring
                print(f"[ringcat] lost {head - seen - size} bytes", file=sys.stderr)
                seen = head - size
            out = bytearray()
            while seen < head:
                out.append(m[off + 16 + (seen % size)])
                seen += 1
            if out:
                sys.stdout.write(out.decode("utf-8", "replace"))
                sys.stdout.flush()
            if a.once:
                return 0
        time.sleep(a.interval)

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        pass
