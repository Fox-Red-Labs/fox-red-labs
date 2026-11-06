#!/usr/bin/env python3
"""Read the GPIO0 registers that decide LD8 (GPIO0_123) and SW5 (GPIO0_90), as root, without touching the lines.
  gpio_regs.py [SECONDS]     prints OUT_DATA of LD8 and IN_DATA of SW5 every 0.1 s for SECONDS (default one line)
Bank n of GPIO0 starts at 0x600000 + 0x10 + 0x28*n: DIR +0x00, OUT_DATA +0x04, IN_DATA +0x10 (TRM SPRUJB4A).
LD8's pad has its input buffer off (PIN_OUTPUT), so IN_DATA reads 0 for it; OUT_DATA shows what the driver wrote.
SW5 reads 1 released and 0 pressed (pull-up on the board, active low)."""
import mmap, os, struct, sys, time
fd = os.open('/dev/mem', os.O_RDONLY | os.O_SYNC)
m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ, offset=0x600000)


def r(off):
    return struct.unpack('<I', m[off:off + 4])[0]


def line(t):
    b3, b2 = 0x10 + 0x28 * 3, 0x10 + 0x28 * 2
    return f'{t:6.2f} LD8 dir={r(b3)>>27&1} out={r(b3+4)>>27&1}   SW5 in={r(b2+0x10)>>26&1}'


secs = float(sys.argv[1]) if len(sys.argv) > 1 else 0
t0 = time.time()
print(line(0))
while time.time() - t0 < secs:
    time.sleep(0.1)
    print(line(time.time() - t0), flush=True)
