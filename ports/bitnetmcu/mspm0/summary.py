#!/usr/bin/env python3
"""One line per board run: answers, right of 10, mean cycles and ms, from each build folder's ram.txt."""
import glob, os
here = os.path.dirname(os.path.abspath(__file__))
for d in sorted(glob.glob(os.path.join(here, "build-*"))):
    f = os.path.join(d, "ram.txt")
    if not os.path.exists(f):
        print(f"{os.path.basename(d):32} not run"); continue
    w = [int(x, 16) for l in open(f) for x in l.split(":")[1].split()]
    magic, clk, ovh, right = w[:4]; pred, cyc = w[4:14], w[14:24]
    ok = "done" if magic == 0xB17E0001 else "NOT DONE"
    print(f"{os.path.basename(d):32} {ok}  {''.join(map(str, pred))}  {right}/10  "
          f"{sum(cyc)//10:>9,} cycles  {sum(cyc)/10/clk*1000:6.2f} ms at {clk//1000000} MHz")
