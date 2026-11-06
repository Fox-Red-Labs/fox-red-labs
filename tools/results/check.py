#!/usr/bin/env python3
"""Check data/results/*.csv against data/benchmark-suite.json and data/boards.csv. Exit 1 on any error.

A reader copy built from this repo (tools/publish/publish.py) leaves some
row sources out, such as build outputs and raw network logs. It lists them in
data/results/sources-elsewhere.txt; a listed source that is missing is counted, not an error.
The notebook itself has no such file, so every source here must exist.

  check.py [--root DIR]   check DIR/data instead of this repo's data (a scratch copy, for example the one
                          tools/benchmark/import_bundle.py builds before it writes); a source missing under
                          DIR is looked up in this repo too.
  check.py --gaps         also list measured rows with no runs count (a provenance gap: the count is
                          unknown, never assumed to be 1); the list is information and does not fail the check."""
import csv, datetime, json, math, re, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ROOT = Path(sys.argv[sys.argv.index("--root") + 1]).resolve() if "--root" in sys.argv else REPO
DATA = ROOT / "data"
COLS = ["board", "test", "variant", "status", "value", "value_hi", "runs", "clock_mhz",
        "conditions", "date", "source", "checked", "note"]
STATUS = {"measured", "computed", "not_run", "waiting", "does_not_fit", "not_supported", "not_applicable", "failed"}
CHECKED = {"crc", "vectors", "serial", "pin-read", "ha-state", "scan", "by-us", "self-report",
           "linker", "build-log", "host-clock", "readback", ""}
# rows render into every reader copy and the API; the feedback log they could cite does not (publishing principle 9)
# what a measured yes/no or status test may record besides a number (blank means the note says it)
CATEGORICAL = {"yes", "no", "partly", "partial"}
FEEDBACK_CITE = re.compile(r"\b(?:friction|our[- ]mistakes?)(?:[- ]log)?,?\s+(?:entr(?:y|ies)\s+)?\d{1,3}\b", re.I)


def load():
    tests = {t["id"]: t for t in json.loads((DATA / "benchmark-suite.json").read_text())["tests"]}
    boards = {r["board"]: r for r in csv.DictReader((DATA / "boards.csv").open())}
    rows = []
    for f in sorted((DATA / "results").glob("*.csv")):
        with f.open() as fh:
            reader = csv.DictReader(fh)
            if reader.fieldnames != COLS:
                rows.append((f, 1, None, f"header must be: {','.join(COLS)}"))
                continue
            for n, r in enumerate(reader, start=2):
                rows.append((f, n, r, None))
    return tests, boards, rows


def num(x):
    """A finite number: NaN, infinities and text that overflows a float are not values a reader can use."""
    try:
        return math.isfinite(float(x))
    except (TypeError, ValueError):
        return False


def real_date(x):
    if not re.fullmatch(r"\d{4}-\d\d-\d\d", x):
        return False
    try:
        datetime.date.fromisoformat(x)
        return True
    except ValueError:
        return False


