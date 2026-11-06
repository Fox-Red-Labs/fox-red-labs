#!/usr/bin/env python3
"""Setup coverage, derived from the registries so no list of supported boards is kept by hand.

  python3 tools/start/route_summary.py           write docs/setup-coverage.md and the generated blocks in README.md
                                                 and tools/publish/templates/README.md
  python3 tools/start/route_summary.py --check   exit 1 if any of them differs from what this would write

Sources, each the one owner of its fact:
  data/routes.json             guided routes (`./start.sh <board> <job>`): recipes, their status, tools, probe, check
  data/results/<board>.csv     hardware evidence: rows from the route's own runs (variant starting `frl`)
  tools/benchmark/recipes.json backup recipe per board
  data/recovery.json           recovery methods with a run behind them
  site/data/code.json          board folders with an AGENTS.md (the manual path)
  data/boards.csv              every board with results
A route listed in data/routes.json, a build, a fake-probe test and a hardware run stay different things here:
only a results row counts as a hardware run, and a route with none says so. Standard library only.
"""
import csv
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = "docs/setup-coverage.md"
BLOCK = ("<!-- routes:other-boards (written by tools/start/route_summary.py; do not edit by hand) -->", "<!-- /routes:other-boards -->")
README_FILES = ("README.md", "tools/publish/templates/README.md")
# which results rows show a recipe ran on hardware, by the recipe's check: the LED pattern or the serial message
EVIDENCE_TESTS = {"pattern": ("blink_flash_repeat", "blink_flash"), "uart": ("message_flash_repeat", "message_flash")}
JOB = {"blink-pattern": "blink", "print-message": "print-a-message"}


def _json(root, rel):
    return json.loads((root / rel).read_text(encoding="utf-8"))


