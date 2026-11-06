#!/usr/bin/env python3
"""Turn one GitHub issue filed through a report form into a row in data/community/reports.csv.

  python3 tools/community/issue_to_row.py issue.json      the issue as GitHub's API returns it (number, title,
                                                           body, labels, created_at, html_url, user.login)
  gh issue view 12 --json number,title,body,labels,createdAt,url,author | python3 tools/community/issue_to_row.py -

A form issue's body is Markdown with one "### <Label>" heading per field, the answer under it. The labels
built, broke and prompt pick the kind. The row is written with status "reported"; a second run for the same
issue replaces its row rather than adding one. Board and project are normalized to the page ids when the text
starts with one (the forms ask for the id first). Never edits any other row. data/community/README.md has
the columns.
"""
import csv, json, pathlib, re, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
CSV = ROOT / "data" / "community" / "reports.csv"
sys.path.insert(0, str(ROOT / "tools" / "community"))
from check import COLS  # noqa: E402

FIELDS = {"Board": "board", "Project": "project", "Your machine": "os", "Tool versions": "tools", "Assistant and versions": "tools",
          "Hands-on minutes": "hands_on_min", "Waiting minutes": "waiting_min", "Did your number match ours?": "matched",
          "Your number": "value", "Photo, log or measurement": "value", "What did not work": "text", "What it produced": "value", "Anything else": "text", "What you tried": "text", "The prompt": "text",
          "Where it broke": "step", "The exact error text": "error", "Credit": "credit"}


def fields(body):
    out = {}
    for m in re.finditer(r"^### (.+?)\s*\n(.*?)(?=^### |\Z)", body or "", re.S | re.M):
        label, val = m.group(1).strip(), m.group(2).strip()
        val = re.sub(r"^```[a-z]*\n|\n```$", "", val).strip()
        if val in ("_No response_", "None"):
            val = ""
        if label in FIELDS:
            out[FIELDS[label]] = val
    return out


def one_line(s, n=400):
    s = " ".join((s or "").split())
    return s[:n]


def norm_id(s):
    m = re.match(r"\s*([a-z0-9][a-z0-9-]*)", s or "")
    return m.group(1) if m else ""


def row_from(issue):
    labels = {l["name"] if isinstance(l, dict) else l for l in issue.get("labels", [])}
    kind = next((k for k in ("built", "broke", "prompt") if k in labels), "")
    if not kind:
        sys.exit("the issue has no built, broke or prompt label; nothing written")
    f = fields(issue.get("body", ""))
    matched = {"Yes": "yes"}.get(f.get("matched", ""), "no" if f.get("matched", "").startswith("No") else "")
    user = issue.get("user", {}).get("login") or issue.get("author", {}).get("login") or ""
    credit = f.get("credit", "").strip().lstrip("@")
    if credit and user and credit.lower() != user.lower():
        credit = user   # the handle on the row is the account that filed it, not a name typed in
    r = {c: "" for c in COLS}
    r.update({"id": f"issue-{issue['number']}", "date": (issue.get("created_at") or issue.get("createdAt") or "")[:10], "kind": kind,
              "board": norm_id(f.get("board", "")), "project": norm_id(f.get("project", "")), "os": one_line(f.get("os", ""), 80),
              "tools": one_line(f.get("tools", ""), 160), "hands_on_min": one_line(f.get("hands_on_min", ""), 10), "waiting_min": one_line(f.get("waiting_min", ""), 10),
              "matched": matched, "value": one_line(f.get("value", ""), 200), "step": one_line(f.get("step", ""), 80), "error": one_line(f.get("error", ""), 300),
              "text": one_line(f.get("text", "")), "status": "reported", "link": issue.get("html_url") or issue.get("url") or "", "credit": credit})
    return r


def main(path):
    issue = json.load(sys.stdin if path == "-" else open(path))
    r = row_from(issue)
    with CSV.open(newline="") as fh:
        rows = [x for x in csv.DictReader(fh) if x["id"] != r["id"]]
    rows.append(r)
    with CSV.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=COLS, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {r['id']} ({r['kind']}, {r['board']}, {r['project'] or 'no project'})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "-")
