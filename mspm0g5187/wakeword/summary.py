#!/usr/bin/env python3
"""Print the replay results as a table: python3 summary.py"""
import re, glob
for f in sorted(glob.glob("results/replay-*.txt")):
    for l in open(f):
        if l.startswith("RESULT"):
            print(f.split("/")[1], "|", l.strip())