def main():
    tests, boards, rows = load()
    errors, seen = [], {}
    elsewhere_file = DATA / "results" / "sources-elsewhere.txt"
    elsewhere = {l.strip() for l in elsewhere_file.read_text().splitlines() if l.strip() and not l.startswith("#")} if elsewhere_file.exists() else set()
    kept_out = 0
    for f, n, r, err in rows:
        where = f"{f.relative_to(ROOT)}:{n}"
        if err:
            errors.append(f"{where}: {err}")
            continue
        if None in r:   # an unquoted comma: csv puts the cut-off end of the row under no column, and every reader loses it
            errors.append(f"{where}: {len(r[None])} cell(s) past the last column; quote any cell that contains a comma")
            continue
        missing = [c for c in COLS if r.get(c) is None]
        if missing:   # a short row: csv fills the absent cells with None, and every reader would trip on them
            errors.append(f"{where}: row has {len(COLS) - len(missing)} of {len(COLS)} cells; missing {', '.join(missing)}")
            continue
        if r["board"] not in boards:
            errors.append(f"{where}: unknown board '{r['board']}' (add it to data/boards.csv)")
        if r["board"] != f.stem:
            errors.append(f"{where}: board '{r['board']}' in the file for '{f.stem}'")
        if r["test"] not in tests:
            errors.append(f"{where}: unknown test '{r['test']}' (add it to data/benchmark-suite.json)")
        if r["status"] not in STATUS:
            errors.append(f"{where}: status '{r['status']}' not one of {sorted(STATUS)}")
        if r["variant"] and not re.fullmatch(r"[a-z0-9][a-z0-9._+-]*", r["variant"]):
            errors.append(f"{where}: variant '{r['variant']}' must be lower case with hyphens")
        categorical = tests.get(r["test"], {}).get("unit") in ("yes/no", "status")
        if r["value"] and not num(r["value"]) and not (categorical and r["value"] in CATEGORICAL):
            errors.append(f"{where}: value '{r['value']}' must be a finite number"
                          + (f" or one of {sorted(CATEGORICAL)}" if categorical else ""))
        elif r["status"] == "measured" and not r["value"] and not categorical:
            errors.append(f"{where}: value: measured row needs a numeric value")
        if r["status"] == "measured":
            if not r["source"]:
                errors.append(f"{where}: measured row needs a source")
            if not r["checked"]:
                errors.append(f"{where}: measured row needs 'checked'")
        if r["status"] == "computed":
            # arithmetic on measured numbers that predicts something no run tested (data/README.md)
            if not num(r["value"]):   # arithmetic gives a number, never a yes or a no
                errors.append(f"{where}: value: computed row needs a numeric value")
            if not r["source"]:
                errors.append(f"{where}: computed row needs a source")
            if not r["note"]:
                errors.append(f"{where}: computed row needs a note saying how it was computed")
        if r["status"] == "not_applicable" and not r["note"]:
            errors.append(f"{where}: not_applicable row needs a note naming the block the chip does not have")
        if tests.get(r["test"], {}).get("superseded_by") and r["status"] == "measured":
            errors.append(f"{where}: test '{r['test']}' is superseded by {tests[r['test']]['superseded_by']}; "
                          "record the new tests, or mark this row computed")
        if r["value_hi"] and not num(r["value_hi"]):
            errors.append(f"{where}: value_hi '{r['value_hi']}' must be a finite number")
        if r["runs"]:
            # 0 records that nothing ran (not_run, waiting); a measured or computed value comes from at least one run
            low = 1 if r["status"] in ("measured", "computed") else 0
            if not re.fullmatch(r"\d+", r["runs"]) or int(r["runs"]) < low:
                errors.append(f"{where}: runs '{r['runs']}' must be a whole number"
                              + (" of 1 or more for a measured or computed row" if low else " (0 when nothing ran)"))
        if r["clock_mhz"] and not (num(r["clock_mhz"]) and float(r["clock_mhz"]) > 0):
            errors.append(f"{where}: clock_mhz '{r['clock_mhz']}' must be a positive number")
        if r["checked"] not in CHECKED:
            errors.append(f"{where}: checked '{r['checked']}' not one of {sorted(CHECKED - {''})}")
        if r["date"] and not real_date(r["date"]):
            errors.append(f"{where}: date '{r['date']}' must be a real YYYY-MM-DD date")
        for col in ("conditions", "note"):
            if FEEDBACK_CITE.search(r[col] or ""):
                errors.append(f"{where}: {col} cites a feedback-log entry number ('{FEEDBACK_CITE.search(r[col]).group(0)}'); "
                              "keep the cause in words and drop the entry number (the feedback log never ships)")
        if r["source"]:
            src = r["source"].split("#")[0].split(" ")[0]
            if not (ROOT / src).exists() and not (REPO / src).exists():
                if src in elsewhere:
                    kept_out += 1
                else:
                    errors.append(f"{where}: source '{src}' does not exist")
        key = (r["board"], r["test"], r["variant"])
        if key in seen:
            errors.append(f"{where}: duplicate of line {seen[key]} (same board, test and variant)")
        seen[key] = n
    for e in errors:
        print(e)
    if "--gaps" in sys.argv:
        gaps = [f"{f.relative_to(ROOT)}:{n}: {r['test']}" for f, n, r, _ in rows
                if r and None not in r and r.get("status") == "measured" and r.get("runs") == ""]
        for g in gaps:
            print(f"gap: {g}: measured, runs unknown")
        print(f"{len(gaps)} measured rows with no runs count")
    n_rows = sum(1 for _, _, r, _ in rows if r)
    print(f"{'FAIL' if errors else 'OK'}: {n_rows} rows, {len(errors)} errors"
          + (f" ({kept_out} rows cite a source kept in the source notebook)" if kept_out else ""))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
