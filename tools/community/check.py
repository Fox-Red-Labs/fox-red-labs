#!/usr/bin/env python3
"""Validate data/community/reports.csv (data/community/README.md has the columns). Prints OK or the problems."""
import csv, json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
KINDS = {"built", "broke", "prompt"}
STATUS = {"reported", "reproduced", "fixed", "not_reproduced", "added"}
COLS = ["id", "date", "kind", "board", "project", "os", "tools", "hands_on_min", "waiting_min", "matched", "value", "step", "error", "text", "status", "link", "credit"]


def rows():
    with (ROOT / "data" / "community" / "reports.csv").open(newline="") as fh:
        r = csv.DictReader(fh)
        return r.fieldnames, list(r)


def main():
    fields, rs = rows()
    probs = []
    if fields != COLS:
        probs.append(f"columns are {fields}, expected {COLS}")
    boards = set(json.loads((ROOT / "site" / "data" / "code.json").read_text())["boards"])
    pj = json.loads((ROOT / "site" / "data" / "projects.json").read_text())
    slugs = {p["slug"] for p in pj["projects"]}
    seen = set()
    for i, r in enumerate(rs, 2):
        if r["id"] in seen:
            probs.append(f"line {i}: duplicate id {r['id']}")
        seen.add(r["id"])
        if r["kind"] not in KINDS:
            probs.append(f"line {i}: kind {r['kind']!r}")
        if r["status"] not in STATUS:
            probs.append(f"line {i}: status {r['status']!r}")
        if r["board"] and r["board"] not in boards:
            probs.append(f"line {i}: board {r['board']!r} is not a board page id")
        if r["project"] and r["project"] not in slugs:
            probs.append(f"line {i}: project {r['project']!r} is not a project slug")
        if r["kind"] == "built" and not (r["hands_on_min"] or r["waiting_min"]):
            probs.append(f"line {i}: a built report needs a time")
        if r["kind"] == "broke" and not r["error"]:
            probs.append(f"line {i}: a broke report needs the error text")
        if r["status"] == "fixed" and not r["link"]:
            probs.append(f"line {i}: fixed needs the page that carries the fix in link")
    for p in probs:
        print("problem:", p)
    print("OK" if not probs else f"{len(probs)} problem(s)", f"({len(rs)} reports)")
    return 1 if probs else 0


if __name__ == "__main__":
    sys.exit(main())
