#!/usr/bin/env python3
"""The site's data, read-only, for the MCP server (BRIEF.md step 8: readable by machines as well as people).

Boards (data/boards.csv), benchmark rows (data/results/<board>.csv), builds (site/data/projects.json) and parts from
any maker (data/parts.json), served as they are stored. Nothing here touches a board, the network or a file outside
the repository; every function returns a core.Result.
"""
import csv
import json
import re
from pathlib import Path

from core import Result

ROOT = Path(__file__).resolve().parents[2]
WORD = {"yes": "Works", "partial": "Could work", "builds": "Should work", "no": "Not for this chip"}


def _json(rel):
    return json.loads((ROOT / rel).read_text(encoding="utf-8"))


def _whole_repo(fn):
    """A sparse checkout (tools/frl/starter-files.txt) has no site data: say so instead of failing."""
    def run(*a, **k):
        try:
            return fn(*a, **k)
        except FileNotFoundError as e:
            return Result.make("refused", "FRL-E070", f"this copy has no {Path(e.filename).relative_to(ROOT) if e.filename else 'site data'}",
                               nxt="clone the whole repository for the site_* tools")
    return run


@_whole_repo
def boards():
    rows = [{k: r[k] for k in ("board", "name", "vendor", "core", "clock_mhz", "flash_kb", "ram_kb")} for r in csv.DictReader(open(ROOT / "data/boards.csv"))]
    for r in rows:
        page = ROOT / "site" / f"board-{r['board']}.html"
        r["page"] = f"site/{page.name}" if page.exists() else None
        r["results"] = f"data/results/{r['board']}.csv" if (ROOT / "data/results" / f"{r['board']}.csv").exists() else None
    return Result.good(f"{len(rows)} boards", boards=rows)


@_whole_repo
def results(board, test=None, limit=200):
    if not isinstance(board, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]*", board):
        return Result.make("refused", "FRL-E070", "use a board id from site_boards, not a file path")
    f = ROOT / "data/results" / f"{board}.csv"
    if f.resolve().parent != ROOT.resolve() / "data/results":
        return Result.make("refused", "FRL-E070", "results must be a CSV inside this repository's data/results folder")
    if not f.exists():
        known = sorted(p.stem for p in (ROOT / "data/results").glob("*.csv"))
        return Result.make("refused", "FRL-E070", f"no results file for {board!r}", nxt=f"one of: {', '.join(known)}")
    with f.open(encoding="utf-8") as source:
        rows = [r for r in csv.DictReader(source) if not test or r["test"] == test]
    return Result.good(f"{len(rows)} rows for {board}" + (f", test {test}" if test else ""), rows=rows[:limit], truncated=len(rows) > limit,
                       method="docs/methodology.md", source=f"data/results/{board}.csv")


@_whole_repo
def builds():
    pj = _json("site/data/projects.json")
    out = []
    for p in pj["projects"]:
        cells = {**p.get("cells", {}), **p.get("also", {})}
        out.append({"slug": p["slug"], "title": (p.get("recipe") or {}).get("outcome") or p["t"], "has_steps": bool(p.get("recipe")),
                    "works_on": [b for b, c in cells.items() if c.get("st") == "yes"],
                    "on_the_bench": [b for b, c in cells.items() if c.get("st") in ("partial", "builds")],
                    "page": f"site/project-{p['slug']}.html", "text": f"docs/builds/{p['slug']}.md"})
    return Result.good(f"{len(out)} builds", builds=out)


@_whole_repo
def build(slug):
    pj = _json("site/data/projects.json")
    p = next((x for x in pj["projects"] if x["slug"] == slug), None)
    if not p:
        return Result.make("refused", "FRL-E070", f"no build {slug!r}", nxt="call site_builds for the list")
    cells = {**p.get("cells", {}), **p.get("also", {})}
    status = {b: {"status": WORD[c["st"]], "why": c.get("why", ""), "source": c.get("source", "")} for b, c in cells.items() if c.get("st") in WORD}
    recipe = p.get("recipe")
    versions = []
    for v in (recipe or {}).get("variants", []):
        versions.append({"id": v["id"], "name": v["name"], "boards": v.get("boards", []), "parts": v.get("parts", []),
                         "pins": (v.get("wiring") or {}).get("pins", []), "steps": v.get("steps", []),
                         "expect": v.get("expect", ""), "limits": (v.get("not_shows") or []) + (v.get("limits") or [])})
    md = ROOT / "docs/builds" / f"{slug}.md"
    return Result.good(p["t"], slug=slug, boards=status, versions=versions,
                       text=f"docs/builds/{slug}.md" if md.exists() else None, page=f"site/project-{slug}.html")


