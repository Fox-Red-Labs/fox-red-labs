#!/usr/bin/env python3
"""One command from a clone to a checked result on a LaunchPad kit, with or without an AI assistant.

  ./start.sh <board> <job> [--probe SERIAL] [--assistant none|claude|codex] [--pattern 150,150,150,900]
             [--model MODEL] [--ccs] [--timeout MIN] [--dry-run]

  board        a board id (mspm0l1306, mspm0g3507, ...); `python3 tools/frl/frl.py identify "<printed name>"` finds it
  job          blink | a project slug (print-a-message, run-zephyr, ...) | "any sentence in quotes" (assistant only)
  --probe      the XDS110 serial (python3 tools/frl/frl.py detect lists them); required when more than one is plugged in
  --assistant  none (default where the board has a guided route: no AI account needed), claude or codex
  --pattern    for blink: LED on,off,on,off ... in ms (default 150,150,150,900: two short flashes, then a pause)

Every value is passed as data: nothing here is pasted into code or a shell line. The board, job and probe are
checked before any assistant starts or anything touches the board. The run's result is the check's result:
an assistant saying it finished is never a success on its own. Exit status: 0 verified, 1 failed, 2 refused
before touching the board, 3 not verified (no check exists, or the check could not read the board).
Each run keeps its own journal, assistant output and timing in the per-user data folder (`frl status`).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.parse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import journal  # noqa: E402
import ops  # noqa: E402
import probes  # noqa: E402
import toolfind  # noqa: E402
import verify as checks  # noqa: E402
from core import CODE, PROJECTS, ROOT, Result, Stop, routes  # noqa: E402

REPO_ISSUES = "https://github.com/Fox-Red-Labs/fox-red-labs/issues/new"
ASK = ("If this board cannot do that, say so before you start. Otherwise build it, flash it, run it, and tell me "
       "exactly how to check the result on the board.")


def parse(argv):
    ap = argparse.ArgumentParser(prog="start.sh", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("board")
    ap.add_argument("job")
    ap.add_argument("--probe")
    ap.add_argument("--assistant", choices=["none", "claude", "codex"])
    ap.add_argument("--pattern")
    ap.add_argument("--message", help="for print-a-message: the text the board prints (1 to 40 characters)")
    ap.add_argument("--model")
    ap.add_argument("--ccs", action="store_true")
    ap.add_argument("--timeout", type=float, default=30.0, help="minutes before the assistant is stopped")
    ap.add_argument("--experimental", action="store_true", help="run a job never tried on this board (no success is claimed)")
    ap.add_argument("--dry-run", action="store_true")
    return ap.parse_args(argv)


def board_aliases():
    """Short and page ids mapped to the real board id (data/board-ids.json); empty in a copy without it."""
    try:
        return json.loads((ROOT / "data" / "board-ids.json").read_text(encoding="utf-8"))["aliases"]
    except (OSError, ValueError, KeyError):
        return {}


def route_id(board, code):
    """The id the route registry and the project cells use. site/data/code.json is keyed by page ids
    (cc3551e) while data/routes.json uses the real id (cc35x1): accept either and route by the real one."""
    real = board_aliases().get(board)
    if real and real in routes()["boards"]:
        return real
    return board


def board_name(board):
    pj = json.loads(PROJECTS.read_text(encoding="utf-8"))
    names = {b["page"][6:-5]: b["name"] for b in pj["boards"]}
    return names.get(board, board)


def plan(a):
    """Everything decided before an assistant or a board is touched. Returns a dict, or raises Stop."""
    code = json.loads(CODE.read_text(encoding="utf-8"))["boards"]
    a.board = route_id(a.board, code)
    b = code.get(a.board) or next((code[k] for k, v in board_aliases().items() if v == a.board and k in code), None)
    if not b:
        raise Stop(Result.make("refused", "FRL-E010", f"no board id {a.board!r}",
                               f"ids: {' '.join(sorted(code))}; or run python3 tools/frl/frl.py identify \"<name printed on the board>\""))
    name = board_name(a.board)
    route = routes()["boards"].get(a.board)
    p = {"board": a.board, "name": name, "folder": b["folder"], "job": a.job, "recipe": None, "pattern": None,
         "experimental": False}
    pj = json.loads(PROJECTS.read_text(encoding="utf-8"))
    proj = next((x for x in pj["projects"] if x["slug"] == a.job), None)
    if a.job == "blink" or (proj and proj["slug"] == "blink"):
        e = ops.eligible(a.board, "blink")
        if not e["ok"]:
            raise Stop(e)
        if route and "blink-pattern" in route["recipes"]:
            r = route["recipes"]["blink-pattern"]
            ms, why = checks.parse_pattern(a.pattern or r["default_pattern"])
            if not ms:
                raise Stop(Result.make("refused", "FRL-E070", why))
            p.update(recipe="blink-pattern", pattern=",".join(map(str, ms)))
            p["say"] = (f"Make LED2 on my {name} flash this pattern, in milliseconds: "
                        + ", ".join(f"{'on' if i % 2 == 0 else 'off'} {m}" for i, m in enumerate(ms))
                        + ", repeating. " + ASK)
        else:
            p["say"] = f"Blink an LED, then change the pattern in plain English on my {name}. " + ASK
    elif proj and route and any(rr.get("project") == proj["slug"] for rr in route["recipes"].values()):
        name_r, r = next((k, rr) for k, rr in route["recipes"].items() if rr.get("project") == proj["slug"])
        e = ops.eligible(a.board, proj["slug"])
        if not e["ok"]:
            raise Stop(e)
        ask, why = ops.ask_of(r, a.message or a.pattern)
        if not ask:
            raise Stop(Result.make("refused", "FRL-E070", why))
        p.update(recipe=name_r, pattern=ops.ask_text(r, ask))
        p["say"] = (f"Make my {name} print this message over its USB serial port: {ask}. " if r.get("edit") == "message"
                    else f"{r['outcome']} on my {name}. ") + ASK
    elif proj:
        e = ops.eligible(a.board, proj["slug"])
        if not e["ok"]:
            if e["code"] == "FRL-E012" and a.experimental:
                p["experimental"] = True
            else:
                raise Stop(e)
        out = (proj.get("recipe") or {}).get("outcome") or proj.get("outcome") or proj["slug"]
        p["say"] = out.rstrip(".") + f" on my {name}. " + ASK
    else:
        if (a.assistant or "none") == "none":
            raise Stop(Result.make("refused", "FRL-E070", "a job in your own words needs an assistant",
                                   "add --assistant claude or --assistant codex, or pick a project slug"))
        p["experimental"] = True
        p["say"] = a.job
    p["assistant"] = a.assistant or ("none" if p["recipe"] else None)
    if not p["assistant"]:
        raise Stop(Result.make("refused", "FRL-E070", f"{a.job} on {name} has no guided route without an assistant yet",
                               f"add --assistant claude or --assistant codex, or follow {b['folder']}/README.md by hand"))
    return p


def prompt_for(p, probe):
    lines = ["Read AGENTS.md and FAQ.md in this folder first.",
             f"Use only the debug probe with serial {probe}; never another one, even if more are plugged in.",
             "Back up the board's flash before the first write (python3 tools/frl/frl.py backup), and never write the boot configuration."]
    if p["recipe"]:
        f = f"python3 tools/frl/frl.py"
        b, pr, pat = p["board"], probe, p["pattern"]
        lines.append("Use the local tool for every step that touches tools or the board. Run these from the repository root "
                     "(one level up), in order, and stop at the first that fails:\n"
                     f"  {f} doctor --board {b}\n  {f} backup --board {b} --probe {pr}\n"
                     f"  {f} prepare --board {b} --recipe {p['recipe']} --pattern \"{pat}\"\n  {f} build --board {b} --recipe {p['recipe']}\n"
                     f"  {f} flash --board {b} --probe {pr} --image <the image path that build printed>\n"
                     f"  {f} verify --board {b} --recipe {p['recipe']} --probe {pr} --pattern \"{pat}\"\n"
                     "Their exit status is the truth: report a failed or not-verified check as it is. "
                     "You may read and explain the source that prepare edited.")
    if p["experimental"]:
        lines.append("Nothing in this repository says this job works on this board: say what you tried and what is unproven.")
    lines.append(p["say"])
    return "\n".join(lines)


def assistant_cmd(p, prompt, model, ccs, rn):
    if p["assistant"] == "claude":
        c = toolfind.find("claude")
        mcp = ["--mcp-config", json.dumps({"mcpServers": {}}), "--strict-mcp-config"]
        if ccs:
            mcp = ["--mcp-config", str(ROOT / "mac-setup" / "ccs-mcp.json"), "--strict-mcp-config",
                   "--allowedTools", "mcp__ccs-project,mcp__ccs-debug,mcp__ccs-sysconfig,mcp__ccs-serial"]
        return [c["exe"], "-p", prompt, *mcp, "--permission-mode", "auto", "--model", model or "claude-sonnet-5-5",
                "--output-format", "json"], c["version"]
    c = toolfind.find("codex")
    from core import data_dir
    cmd = [c["exe"], "exec", "-C", str(ROOT / p["folder"]), "-s", "workspace-write", "--add-dir", str(ROOT),
           "--add-dir", str(data_dir()), "--add-dir", str(ops.default_project(p["board"], p["recipe"] or "job").parent),
           "-o", str(rn.dir / "assistant-last-message.txt"), prompt]
    if model:
        cmd[2:2] = ["-m", model]
    return cmd, c["version"]


def report_url(p, res, rn):
    st = rn.state()
    machine = st.get("wall_s") or sum(st["timing"]["machine_s"].values())
    q = {"template": "built-it.yml", "title": f"{p['board']} {p['job']}: {res['outcome']}", "board": p["board"],
         "project": p["job"], "waiting_min": str(max(1, round(machine / 60))), "hands_on_min": "",
         "tools": f"start.sh, assistant {p['assistant']}, check {res['code']}"}
    return REPO_ISSUES + "?" + urllib.parse.urlencode(q)


def run_assistant(p, a, probe, rn):
    """Run the assistant, then the recipe's own check. The check decides the outcome."""
    prompt = prompt_for(p, probe)
    (rn.dir / "prompt.txt").write_text(prompt, encoding="utf-8")
    ok = ops.doctor(p["board"], p["recipe"], p["assistant"], run=rn) if p["recipe"] else None
    if ok is not None and not ok["ok"]:
        return ok
    try:
        cmd, ver = assistant_cmd(p, prompt, a.model, a.ccs, rn)
    except Stop as s:
        return Result.make("refused", "FRL-E023", s.result["message"], s.result["next"])
    model = a.model or ("claude-sonnet-5-5" if p["assistant"] == "claude" else "the codex default from ~/.codex/config.toml")
    rn.set(assistant={"name": p["assistant"], "version": ver, "model": model})
    env = dict(os.environ, PROBE=probe, FRL_RUN=rn.id)
    env.pop("ELECTRON_RUN_AS_NODE", None)
    t0 = time.time()
    out, err = open(rn.dir / "assistant.out", "w", encoding="utf-8"), open(rn.dir / "assistant.err", "w", encoding="utf-8")
    try:
        proc = subprocess.run(cmd, cwd=ROOT / p["folder"], stdout=out, stderr=err, env=env, timeout=a.timeout * 60)
        code = proc.returncode
    except subprocess.TimeoutExpired:
        code = None
    finally:
        out.close()
        err.close()
    rn.time("machine_s", "assistant", time.time() - t0)
    rn.log("assistant-exit", {"code": code})
    said = (rn.dir / "assistant.out").read_text(encoding="utf-8")
    try:
        said = json.loads(said).get("result", said)
    except ValueError:
        pass
    if code is None:
        res = Result.make("failed", "FRL-E061", f"the assistant ran past {a.timeout:g} min and was stopped",
                          "the board may be part way through a step: check it with `frl status` and `frl verify`")
    elif code != 0:
        res = Result.make("failed", "FRL-E060", f"the assistant stopped with exit {code}",
                          f"read {rn.dir / 'assistant.err'}; the board was not checked")
    elif p["recipe"]:
        image = rn.state().get("image")   # the build the assistant ran through frl (FRL_RUN); None if it built elsewhere
        res = ops.check(p["board"], p["recipe"], probe=probe, pattern=p["pattern"], image=image, run=rn)
        if res["ok"] and not image:
            res = Result.make("unverified", "FRL-E041", "the LED follows the pattern, but the assistant did not build through frl, "
                              "so there is no image to compare with the flash", "rerun with the frl build step", **res["data"])
    else:
        res = Result.make("unverified", "FRL-E041", f"no check exists for {p['job']} on {p['name']}",
                          "look at the board yourself and compare with what the assistant said")
        rn.set("unverified")
    res["data"]["assistant_said"] = str(said)[-2000:]
    return res


