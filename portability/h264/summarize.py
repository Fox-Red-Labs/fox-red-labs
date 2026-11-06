#!/usr/bin/env python3
"""summarize.py <clock_mhz> <log> [<log>...]: median over passes per stream from H264R lines (console logs, or parse_rec.py output).
Prints: stream, WxH, passes, frames, status, cycles/frame (decode only), cycles/pixel, fps at the clock, RGB565 cycles/frame, heap, input buffer, CRC triple."""
import re, sys, statistics as st
clk = float(sys.argv[1]) * 1e6; rows = {}
for fn in sys.argv[2:]:
    for l in open(fn, errors="replace"):
        m = re.search(r"H264R s=(\S+) p=(\d+) st=(\d+) (\d+)x(\d+) fr=(\d+) crc=(\w+)/(\w+)/(\w+) rgb=(\w+) cyc=(\d+) idr=(\d+) p=(\d+) pmax=(\d+) rgbc=(\d+) heap=(\d+) in=(\d+) allocs=(\d+) conc=(\d+)", l)
        if m: rows.setdefault(m.group(1), []).append(m.groups())
for s, r in rows.items():
    g = r[0]; w, h, fr = int(g[3]), int(g[4]), int(g[5])
    if int(g[2]) != 0: print("%-24s REFUSED status=%s rc=%d heap=%s" % (s, g[2], int(g[18]) - 1000, g[15])); continue
    cyc = [int(x[10]) for x in r]; rgb = [int(x[14]) for x in r]; pmax = [int(x[13]) for x in r]
    c = st.median(cyc); pf = c / fr; px = w * h
    print("%-24s %4dx%-4d n=%d fr=%d  cyc/frame=%9.0f (%.2fM)  cyc/px=%6.1f  fps=%7.1f  rgb/frame=%9.0f (%.1f/px)  fps_with_rgb=%6.1f  spread=%d  heap=%s in=%s  crc=%s/%s/%s rgb=%s" % (
        s, w, h, len(r), fr, pf, pf / 1e6, pf / px, clk / pf, st.median(rgb) / fr, st.median(rgb) / fr / px, clk / (pf + st.median(rgb) / fr), max(cyc) - min(cyc), g[15], g[16], g[6], g[7], g[8], g[9]))
