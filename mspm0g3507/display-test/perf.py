#!/usr/bin/env python3
"""Decode a perf.c memory dump (results/<name>.bin) into a table."""
import struct, sys
b = open(sys.argv[1], "rb").read()
SPEEDS = tuple(int(x) for x in (sys.argv[2] if len(sys.argv) > 2 else "8,16").split(","))
w = struct.unpack('<31I', b[:124])
rb = b[124:124 + 386]
off = 124 + 386 + 2                      # align to 4 for loop_cyc_x1000
loop = struct.unpack('<I', b[off:off + 4])[0]
pwr = b[off + 4]
print(f"magic {w[0]:08x}  boot to screen ready {w[1]/1000:.1f} ms  to first full frame {w[2]/1000:.1f} ms  power mode 0x{pwr:02X}")
names = ['CPU, 8-bit frames', 'CPU, 16-bit frames', 'DMA, 8-bit frames', 'DMA, 16-bit frames']
for s, mhz in enumerate(SPEEDS):
    for m in range(4):
        f, b100, b32 = w[3 + s * 12 + m * 3: 6 + s * 12 + m * 3]
        free = f" CPU free {w[27 + s * 2 + m - 2]}%" if m >= 2 else ""
        print(f"{mhz:2d} MHz  {names[m]:20s} full {f/1000:6.1f} ms ({1e6/f:4.1f} fps)  100x100 {b100/1000:5.2f} ms  32x32 {b32/1000:5.2f} ms{free}")
for s, mhz in enumerate(SPEEDS):
    raw = rb[s * 193:(s + 1) * 193]
    # RAMRD in 16-bit color mode returns one dummy byte, then RGB565, 2 bytes per pixel
    ok = sum(int.from_bytes(raw[1 + 2 * i: 3 + 2 * i], 'big') == (0x1234 ^ ((i * 0x0841) & 0xFFFF)) for i in range(64))
    print(f"{mhz:2d} MHz write: {ok} of 64 pixels read back exactly from the screen's memory")
print(f"DMA wait loop: {loop/1000:.1f} cycles per pass")
