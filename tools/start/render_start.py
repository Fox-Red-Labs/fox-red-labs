#!/usr/bin/env python3
"""Write the board table in START.md from the data the site already uses, so it cannot drift.

  python3 tools/start/render_start.py           rewrite the table between the start:boards markers
  python3 tools/start/render_start.py --check   exit 1 if START.md is not what this would write, or the home page's Copy box
                                                carries a different prompt from START.md's

Both also write or check the setup coverage (tools/start/route_summary.py: docs/setup-coverage.md and the
"Other boards" block in README.md and the export's README template), so site_check.py covers it through this file.

Rows: every board in site/data/code.json with an AGENTS.md. Name as the site shows it (the same lookup as
tools/render_start_ai.py), folder, whether tools/benchmark/backup.py has a tested backup recipe for it
(tools/benchmark/recipes.json), and the projects: tested by recipe (site/data/projects.json, level "tested"),
else what ran on the board (site/data/boards/<id>.json, status works), plus those expected to work and not run here.
"""
import html
import json
import pathlib
import re
import sys

import route_summary

ROOT = pathlib.Path(__file__).resolve().parents[2]
DATA = ROOT / "site" / "data"
START = ROOT / "START.md"
OPEN, CLOSE = "<!-- start:boards (written by tools/start/render_start.py; do not edit by hand) -->", "<!-- /start:boards -->"


def rows():
    pj = json.loads((DATA / "projects.json").read_text())
    code = json.loads((DATA / "code.json").read_text())["boards"]
    recipes = json.loads((ROOT / "tools" / "benchmark" / "recipes.json").read_text())["boards"]
    names = {b["page"][6:-5]: b["name"] for b in pj["boards"]}
    names.update({s["part"].lower(): s["name"] for s in json.loads((DATA / "spectrum.json").read_text())["stops"] if s["part"].lower() not in names})
    for f in sorted((DATA / "boards").glob("*.json")):   # boards with their own page data carry the printed name
        names.setdefault(f.stem, json.loads(f.read_text()).get("board") or f.stem)
    pids = [b["id"] for b in pj["boards"]]
    tested = {}
    for p in pj["projects"]:
        for route in (p.get("recipe") or {}).get("compat", {}).values():
            for bid, v in route.items():
                if v.get("level") in ("tested", "expected"):
                    tested.setdefault(bid, {"tested": [], "expected": []})
                    lst = tested[bid][v["level"]]
                    if p["t"] not in lst and not (v["level"] == "expected" and p["t"] in tested[bid]["tested"]):
                        lst.append(p["t"])
    out = []
    for key, c in sorted(code.items(), key=lambda kv: names.get(kv[0], kv[0])):
        if not c.get("agents"):
            continue
        pid = next((i for i in pids if key == i or key.endswith(i)), None)
        backup = "tested (`backup.py`)" if "backup" in recipes.get(key, {}) else "see its `AGENTS.md`"
        tb = tested.get(pid, {"tested": [], "expected": []})
        bj = DATA / "boards" / f"{key}.json"
        ran = [x["t"] for x in (json.loads(bj.read_text()).get("projects") or []) if x.get("st") == "works" and not x["t"].startswith("Blink")] if bj.exists() else []
        exp = [x for x in tb["expected"] if x not in tb["tested"]]
        projects = "; ".join(filter(None, [", ".join(tb["tested"]) and "tested: " + ", ".join(tb["tested"]),
                                           not tb["tested"] and ran and "ran on this board: " + ", ".join(ran[:5]),
                                           exp and "expected, not run here: " + ", ".join(exp[:4])])) or "its README's ten-minute path"
        out.append(f"| {names.get(key, key)} | `{key}` | `{c['folder']}` | {backup} | {projects} |")
    return out


def table():
    head = ["Each row: the name printed on the board, the id the tools use, the folder to fetch, how to back it up, and the projects tested on it (or expected to work and not run here) with this route.",
            "", "| Board | Id | Folder | Backup | Projects |", "|---|---|---|---|---|"]
    return "\n".join(head + rows())


def main():
    stale = route_summary.stale_files()
    if "--check" in sys.argv:
        if stale:
            print("; ".join(f"{rel}: setup coverage is out of date" for rel in stale) + "; run python3 tools/start/render_start.py")
            return 1
    else:
        for rel, t in stale.items():
            (ROOT / rel).write_text(t, encoding="utf-8")
            print(f"wrote {rel}")
    text = START.read_text()
    m = re.search(re.escape(OPEN) + r".*?" + re.escape(CLOSE), text, re.S)
    if not m:
        sys.exit("START.md: the start:boards markers are missing")
    new = text[:m.start()] + OPEN + "\n" + table() + "\n" + CLOSE + text[m.end():]
    if "--check" in sys.argv:
        prompt = re.search(r"\*\*The prompt a person pastes into Claude Code\*\*.*?```\n(.*?)\n```", text, re.S)
        home = re.search(r"<!-- start:prompt -->(.*?)<!-- /start:prompt -->", (ROOT / "site" / "index.html").read_text(), re.S)
        if prompt and home and html.unescape(home.group(1)).strip() != prompt.group(1).strip():
            print("site/index.html: the prompt in the Copy box differs from START.md; copy START.md's prompt (and its Open in Claude Code link) into the page")
            return 1
        if new != text:
            print("START.md board table is out of date; run python3 tools/start/render_start.py")
            return 1
        print("START.md current")
        return 0
    if new != text:
        START.write_text(new)
    print(f"START.md: {len(rows())} boards")
    return 0


if __name__ == "__main__":
    sys.exit(main())
