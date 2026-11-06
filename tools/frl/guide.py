#!/usr/bin/env python3
"""A guided first run in the terminal: questions with numbered answers, no flags and no config files.

  python3 tools/frl/guide.py          (also: ./start.sh with nothing after it)

It asks what you have, finds the board, checks the tools, shows exactly what will be written before the
first write, then runs the same steps as `frl run`. Type a number and press return; press return alone for
the suggested answer; Ctrl-C stops before the next step and says what state the board is in.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ops  # noqa: E402
import probes  # noqa: E402
from core import Stop, routes  # noqa: E402

PATTERNS = [("Two short flashes, then a pause", "150,150,150,900"),
            ("One long, one short", "600,200,150,600"),
            ("Three quick flashes, then a pause", "100,100,100,100,100,700"),
            ("Slow and steady", "500,500")]
MESSAGES = ["Hello from my LaunchPad", "It works"]


def ask(question, options, default=1, inp=input, out=print):
    out("")
    out(question)
    for i, o in enumerate(options, 1):
        out(f"  {i}. {o}" + ("   (press return for this)" if i == default else ""))
    while True:
        a = inp("> ").strip()
        if not a:
            return default - 1
        if a.isdigit() and 1 <= int(a) <= len(options):
            return int(a) - 1
        out(f"Type a number from 1 to {len(options)}.")


def show(res, out=print):
    out(("  ok: " if res["ok"] else f"  {res['code']}: ") + res["message"])
    if not res["ok"] and res.get("next"):
        out("  what to do: " + res["next"])


def main(inp=input, out=print):
    out("Fox Red Labs: from a LaunchPad kit to a program you chose, checked on the board.")
    if ask("What do you have?", ["A board in hand", "No board yet: help me choose"], 1, inp, out) == 1:
        out("Start at site/hardware.html: it matches projects to boards. Come back with the board.")
        return 0
    rt = routes()["boards"]
    names = [b["kit"] for b in rt.values()] + ["Something else"]
    k = ask("Which kit is it? The name is printed on the board.", names, 1, inp, out)
    if k == len(names) - 1:
        out("Type the name exactly as printed (for example LP-EM-CC2340R53):")
        res = ops.identify(inp("> ").strip())
        show(res, out)
        return res.exit_status()
    board = list(rt)[k]
    b = rt[board]
    out(f"Where to look: {b['printed']}")
    out("")
    out("Plug the board in with a USB data cable now. Press return when its power light is on.")
    inp("> ")
    ps = probes.list_probes()
    if not ps:
        show(ops.detect(), out)
        out("  No probe yet. A charge-only cable powers the board but hides it; try another cable, straight into the computer.")
        return 2
    serial = ps[0]["serial"]
    if len(ps) > 1:
        i = ask("More than one board is plugged in. Which probe is this board? (Unplug the others to be sure.)",
                [p["serial"] for p in ps], 1, inp, out)
        serial = ps[i]["serial"]
    res = ops.identify(board=board, probe=serial)
    show(res, out)
    if not res["ok"]:
        return res.exit_status()
    recipes = b["recipes"]   # only what this board's route has: a print-only board never gets the blink steps
    rname = next(iter(recipes))
    if len(recipes) > 1:
        rname = list(recipes)[ask("What should the board do?", [r["title"] for r in recipes.values()], 1, inp, out)]
    r = recipes[rname]
    res = ops.doctor(board, rname)
    show(res, out)
    if not res["ok"]:
        out("  Install what is listed, then run this again. Nothing was written to the board.")
        return res.exit_status()
    message = r.get("edit") == "message"
    if message:
        texts = [r["default_message"]] + [m for m in MESSAGES if m != r["default_message"]]
        pattern = texts[ask("Which message should the board print?", texts, 1, inp, out)]
    else:
        pattern = PATTERNS[ask("Which pattern should LED2 flash?", [n for n, _ in PATTERNS], 1, inp, out)][1]
    proj = ops.default_project(board, rname)
    never = b.get("target", {}).get("never_write")
    out("")
    out("Before the first write, this is what will happen:")
    out(f"  board: {b['kit']} on probe {serial} (chip ID checked again just before writing)")
    out("  1. back up what the board holds now (nothing written)")
    out(f"  2. copy TI's example to {proj} and " + (f'put in the message "{pattern}"' if message else f"set the pattern {pattern} ms"))
    out("  3. build it with the tools found above")
    out("  4. WRITE the new program to the board" + (f" ({never})" if never else ""))
    out("  5. " + ("read the message on the board's serial port to check it" if r["verify"]["kind"] == "uart"
                   else "time the LED pin through the debugger to check the pattern"))
    if ask("Go ahead?", ["Yes, do it", "No, stop here"], 1, inp, out) == 1:
        out("Stopped. Nothing was written.")
        return 2
    try:
        res = ops.run_route(board, rname, pattern, serial)
    except KeyboardInterrupt:
        out("\nStopped. Run `python3 tools/frl/frl.py status --handoff` to see where it stopped and what the board holds.")
        return 1
    show(res, out)
    d = res.get("data", {})
    if d.get("source"):
        out(f"\nYour program: {d['source']}" + ("." if message else " (the line `static const uint32_t pattern_ms[]` sets the pattern)."))
    if d.get("backup"):
        out(f"To put the board back as it came: python3 tools/frl/frl.py restore \"{d['backup']}\" --probe {serial}")
    for n in d.get("next_projects", []):
        out(f"Next idea: {n['project']} ({n['why']}).")
    return res.exit_status()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyboardInterrupt, EOFError):
        print("\nStopped before the next step.")
        sys.exit(1)
    except Stop as s:
        print(s.result.text())
        sys.exit(s.result.exit_status())