def main(argv=None):
    argv = argv if argv is not None else sys.argv[1:]
    if not argv and sys.stdin.isatty():
        import guide          # ./start.sh alone: the guided questions
        return guide.main()
    a = parse(argv)
    try:
        p = plan(a)
    except Stop as s:
        print(s.result.text())
        return s.result.exit_status()
    if p["recipe"] and not a.dry_run:
        # the tools first: the full list of what to install is useful before the board is even out of the box
        d = ops.doctor(p["board"], p["recipe"], p["assistant"] if p["assistant"] != "none" else "terminal")
        if not d["ok"]:
            print(d.text())
            total = routes()["boards"][p["board"]].get("install_total")
            if total and d["code"] == "FRL-E020":
                print("\n" + total)
            return d.exit_status()
    try:
        probe = probes.select(a.probe)
    except Stop as s:
        if not a.dry_run:
            print(s.result.text())
            return s.result.exit_status()
        # a dry run checks the arguments and says what a real run would stop at, with no board needed
        print("probe: a real run would stop here: " + s.result.text().replace("\n", "\n  "))
        probe = a.probe or "<serial>"
    print(f"board {p['name']} ({p['board']}), folder {p['folder']}/, probe {probe}, assistant {p['assistant']}"
          + (f", asked: {p['pattern']}" if p["pattern"] else "") + (", EXPERIMENTAL" if p["experimental"] else ""))
    t_start = time.time()
    if a.dry_run:
        if p["recipe"]:
            d = ops.doctor(p["board"], p["recipe"], p["assistant"] if p["assistant"] != "none" else "terminal")
            print("tools: " + ("all found" if d["ok"] else "a real run would stop here: " + d.text().replace("\n", "\n  ")))
        if p["assistant"] != "none":
            print("prompt:\n" + prompt_for(p, probe))
        print("dry run: nothing started, nothing touched the board")
        return 0
    if p["assistant"] == "none":
        res = ops.run_route(p["board"], p["recipe"], p["pattern"], probe)
        rn = journal.Run.load(res["data"]["run_id"]) if res["data"].get("run_id") else None
    else:
        rn = journal.Run.new(board=p["board"], recipe=p["recipe"], job=p["job"], pattern=p["pattern"],
                             route=p["assistant"], probe=probe)
        res = run_assistant(p, a, probe, rn)
    print(res.text())
    if res["data"].get("assistant_said"):
        print("\nThe assistant said (its words, not the check):\n" + res["data"]["assistant_said"][-1200:])
    if rn:
        print("\n" + rn.summary())
        st = rn.state()
        rn.set(wall_s=round(time.time() - t_start, 1))
        print(f"time: {time.time() - t_start:.0f} s from start to result (the machine working); your hands-on time: unknown (not measured by this tool)")
        print("Report it (prefilled; nothing is sent until you submit): " + report_url(p, res, rn))
    return res.exit_status()


if __name__ == "__main__":
    sys.exit(main())
