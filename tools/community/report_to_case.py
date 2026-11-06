#!/usr/bin/env python3
"""Turn one reader report (a row of data/community/reports.csv) into a regression case and a FAQ draft.

  python3 tools/community/report_to_case.py issue-12            writes data/community/cases/issue-12.json
                                                                and prints a FAQ draft for the board's FAQ.md

The case keeps the reporter's configuration as they gave it (board, project, OS, tool versions, step, the
error) and their consent as recorded on the row: the report was filed as a public issue, so its words may
be quoted; `credit` is their choice to be named (empty means do not name them). Machine details they did
not mean to share are removed before anything is written: home-folder paths, IP addresses and probe serial
numbers. A case stays separate from our measurements (it never becomes a row in data/results/) until we
reproduce it; then its `status` in reports.csv changes and the fix goes where a reader hits it.

When the error carries an frl code (FRL-E0xx), the case names it and the matching entry in
docs/frl-errors.md, and suggests the regression test to add to tools/tests/test_frl.py.
"""
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORTS = ROOT / "data" / "community" / "reports.csv"
CASES = ROOT / "data" / "community" / "cases"


def scrub(text):
    """Remove the reporter's private machine details from free text."""
    t = str(text or "")
    t = re.sub(r"(/Users/|/home/)[^/\s\"']+", r"\1<user>", t)
    t = re.sub(r"([A-Za-z]:\\\\?Users\\\\?)[^\\\s\"']+", r"\1<user>", t)
    t = re.sub(r"\b(\d{1,3}\.){3}\d{1,3}\b", "<ip>", t)
    t = re.sub(r"(adapter serial|--probe|probe|Serial Num:?)\s+([A-Za-z0-9]{6,})", lambda m: f"{m.group(1)} <serial>", t)
    t = re.sub(r"usbmodem[A-Za-z0-9]+", "usbmodem<serial>", t)
    return t


def frl_code(error):
    m = re.search(r"FRL-E\d{3}", error or "")
    return m.group(0) if m else None


def case_for(row):
    err, text = scrub(row.get("error")), scrub(row.get("text"))
    code = frl_code(err)
    case = {
        "report": row["id"], "link": row.get("link", ""), "date": row.get("date", ""),
        "consent": {"quote": "yes: filed as a public issue", "name": row.get("credit") or "no: not named"},
        "config": {k: row.get(k, "") for k in ("board", "project", "os", "tools", "step")},
        "error": err, "their_words": text,
        "frl_code": code, "faq": f"docs/frl-errors.md#{code.lower()}" if code else None,
        "regression_test": (f"tools/tests/test_frl.py: reproduce {code} with the fake OpenOCD (fixtures/frl) "
                            f"using the config above, and assert the code and the next step" if code else
                            "reproduce on the bench first; then add a test where the failure can be simulated"),
        "status": row.get("status", "reported"),
        "note": "A reader's report, not our measurement. It stays here until reproduced.",
    }
    return case


def faq_draft(case):
    q = case["error"].splitlines()[0][:140] if case["error"] else f"Step {case['config']['step']} failed"
    lines = [f"### {q}. What do I do?", "",
             f"Reported by a reader on {case['date']} ({case['config']['os'] or 'computer not given'}; {case['config']['tools'] or 'tools not given'})"
             f"{', credited as ' + case['consent']['name'] if not case['consent']['name'].startswith('no:') else ''}. "
             "Not reproduced on our bench yet. **Tag: Unknown.**"]
    if case["faq"]:
        lines.append(f"\nThe code is explained in [docs/frl-errors.md](../{case['faq']}).")
    return "\n".join(lines)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    rows = list(csv.DictReader(REPORTS.open())) if REPORTS.exists() else []
    row = next((r for r in rows if r["id"] == sys.argv[1]), None)
    if not row:
        print(f"no report {sys.argv[1]!r} in {REPORTS.relative_to(ROOT)}")
        return 2
    case = case_for(row)
    CASES.mkdir(parents=True, exist_ok=True)
    out = CASES / f"{row['id']}.json"
    out.write_text(json.dumps(case, indent=1) + "\n")
    print(f"wrote {out.relative_to(ROOT)}\n\nFAQ draft for {row.get('board') or '<board>'}/FAQ.md (add only after reproducing it):\n")
    print(faq_draft(case))
    return 0


if __name__ == "__main__":
    sys.exit(main())
