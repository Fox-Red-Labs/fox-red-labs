#!/usr/bin/env python3
"""frl: the Fox Red Labs local setup tool. Works with no assistant; assistants call the same operations.

  frl detect                                          probes on USB (a probe is not a board)
  frl identify <printed kit name>                     match the name on the board; honest handoff if unknown
  frl identify --board B --probe S                    read the chip behind a probe and check it is board B
  frl eligible --board B --project P                  has P run on B? (before any install or model call)
  frl doctor --board B [--recipe R] [--route terminal|claude|codex]
  frl prepare --board B [--recipe R] [--pattern 150,150,150,900] [--project DIR]
  frl build --board B [--recipe R] [--project DIR]
  frl backup --board B [--probe S]
  frl flash --board B --image FILE [--probe S]
  frl verify --board B [--recipe R] [--pattern ...] [--image FILE] [--probe S]
  frl verify-image --board B --image FILE [--probe S]
  frl flash-prebuilt <manifest.json> --probe S       a released image: manifest checked, flashed, then checked on the board
  frl restore <backup folder> [--probe S]
  frl run --board B [--recipe R] [--pattern ...] [--probe S] [--project DIR] [--no-backup] [--resume RUN]
  frl status [RUN] [--handoff]                        a run's state, or a short summary to hand a fresh session
  frl mcp                                             serve these operations to a desktop assistant (MCP, stdio)

Add --json for the structured result. Exit status: 0 done or verified, 1 failed, 2 refused before touching
the board, 3 not verified. Errors carry a code (FRL-E0xx) explained in docs/frl-errors.md.
"""
import argparse
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import journal  # noqa: E402
import ops  # noqa: E402
from core import Result, VERSION  # noqa: E402

DEFAULT_RECIPE = "blink-pattern"


