#!/usr/bin/env python3
"""Expected frames per second on the LP-MSPM33C321A from the emulator's instruction counts (speed/emu_pgb.py outputs).
  python3 speed/model.py <dir with base.txt acc0.txt lut.txt o2.txt zero.txt skip.txt> [--cpi 1.64]
This is arithmetic, not a measurement. Constants and where they come from:
  CPI 1.64 at 32 MHz: the 2026-10-20 board run of the first image (146.6 to 194.5 ms a frame over the first 146 frames, of which 29.6 ms
    was the screen) against 2.75 million counted instructions a frame for the same frames (range 1.36 to 1.92 over the spread of those frames).
  x1.29 at 160 MHz: flash needs 2 wait states there; CoreMark per MHz falls from 4.38 to 3.40 (mspm33c321a/bench/RESULTS-mspm33c321a.md).
  The first two images block on the screen (29.6 ms a frame at 16 MHz, measured); later ones stream and overlap it with the CPU.
  Screen: 32 KB frame by DMA took 19.5 ms at 16 MHz (19 percent over the bare wire time); 128 row starts add about 0.4 ms. At 160 MHz the
    SPI input clock is 80 MHz and the generated divider gives 13.33 MHz, so 23.4 + 0.4 ms.
  The CPU runs 22 percent slower while the DMA holds the bus (same results file)."""
import sys, re, os
d = sys.argv[1]; cpi = float(sys.argv[sys.argv.index('--cpi') + 1]) if '--cpi' in sys.argv else 1.64
def mean(name):
    t = open(os.path.join(d, name + '.txt')).read()
    return float(re.search(r'mean (\d+)', t).group(1))
rows = []
def per_clock(mhz, W, k):
    out = {}
    for name in ('base', 'acc0', 'lut', 'o2', 'zero'):
        c = mean(name) * cpi * k / (mhz * 1e3)          # ms of CPU work per frame, all frames drawn
        out[name] = (mean(name), c, max(1.22 * c, W))   # frame time when the screen runs alongside; the first image blocked: c + W
    WB = 29.6   # the first images sent 32 lines by blocking calls: 29.6 ms a frame measured on the board at 16 MHz
    out['base'] = (out['base'][0], out['base'][1], out['base'][1] + WB)
    out['acc0'] = (out['acc0'][0], out['acc0'][1], out['acc0'][1] + WB)
    # the table scaler streams: screen and CPU overlap
    m_full = mean('zero'); m_skip = mean('skip'); m_nodraw = max(2 * m_skip - m_full, 0)
    c_pair = (m_full + m_nodraw) * cpi * k / (mhz * 1e3)
    t_pair = max(1.22 * c_pair, W)
    out['skip'] = (m_skip, c_pair / 2, t_pair / 2)
    return out
print(f"CPI {cpi}")
for mhz, W, k in ((32, 19.9, 1.0), (160, 23.8, 1.29)):
    print(f"\n{mhz} MHz, screen {W} ms per drawn frame")
    o = per_clock(mhz, W, k)
    for name in ('base', 'acc0', 'lut', 'o2', 'zero', 'skip'):
        if mhz == 160 and name in ('base', 'acc0'): continue   # those images never ran at 160 MHz
        ins, c, t = o[name]
        print(f"  {name:5s} {ins:9.0f} instr  cpu {c:6.1f} ms  frame {t:6.1f} ms  {1000 / t:5.1f} fps  {100000 / t / 59.73:5.1f} percent of the console")