@_whole_repo
def parts(query=None):
    d = _json("data/parts.json")
    ps = d["parts"] if isinstance(d, dict) else d
    q = (query or "").lower()
    out = [{"id": p["id"], "name": p["name"], "category": p.get("cat"), "what": p.get("what"), "connect": p.get("connect"),
            "buy": p.get("buy", []), "used_in": p.get("used_in", [])}
           for p in ps if not q or q in (p["name"] + " " + p.get("what", "") + " " + p["id"]).lower()]
    return Result.good(f"{len(out)} parts" + (f" matching {query!r}" if query else ""), parts=out, catalog="docs/parts-catalog.md")


# ---- content tools (System Lab step 6, 2026-10-17): what an assistant could not read through MCP before ----
# A document is served only if the public export would carry it: the same manifest (tools/publish/manifest.json include and
# exclude globs, with its exceptions) decides, so there is no second hand-written list to drift. The path must be a file git
# tracks, spelled exactly as tracked (a case-insensitive disk must not let a private path through under another spelling), and a Markdown file.
DOC_MAX = 400_000


def _publish():
    import sys as _sys
    pub = str(ROOT / "tools" / "publish")
    if pub not in _sys.path:
        _sys.path.insert(0, pub)
    import publish  # noqa: E402
    return publish


def _tracked():
    import subprocess
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z"], capture_output=True, text=True, check=True).stdout
    return set(out.split("\0")) - {""}


def _safe_rel(rel):
    """A repository-relative path that stays inside the repository, or None."""
    if not isinstance(rel, str) or not rel or rel.startswith(("/", "~")) or ".." in rel.split("/") or "\\" in rel:
        return None
    if any(part.startswith(".") for part in rel.split("/")):
        return None
    p = (ROOT / rel).resolve()
    try:
        p.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return p


def doc_allowed(rel):
    """True when the public export would carry this exact tracked Markdown file."""
    if not rel.endswith(".md") or rel not in _tracked():
        return False
    pub = _publish()
    m = pub.load_manifest("manifest.json")
    if not pub.matches(rel, m["include"]):
        return False
    if pub.matches(rel, m.get("exclude", [])) and not pub.matches(rel, m.get("exclude_except", [])):
        return False
    return True


@_whole_repo
def doc(path):
    p = _safe_rel(path)
    if p is None:
        return Result.make("refused", "FRL-E070", "give a path inside this repository, like mspm0l1306/AGENTS.md or docs/methodology.md")
    rel = path.strip("/")
    if not doc_allowed(rel):
        return Result.make("refused", "FRL-E070", f"{rel} is not a document this connection serves",
                           nxt="a Markdown file the public export carries: a board folder's AGENTS.md, FAQ.md or README.md, or a page under docs/, portability/, ports/ or recipes/; spell the path as the repository does")
    text = p.read_text(errors="replace")
    return Result.good(f"{rel}: {len(text)} characters", path=rel, text=text[:DOC_MAX], truncated=len(text) > DOC_MAX)


@_whole_repo
def ports():
    d = _json("site/data/ports.json")
    rows = d.get("ports", d)
    return Result.good(f"{len(rows)} ports", ports=rows, note=d.get("note", ""), source="site/data/ports.json", page="site/ports.html")


@_whole_repo
def support_matrix(family=None):
    d = _json("data/support-matrix.json")
    out = {k: v for k, v in d.items() if k != "note"}
    if family:
        fams = {k: v for k, v in (d.get("families") or {}).items() if family.lower() in k.lower()} if isinstance(d.get("families"), dict) else d.get("families")
        out["families"] = fams
    return Result.good("works-with grid", states=d.get("note", ""), **out, source="data/support-matrix.json", page="site/portability.html")


@_whole_repo
def questions():
    d = _json("data/questions.json")
    return Result.good(f"{len(d.get('questions', []))} questions", questions=d.get("questions", []), note=d.get("note", ""), source="data/questions.json", test="tools/ai_runs/readability/README.md")


@_whole_repo
def api_index():
    d = _json("site/api/v1/index.json")
    return Result.good(f"API v{d.get('schema_version')}, generated {d.get('generated')}", **d, source="site/api/v1/index.json")