def main(argv=None):
    ap = argparse.ArgumentParser(prog="frl", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json", action="store_true", help="print the structured result")
    ap.add_argument("--version", action="version", version=f"frl {VERSION}")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def p(name, *opts):
        q = sub.add_parser(name)
        for o in opts:
            if o == "board":
                q.add_argument("--board", required=True)
            elif o == "recipe":
                q.add_argument("--recipe", default=DEFAULT_RECIPE)
            elif o == "probe":
                q.add_argument("--probe")
            elif o == "pattern":
                q.add_argument("--pattern", help="blink-pattern: on,off,on,off ... in ms, e.g. 150,150,150,900 (two short flashes, then a pause)")
                q.add_argument("--message", help="print-message (--recipe print-message): the text the board prints, 1 to 40 characters")
            elif o == "project":
                q.add_argument("--project", help="your project folder (default ~/FoxRedLabs/<board>-<recipe>)")
            elif o == "image":
                q.add_argument("--image", required=True)
        return q

    p("detect")
    q = p("identify")
    q.add_argument("name", nargs="?")
    q.add_argument("--board")
    q.add_argument("--probe")
    q.add_argument("--connect-step", help="with --board and --probe: connect through this one ladder step only (a test of that step)")
    q = p("eligible", "board")
    q.add_argument("--project", required=True)
    q = p("doctor", "board", "recipe")
    q.add_argument("--route", default="terminal", choices=["terminal", "claude", "codex"])
    p("prepare", "board", "recipe", "pattern", "project")
    p("build", "board", "recipe", "project")
    p("backup", "board", "probe")
    p("flash", "board", "image", "probe")
    q = p("verify", "board", "recipe", "pattern", "probe")
    q.add_argument("--image")
    q.add_argument("--seconds", type=float)
    p("verify-image", "board", "image", "probe")
    q = p("flash-prebuilt", "probe")
    q.add_argument("manifest")
    q = p("restore", "probe")
    q.add_argument("folder")
    q = p("run", "board", "recipe", "pattern", "probe", "project")
    q.add_argument("--no-backup", action="store_true", help="skip the backup (recorded as backup unavailable)")
    q.add_argument("--resume", metavar="RUN")
    q = p("status")
    q.add_argument("run", nargs="?")
    q.add_argument("--handoff", action="store_true")
    q.add_argument("--report", action="store_true", help="the handoff with your home folder, computer name and probe serials removed, to paste into a public report")
    p("mcp")
    try:   # optional commands; not in every copy
        import design_tools
        design_tools.add_parser(p)
    except ImportError:
        design_tools = None
    argv = list(sys.argv[1:] if argv is None else argv)
    as_json = "--json" in argv                 # accepted anywhere: frl --json detect or frl detect --json
    argv = [x for x in argv if x != "--json"]
    a = ap.parse_args(argv)
    a.json = as_json

    if a.cmd == "mcp":
        import mcp_server
        return mcp_server.serve()
    if design_tools and a.cmd == design_tools.COMMAND:
        return design_tools.run(a)
    if a.cmd == "status":
        r = journal.Run.load(a.run) if a.run else journal.latest()
        if not r:
            res = Result.make("refused", "FRL-E070", "no runs yet", "start one with `frl run --board <board>`")
        else:
            res = Result.good(r.summary(), run=r.state())
        if (a.handoff or a.report) and r and not a.json:
            st = r.state()
            text = r.summary()
            if a.report:
                text = journal.redact(text + "\nLast error: " + json.dumps((st.get("steps") or [{}])[-1]), [st.get("probe") or ""])
                text += "\n(Redacted copy: check it before you paste it anywhere. Nothing is sent by this tool.)"
            print(text)
            return 0
    else:
        pat = getattr(a, "pattern", None) or getattr(a, "message", None)
        # An assistant started by start.sh gets FRL_RUN: its steps land in that run's journal.
        rn, own_run = None, False
        if os.environ.get("FRL_RUN") and a.cmd not in ("detect", "identify", "eligible", "run"):
            try:
                rn = journal.Run.load(os.environ["FRL_RUN"])
            except FileNotFoundError:
                rn = None
        if rn is None and a.cmd in ("flash", "restore", "flash-prebuilt"):   # every write to a board gets a journal
            bd = getattr(a, "board", None)
            if a.cmd == "restore":
                try:
                    bd = json.loads((Path(a.folder) / "backup.json").read_text(encoding="utf-8")).get("board")
                except (OSError, ValueError):
                    bd = None
            rn = journal.Run.new(board=bd, route="cli", op=a.cmd)
            own_run = True
        res = {
            "detect": lambda: ops.detect(),
            "identify": lambda: ops.identify(a.name, a.board, a.probe, connect_step=a.connect_step),
            "eligible": lambda: ops.eligible(a.board, a.project),
            "doctor": lambda: ops.doctor(a.board, a.recipe, a.route, run=rn),
            "prepare": lambda: ops.prepare(a.board, a.recipe, pat, a.project, run=rn),
            "build": lambda: ops.build(a.board, a.recipe, a.project, run=rn),
            "backup": lambda: ops.backup(a.board, a.probe, run=rn),
            "flash": lambda: ops.flash(a.board, a.image, a.probe, run=rn),
            "verify": lambda: ops.check(a.board, a.recipe, a.probe, pat, a.image, a.seconds, run=rn),
            "verify-image": lambda: ops.verify_image(a.board, a.image, a.probe, run=rn),
            "flash-prebuilt": lambda: ops.flash_prebuilt(a.manifest, a.probe, run=rn),
            "restore": lambda: ops.restore(a.folder, a.probe, run=rn),
            "run": lambda: ops.run_route(a.board, a.recipe, pat, a.probe, a.project, not a.no_backup, a.resume),
        }[a.cmd]()
    if a.cmd not in ("status", "mcp") and own_run and rn and res["outcome"] != "unverified" and rn.state()["state"] not in ("programming",):
        rn.set("done" if res["ok"] else ("refused" if res["outcome"] == "refused" else "failed"))
    if a.json:
        print(json.dumps(res, indent=1, default=str))
    else:
        print(res.text() if isinstance(res, Result) else Result(res).text())
        d = res.get("data", {})
        if a.cmd == "detect":
            for pr in d.get("probes", []):
                print(f"  {pr['serial']}  {pr['name']}")
            print("  " + d.get("note", ""))
        if a.cmd == "run" and d.get("summary"):
            print("\n" + d["summary"])
            for n in d.get("next_projects", []):
                print(f"  next: {n['project']} ({n['why']})")
    return Result(res).exit_status()


if __name__ == "__main__":
    sys.exit(main())
