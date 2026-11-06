#!/usr/bin/env python3
"""Merge harness CSVs (shards, partial reruns) into one file per SDK.
  merge_csv.py <out.csv> <in.csv> [<in.csv> ...]   later files override earlier ones on (project, config)."""
import csv, sys

out, ins = sys.argv[1], sys.argv[2:]
rows = {}
for p in ins:
    for r in csv.DictReader(open(p, newline="")):
        rows[(r["project"], r["config"])] = r
fields = list(next(iter(rows.values())).keys())
with open(out, "w", newline="") as f:
    w = csv.DictWriter(f, fields, lineterminator="\n"); w.writeheader()
    for k in sorted(rows):
        w.writerow(rows[k])
print(out, len(rows), "builds")