def _rows(root, bid):
    f = root / "data" / "results" / f"{bid}.csv"
    if not f.exists():
        return []
    with f.open(encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def hardware_runs(rows, verify_kind):
    """Measured rows from the route's own runs for this kind of check, most runs first."""
    tests = EVIDENCE_TESTS.get(verify_kind, ())
    hits = [r for r in rows if r["test"] in tests and r["status"] == "measured" and r["variant"].startswith("frl")]
    return sorted(hits, key=lambda r: (tests.index(r["test"]), -int(r["runs"] or 0)))


def recipe_level(recipe, runs):
    """('works' | 'could', words). 'works' only with a hardware run row; the registry status alone is not one."""
    if runs:
        r = runs[0]
        n = int(r["runs"] or 0)
        words = (f"{n} runs on hardware" if n > 1 else "1 run on hardware") + (f", {r['date']}" if r["date"] else "")
        if recipe.get("status") != "tested":   # the registry's own caveat stays visible (limits per board: docs/frl.md)
            words += f"; route marked {recipe.get('status', 'unknown')} in data/routes.json, limits in docs/frl.md"
        return "works", words
    if recipe.get("status") == "tested":
        return "could", "route marked tested in data/routes.json, but no hardware run row in data/results"
    return "could", f"route status '{recipe.get('status', 'unknown')}', no hardware run row in data/results"


def summary(root=ROOT):
    """One dict per board with results or a folder: guided routes first, then manual paths."""
    routes = _json(root, "data/routes.json")
    code = _json(root, "site/data/code.json")["boards"]
    aliases = _json(root, "data/board-ids.json")["aliases"] if (root / "data/board-ids.json").exists() else {}
    backups = _json(root, "tools/benchmark/recipes.json")["boards"] if (root / "tools/benchmark/recipes.json").exists() else {}
    recovery = _json(root, "data/recovery.json")["boards"] if (root / "data/recovery.json").exists() else {}
    with (root / "data" / "boards.csv").open(encoding="utf-8") as fh:
        boards = {r["board"]: r for r in csv.DictReader(fh)}
    code_by_real = {aliases.get(k, k): v for k, v in code.items()}
    out = []
    for bid in sorted(set(boards) | set(routes["boards"]) | set(code_by_real)):
        rt = routes["boards"].get(bid)
        c = code_by_real.get(bid, {})
        near = sorted(p.parent.name for p in root.glob(f"{bid}*/README.md"))   # cc3200 lives in cc3200-launchxl/
        folder = c.get("folder") or (bid if (root / bid / "README.md").exists() else (near[0] if len(near) == 1 else ""))
        rec = recovery.get(bid, {})
        e = {"id": bid, "name": rt["kit"] if rt else (boards.get(bid, {}).get("name") or bid), "folder": folder,
             "readme": c.get("readme") or (f"{folder}/README.md" if folder and (root / folder / "README.md").exists() else ""),
             "agents": bool(c.get("agents")), "backup": "backup" in backups.get(bid, {}),
             "recovery": sum(1 for m in rec.get("methods", []) if m.get("status") == "proven"),
             "restore": bool(rec.get("restore")), "routes": []}
        if rt:
            rows = _rows(root, bid)
            probe = rt.get("probe", {})
            e["probe"] = ("XDS110 on the board" if probe.get("onboard") else f"separate {probe.get('family', 'debug')} probe")
            e["transport"] = rt.get("target", {}).get("transport", "")
            e["before"] = rt.get("checks_before_flash", "")
            for name, r in rt["recipes"].items():
                runs = hardware_runs(rows, r["verify"]["kind"])
                level, words = recipe_level(r, runs)
                tools = ", ".join(f"{k} {'/'.join(v.get('validated') or ['any'])}" for k, v in r["tools"].items() if k != "make")
                e["routes"].append({"recipe": name, "job": r.get("project") or JOB.get(name, name), "title": r["title"],
                                    "check": "LED pin timed through the debugger" if r["verify"]["kind"] == "pattern" else "message read on the serial port",
                                    "status": r.get("status", ""), "level": level, "evidence": words,
                                    "source": runs[0]["source"] if runs else "", "tools": tools})
        out.append(e)
    return out, routes.get("hosts", {})


# ---------------------------------------------------------------- renderers

def other_boards_block(entries):
    guided = [e for e in entries if e["routes"]]
    works = [e for e in guided if any(r["level"] == "works" for r in e["routes"])]
    could = [e for e in guided if e not in works]
    manual = [e for e in entries if not e["routes"] and e["readme"]]

    def jobs(e):
        return ", ".join(f"`{r['job']}`" for r in e["routes"] if r["level"] == "works")
    lines = [f"**Other boards:** `./start.sh <board id> <job>` drives {len(guided)} kits. "
             f"With a recorded hardware run: " + "; ".join(f"{e['name']} (`{e['id']}`: {jobs(e)})" for e in works) + "."]
    if could:
        lines[0] += " Listed, with no hardware run of the guided route recorded: " + ", ".join(f"{e['name']} (`{e['id']}`)" for e in could) + "."
    lines.append("")
    n = len(manual)
    lines.append(f"{n} other board{'s' if n != 1 else ''} with a folder {'have' if n != 1 else 'has'} a manual path: `bash mac-setup/bootstrap.sh --check` lists the missing tools and their disk space, "
                 "then follow the ten-minute path in the board folder's `README.md`. Each board's tools, probe, check, backup and recovery, "
                 f"and what is not yet tested: [{OUT}]({OUT}). For a kit not in the folder map, "
                 "`python3 tools/frl/frl.py identify \"<name printed on the board>\"` returns TI's page for it. Board cores, memory and prices are in "
                 "`data/boards.csv`; `site/hardware.html` compares them.")
    return "\n".join(lines)


def coverage_doc(entries, hosts):
    guided = [e for e in entries if e["routes"]]
    manual = [e for e in entries if not e["routes"]]
    L = ["# Which boards have a guided setup, and what each one needs", "",
         "<!-- written by tools/start/route_summary.py from data/routes.json, data/results, tools/benchmark/recipes.json, "
         "data/recovery.json and site/data/code.json; do not edit by hand -->", "",
         f"{len(guided)} kits have a guided route: `./start.sh <board id> <job>` (Windows: `py start.py <board id> <job>`) backs up the board, "
         "builds TI's example with your change, writes it and checks the result on the board. "
         f"The other {len(manual)} boards are set up by hand from their folder's `README.md`.", "",
         "Works means the guided route has a hardware run in the board's results file. Could work means the route is in the registry "
         "but that run is not recorded; the row says what is missing. A build or a test with a fake probe is never counted as a hardware run.", "",
         "Contents: [guided routes](#guided-routes), [what each guided board needs](#what-each-guided-board-needs), "
         "[computers](#computers-the-guided-route-has-run-on), [boards set up by hand](#boards-set-up-by-hand). "
         "Terms: [docs/vocab.md](vocab.md).", "",
         "## Guided routes", "",
         "| Board | Id | Job | Status | Evidence | Check |", "|---|---|---|---|---|---|"]
    for e in guided:
        for r in e["routes"]:
            st = "Works" if r["level"] == "works" else "Could work"
            ev = r["evidence"] + (f" ([row](../data/results/{e['id']}.csv))" if r["source"] else "")   # the row ships; its source may not
            L.append(f"| {e['name']} | `{e['id']}` | `{r['job']}` | {st} | {ev} | {r['check']} |")
    L += ["", "## What each guided board needs", ""]
    for e in guided:
        L += [f"### {e['name']} (`{e['id']}`)", ""]
        L.append(f"- Probe: {e['probe']}; transport `{e['transport']}`.")
        for r in e["routes"]:
            L.append(f"- `{r['job']}` tools, versions the route was validated with: {r['tools']}.")
        if e["before"]:
            L.append(f"- Before the first write: {e['before'].rstrip('.')}.")
        L.append("- Backup: " + ("`tools/benchmark/backup.py` has a recipe; the route backs up before writing." if e["backup"]
                                  else "no `backup.py` recipe; the route's own backup is described in the board's `AGENTS.md`."))
        L.append("- Recovery: " + (f"{e['recovery']} method(s) with a run behind them" if e["recovery"] else "no recovery method with a run behind it yet")
                 + ("; a restore is recorded" if e["restore"] else "") + (f" (`{e['folder']}/AGENTS.md`, When the board stops answering)." if e["folder"] else "."))
        L.append(f"- First command: `./start.sh {e['id']} {e['routes'][0]['job']} --dry-run`, then without `--dry-run`.")
        L.append("")
    L += ["## Computers the guided route has run on", "",
          "Only these cells have a run record (`hosts` in `data/routes.json`); every other combination is untested.", "",
          "| Computer | Result | Date |", "|---|---|---|"]
    for k, h in hosts.items():
        L.append(f"| {h.get('cell') or k} | {h.get('result', 'untested')} | {h.get('date', '')} |")
    L += ["", "## Boards set up by hand", "",
          "No guided route. Start with `bash mac-setup/bootstrap.sh --check` for the tools, then the board's own README.", "",
          "| Board | Id | Start here | Backup | Recovery methods with a run |", "|---|---|---|---|---|"]
    for e in manual:
        start = f"[`{e['readme']}`]({'../' + e['readme']})" if e["readme"] else "no setup guide; results only"
        L.append(f"| {e['name']} | `{e['id']}` | {start} | {'recipe in `backup.py`' if e['backup'] else 'none'} | {e['recovery'] or 'none'} |")
    return "\n".join(L) + "\n"


def splice(text, block):
    a, b = BLOCK
    m = re.search(re.escape(a) + r".*?" + re.escape(b), text, re.S)
    if not m:
        raise SystemExit(f"the {a[:28]}... markers are missing")
    return text[:m.start()] + a + "\n" + block + "\n" + b + text[m.end():]


def outputs(root=ROOT):
    entries, hosts = summary(root)
    files = {OUT: coverage_doc(entries, hosts)}
    block = other_boards_block(entries)
    for rel in README_FILES:
        p = root / rel
        if p.exists():
            files[rel] = splice(p.read_text(encoding="utf-8"), block)
    return files


def stale_files(root=ROOT):
    """{path: text this would write} for every output that differs from what is on disk."""
    files = outputs(root)
    return {rel: t for rel, t in files.items() if not (root / rel).exists() or (root / rel).read_text(encoding="utf-8") != t}


def main(argv):
    files = stale_files()
    stale = list(files)
    if "--check" in argv:
        for rel in stale:
            print(f"{rel}: setup coverage is out of date; run python3 tools/start/route_summary.py")
        return 1 if stale else 0
    for rel in stale:
        (ROOT / rel).write_text(files[rel], encoding="utf-8")
        print(f"wrote {rel}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
