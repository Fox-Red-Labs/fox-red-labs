"""The operations every route calls: detect, identify, eligible, doctor, prepare, build, backup, flash,
verify, verify_image, restore and run. Each returns a Result (core.py) and, given a Run, journals itself.

A person at a terminal, start.sh, an assistant and the MCP adapter all call these same functions. There
is one flash implementation.
"""
import contextlib
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

import journal
import probes
import target
import toolfind
import verify as checks
from core import CODE, PROJECTS, ROOT, Result, Stop, board_route, data_dir, host, recipe, routes, stop
from lock import ProbeLock

sys.path.insert(0, str(ROOT / "tools" / "benchmark"))
MSG_MARK = "/* Fox Red Labs message (frl prepare --message): printed with a counter once a second */"
MARK = "/* Fox Red Labs pattern (frl prepare --pattern): LED on, off, on, off ... in ms */"


def display_message(text, ms):
    """TI's drivers/display example (CC23xx): print "> <message> <count>" once a second in place of its LED line, and
    start after 1 s instead of 6. Returns (new text, edited line), or (None, line) when the file was changed by hand."""
    line = f'static const char frlMessage[] = "{ms}";'
    if MSG_MARK in text:
        return re.sub(r'static const char frlMessage\[\] = "[^"\n]*";', lambda m: line, text, count=1), line
    led = re.search(r'Display_printf\(hSerial,\s*ledPinValue \? 0 : 1,[^;]*serialLedOff\);', text)
    if "    unsigned int loopCount = 0;\n" not in text or "    sleep(6);\n" not in text or not led:
        return None, line
    text = text.replace(led.group(0), 'Display_printf(hSerial, DisplayUart2_SCROLLING, 0, "> %s %u", frlMessage, loopCount++);')
    text = text.replace("    unsigned int loopCount = 0;\n", "    unsigned int loopCount = 0;\n    " + MSG_MARK + "\n    " + line + "\n", 1)
    return text.replace("    sleep(6);\n", "    sleep(1);\n", 1), line


def uart2echo_message(text, ms):
    """TI's drivers/uart2echo example (CC35xx): print "> <message> <count>" once a second instead of echoing. Returns
    (new text, edited line), or (None, line) when the file was changed by hand."""
    line = f'static const char frlMessage[] = "{ms}";'
    if MSG_MARK in text:
        return re.sub(r'static const char frlMessage\[\] = "[^"\n]*";', lambda m: line, text, count=1), line
    loop = text.find("    /* Loop forever echoing */")
    end = text.rstrip().rfind("}")
    if loop < 0 or '#include "ti_drivers_config.h"' not in text or end < loop:
        return None, line
    body = ("    " + MSG_MARK + "\n    " + line + "\n"
            "    char frlLine[64];\n    unsigned int loopCount = 0;\n    (void)input; (void)bytesRead; (void)status;\n"
            "    while (1)\n    {\n"
            "        int n = snprintf(frlLine, sizeof(frlLine), \"> %s %u\\r\\n\", frlMessage, loopCount++);\n"
            "        UART2_write(uart, frlLine, (size_t)n, &bytesWritten);\n"
            "        sleep(1);\n    }\n}\n")
    head = text[:loop].replace('#include "ti_drivers_config.h"', '#include <stdio.h>\n#include <unistd.h>\n\n#include "ti_drivers_config.h"', 1)
    return head + body, line


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def guarded(fn):
    """Turn a Stop or an unexpected error into a Result; journal it when a run is given."""
    def wrap(*a, run=None, **k):
        t0 = time.time()
        try:
            res = fn(*a, run=run, **k)
        except Stop as s:
            res = s.result
        except SystemExit as e:  # tools/benchmark/backup.py refuses with SystemExit("REFUSED: ...")
            msg = str(e.code if e.code is not None else "")
            code = ("FRL-E052" if ("changed since" in msg or "does not match" in msg) else
                    "FRL-E051" if ("another board" in msg or "index" in msg or "identity" in msg or "probe" in msg) else
                    "FRL-E050" if fn.__name__ == "backup" else "FRL-E099")
            res = Result.make("refused" if msg.startswith("REFUSED") else "failed", code, msg.replace("REFUSED: ", ""),
                              "the board may be partly restored: run the same restore again" if "read-back" in msg
                              else "nothing was written to the board by this step")
        except Exception as e:  # never report an unexpected error as success
            res = Result.make("failed", "FRL-E099", f"{type(e).__name__}: {e}", "save the run summary (`frl status`) and report it")
        if run:
            run.time("machine_s", fn.__name__, time.time() - t0)
            run.set(step={"name": fn.__name__, "outcome": res["outcome"], "code": res["code"],
                          "message": res["message"], "next": res.get("next", "")})
        return res
    wrap.__name__ = fn.__name__
    wrap.__doc__ = fn.__doc__
    return wrap


# ---------------------------------------------------------------- finding the board and the project

@guarded
def detect(run=None):
    """List debug probes on USB. A probe is not a board: say which board it is with `frl identify`."""
    ps = probes.list_probes()
    return Result.good(f"{len(ps)} XDS110 probe(s): {', '.join(p['serial'] for p in ps) or 'none'}",
                       probes=ps, host=host(),
                       note="A detected XDS110 is a probe family, not proof of which board is attached.")


def _project_names():
    pj = json.loads(PROJECTS.read_text(encoding="utf-8"))
    names = {b["page"][6:-5]: b["name"] for b in pj["boards"]}
    short = {b["page"][6:-5]: b["id"] for b in pj["boards"]}
    for rid, rb in json.loads((ROOT / "data" / "routes.json").read_text(encoding="utf-8"))["boards"].items():   # routed boards without a grid
        short.setdefault(rid, rid.removeprefix("mspm0"))      # column: cells use the short id (render_projects rb_id)
        names.setdefault(rid, rb["kit"])
    return pj, names, short


def _norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


@guarded
def identify(name=None, board=None, probe=None, run=None, connect_step=None):
    """Match a printed kit name to a board, or read the chip behind a probe. Unknown names get an honest handoff."""
    rt = routes()["boards"]
    if probe and board:
        b = board_route(board)
        ident = target.check_identity(b, probes.select(probe), start=connect_step)
        return Result.good(f"probe {probe} is attached to an {b['kit']} ({('DEVICEID ' + ident['deviceid']) if ident.get('deviceid') else ident.get('part', '')})", board=board, identity=ident,
                           connect=target.connect_note(probes.select(probe)))
    if probe and not name:                            # no board named: discover.py tries the wire protocols and ID registers
        import discover
        from lock import ProbeLock
        serial = probes.select(probe)
        with ProbeLock(serial, "", "identify"):           # another session or the bench holding this probe gets FRL-E004, not a clash
            got = discover.identify(serial)
        parts = got["part"]
        hits = [h for st in got["steps"] for h in st.get("hits", [])]

        def same_chip(b):                             # a route's part name, by prefix (CC2340R5 is CC2340R52E0RKPR), or its ID register reading
            if any(_norm(p).startswith(_norm(a)) for a in b["aliases"] if len(_norm(a)) >= 6 for p in parts):
                return True
            i = b["target"].get("identity", {})
            if "addr" not in i:
                return False
            ids = [i.get("deviceid")] + list(i.get("deviceid_any") or [])
            mask = int(i.get("deviceid_mask", "0x0FFFFFFF"), 16)
            return any(int(h["addr"], 16) == int(i["addr"], 16) + 4 and any(x and int(h["deviceid"], 16) & mask == int(x, 16) & mask for x in ids)
                       for h in hits)
        routed = [bid for bid, b in rt.items() if same_chip(b)]
        if not got["family"]:
            return Result.make("refused", "FRL-E010", f"probe {probe}: {got['how']}",
                               "check the board's power and the probe cable; then name the board with --board", steps=got["steps"])
        what = (parts[0] if got["exact"] and len(parts) == 1 else
                f"{' or '.join(got['family'])}" + (f" ({', '.join(parts[:6])}{' ...' if len(parts) > 6 else ''})" if parts else ""))
        msg = f"probe {probe} is attached to {what}, found by {got['how']}"
        if routed:
            msg += f"; frl has a route for it: --board {routed[0]}"
        return Result.good(msg, board=routed[0] if routed else None, family=got["family"], parts=parts, exact=got["exact"],
                           how=got["how"], steps=got["steps"])
    if not name:
        stop("refused", "FRL-E070", "say the name printed on the board", "for example: frl identify LP-MSPM0L1306")
    n = _norm(name)
    for bid, b in rt.items():
        if any(_norm(a) == n for a in b["aliases"]) or _norm(bid) == n:
            return Result.good(f"{b['kit']}: supported route(s) {', '.join(b['recipes'])}", board=bid, kit=b["kit"],
                               printed=b["printed"], official=b["official"], routes=list(b["recipes"]))
    pj, names, _ = _project_names()
    code = json.loads(CODE.read_text(encoding="utf-8"))["boards"]
    known = [bid for bid, nm in names.items() if _norm(nm) == n or _norm(bid) == n or n in _norm(nm) and len(n) >= 6]
    if known:
        bid = known[0]
        return Result.make("refused", "FRL-E010",
                           f"{names[bid]} is in the lab notebook, but has no guided route yet",
                           f"read {code.get(bid, {}).get('folder', bid)}/AGENTS.md and its README's ten-minute path; steps there are manual and were run on our bench only",
                           board=bid, kit=names[bid], folder=code.get(bid, {}).get("folder"), experimental=True)
    kit = name.strip().upper()
    return Result.make("refused", "FRL-E010", f"no notes for {name!r} here",
                       "check the exact name printed on the board (front silkscreen, or the label on the back); "
                       f"TI's page for a kit is usually https://www.ti.com/tool/{kit} and lists its user's guide and out-of-box demo. "
                       "Nothing here will flash an unlisted board automatically.",
                       official=f"https://www.ti.com/tool/{kit}", experimental=True)


@guarded
def eligible(board, project, run=None):
    """Can this project run on this board? From site/data/projects.json, before any install or model call."""
    pj, names, short = _project_names()
    p = next((x for x in pj["projects"] if x["slug"] == project), None)
    if not p:
        stop("refused", "FRL-E070", f"no project {project!r}", "the project ids are the slugs in site/data/projects.json (blink, print-a-message, speed-test, ...)")
    cell = p.get("cells", {}).get(short.get(board, board))
    alts = [x["slug"] for x in pj["projects"] if (x.get("cells", {}).get(short.get(board, "")) or {}).get("st") == "yes"]
    if not cell or cell.get("st") in (None, "untested"):
        stop("refused", "FRL-E012", f"{project} has not been tried on {names.get(board, board)}",
             f"projects that ran on this board: {', '.join(alts[:6]) or 'none yet'}; or try it with ./start.sh --assistant claude --experimental, knowing nothing here says it works",
             alternatives=alts)
    if cell["st"] == "no":
        stop("refused", "FRL-E011", f"{project} does not run on {names.get(board, board)}: {cell.get('why', '')}",
             f"try one that ran here: {', '.join(alts[:6])}", alternatives=alts, why_not=cell.get("why_not"))
    return Result.good(f"{project} on {names.get(board, board)}: {cell['st']} ({cell.get('why', '')})",
                       status=cell["st"], source=cell.get("source"), radio=p.get("radio") or None)


# ---------------------------------------------------------------- tools

BUILD_TOOLS = ("sdk", "sysconfig", "compiler", "make", "toolbox")   # toolbox: the CC35xx image signer


def _tools(r, keys=None):
    found = {}
    for key, spec in r["tools"].items():
        if keys and key not in keys:
            continue
        found[key] = toolfind.find(spec["kind"], spec.get("validated"))
    return found


@guarded
def doctor(board, recipe_name, route="terminal", run=None):
    """Check exactly what this board, recipe and route need. A route without an assistant needs no assistant."""
    b, r = recipe(board, recipe_name)
    report, problems = {}, []
    for key, spec in r["tools"].items():
        try:
            c = toolfind.find(spec["kind"], spec.get("validated"))
            report[key] = {"path": c["path"], "version": c["version"], "validated": c["validated"], "source": c["source"]}
        except Stop as s:
            report[key] = {"missing": s.result["message"], "fix": s.result["next"], "code": s.result["code"]}
            problems.append(s.result)
    extra = {"claude": "claude", "codex": "codex"}.get(route)
    if extra:
        try:
            c = toolfind.find(extra)
            report[extra] = {"path": c["path"], "version": c["version"]}
        except Stop as s:
            problems.append(Result.make("refused", "FRL-E023", f"the {route} route needs {extra}, which is not installed",
                                        s.result["next"]))
            report[extra] = {"missing": problems[-1]["message"]}
    if problems:
        p = problems[0]
        return Result.make("refused", p["code"], f"{len(problems)} missing: " + "; ".join(x["message"] for x in problems),
                           (problems[0]["next"] if len(problems) == 1 else "\n    - " + "\n    - ".join(x["next"] for x in problems)), tools=report, route=route, host=host())
    if run:
        run.set("ready", tools=report)
    return Result.good(f"ready for {b['kit']} {recipe_name} by the {route} route", tools=report, route=route, host=host())


# ---------------------------------------------------------------- the project

def check_project_path(proj):
    """TI's example makefiles split paths at spaces (seen 2026-10-14: '@é/ticlang/device.opt' not found)."""
    if " " in str(proj):
        stop("refused", "FRL-E070", f"the project folder {proj} has a space in its path, which TI's example makefile cannot build",
             "choose a folder without spaces with --project, or set FRL_PROJECTS (needed when your home folder name has a space); your files stay where they are")


def default_project(board, recipe_name):
    return Path(os.environ.get("FRL_PROJECTS", Path.home() / "FoxRedLabs")) / f"{board}-{recipe_name}"


def ask_of(r, value):
    """The recipe's ask, parsed: a pattern (list of ms) or a message (text). `value` None means the default."""
    if r.get("edit") == "message":
        return checks.parse_message(value or r["default_message"])
    return checks.parse_pattern(value or r["default_pattern"])


def ask_text(r, ask):
    return ask if isinstance(ask, str) else ",".join(map(str, ask))


@guarded
def prepare(board, recipe_name, pattern=None, project=None, run=None):
    """Copy TI's example into the person's own folder (once) and set what was asked (LED pattern or message)."""
    b, r = recipe(board, recipe_name)
    ms, why = ask_of(r, pattern)
    if not ms:
        stop("refused", "FRL-E070", why)
    proj = Path(project).expanduser() if project else default_project(board, recipe_name)
    check_project_path(proj)
    sdk = toolfind.find(r["tools"]["sdk"].get("kind", "mspm0_sdk"), r["tools"]["sdk"].get("validated"))
    # example_repo: a project kept in this repository, where the SDK copy has no examples/ folder (the F2 SDK from TI's GitHub)
    src_example = (ROOT / r["example_repo"]) if r.get("example_repo") else Path(sdk["path"]) / r["example"]
    if not proj.exists():
        if not src_example.is_dir():
            stop("refused", "FRL-E020", f"the SDK at {sdk['path']} has no {r['example']}", "install the validated SDK version")
        proj.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(src_example, proj)
        for junk in ("iar", "keil", "gcc"):
            shutil.rmtree(proj / junk, ignore_errors=True)
    src = proj / r["source_file"]
    text = src.read_text(encoding="utf-8")
    if r.get("edit") == "message" and r.get("message_style") in ("display", "uart2echo"):
        new, line = (display_message if r["message_style"] == "display" else uart2echo_message)(text, ms)
        if new is None:
            stop("refused", "FRL-E070", f"{src} has been changed by hand and has no message line to edit",
                 "edit the message in it yourself, or move it aside and run prepare again to start from TI's example")
        src.write_text(new, encoding="utf-8")
        info = {"project": str(proj), "source": str(src), "source_sha256": sha256(src), "text": ms,
                "sdk": sdk["path"], "edited_line": line}
        if run:
            run.set(project=str(proj), message=ms, source_sha256=info["source_sha256"])
        return Result.good(f"message \"{ms}\" set in {src}", **info)
    if r.get("edit") == "message":
        line = f'uint8_t gTestingMsg[] = "> {ms} 0\\r\\n";'
        if MSG_MARK in text:
            new = re.sub(r'uint8_t gTestingMsg\[\] = "[^"\n]*";', lambda m: line, text, count=1)
        elif 'uint8_t gTestingMsg[] = "> Testing...0\\r\\n";' in text and "gTestingMsg[12]" in text:
            new = text.replace('uint8_t gTestingMsg[] = "> Testing...0\\r\\n";', MSG_MARK + "\n" + line)
            new = new.replace("gTestingMsg[12]", "gTestingMsg[sizeof(gTestingMsg) - 4]")
        else:
            stop("refused", "FRL-E070", f"{src} has been changed by hand and has no message line to edit",
                 "edit the message in it yourself, or move it aside and run prepare again to start from TI's example")
        src.write_text(new, encoding="utf-8")
        info = {"project": str(proj), "source": str(src), "source_sha256": sha256(src), "text": ms,
                "sdk": sdk["path"], "edited_line": line}
        if run:
            run.set(project=str(proj), message=ms, source_sha256=info["source_sha256"])
        return Result.good(f"message \"{ms}\" set in {src}", **info)
    table = "static const uint32_t pattern_ms[] = {" + ", ".join(map(str, ms)) + "};"
    if r.get("builder"):                              # build_<builder>.py: an example with its own loop and no makefile (am13, c28)
        import importlib
        new = importlib.import_module(f"build_{r['builder']}").with_pattern(text, ms, src)
    elif MARK in text:
        new = re.sub(r"static const uint32_t pattern_ms\[\] = \{[^}]*\};", table, text, count=1)
    elif r.get("template") == "gpio_driver" and "void *mainThread(void *arg0)" in text:
        head = text[: text.index("/*\n *  ======== mainThread")]
        new = head + MARK + "\n" + table + "\n\n" + driver_main(r)
    elif re.search(r"#define DELAY \(\d+\)", text) and "int main(void)" in text and "/* This results in approximately" in text:
        head = text[: text.index("/* This results in approximately")]
        new = head + MARK + "\n" + table + "\n" + pattern_main(r)
    else:
        stop("refused", "FRL-E070", f"{src} has been changed by hand and has no pattern table to edit",
             f"set the times in it yourself, or move it aside and run prepare again to start from TI's example")
    src.write_text(new, encoding="utf-8")
    info = {"project": str(proj), "source": str(src), "source_sha256": sha256(src), "pattern_ms": ms,
            "sdk": sdk["path"], "edited_line": table}
    if run:
        run.set(project=str(proj), pattern_ms=ms, source_sha256=info["source_sha256"])
    return Result.good(f"pattern {','.join(map(str, ms))} ms set in {src}", **info)


PATTERN_MAIN = """#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

int main(void)
{
    /* Power on GPIO, initialize pins as digital outputs */
    SYSCFG_DL_init();

    /* All LEDs off; the pattern drives LED2 red (USER_LED_2, PA26) only */
    DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN | GPIO_LEDS_USER_LED_2_PIN |
                                          GPIO_LEDS_USER_LED_3_PIN | GPIO_LEDS_USER_TEST_PIN);

    while (1) {
        for (unsigned i = 0; i < sizeof(pattern_ms) / sizeof(pattern_ms[0]); i++) {
            if (i % 2 == 0) {
                DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_2_PIN);
            } else {
                DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_2_PIN);
            }
            delay_cycles(pattern_ms[i] * CYCLES_PER_MS);
        }
    }
}
"""


def pattern_main(r):
    """PATTERN_MAIN for the board's example. A route names the LED macro its example's SysConfig gives the
    checked LED (led_macro), the LED pins to clear (clear_macros) and, where the example gives each pin its own
    port (LP-MSPM0G5187), per_pin_port; the LP-MSPM0L1306's text is the default."""
    if "led_macro" not in r:
        return PATTERN_MAIN
    name, pin = r["led"].rstrip(")").split(" (")
    short = r["led_macro"].removeprefix("GPIO_LEDS_").removesuffix("_PIN")
    port = (lambda m: m.removesuffix("_PIN") + "_PORT") if r.get("per_pin_port") else (lambda m: "GPIO_LEDS_PORT")
    clear = "\n".join(f"    DL_GPIO_clearPins({port(m)}, {m});" for m in r["clear_macros"])
    led, lp = r["led_macro"], port(r["led_macro"])
    return f"""#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

int main(void)
{{
    /* Power on GPIO, initialize pins as digital outputs */
    SYSCFG_DL_init();

    /* All LEDs off; the pattern drives {name} ({short}, {pin}) only */
{clear}

    while (1) {{
        for (unsigned i = 0; i < sizeof(pattern_ms) / sizeof(pattern_ms[0]); i++) {{
            if (i % 2 == 0) {{
                DL_GPIO_setPins({lp}, {led});
            }} else {{
                DL_GPIO_clearPins({lp}, {led});
            }}
            delay_cycles(pattern_ms[i] * CYCLES_PER_MS);
        }}
    }}
}}
"""


def driver_main(r):
    """The pattern loop for TI's SimpleLink F3 `drivers/empty` example (CC23xx, CC27xx): the GPIO driver writes the LED
    pin SysConfig names (led_macro) and usleep() paces it, instead of the MSPM0 examples' DL_GPIO calls and delay_cycles."""
    led = r["led_macro"]
    return f"""/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{{
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED pin, off */
    GPIO_setConfig({led}, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    while (1)
    {{
        for (unsigned i = 0; i < sizeof(pattern_ms) / sizeof(pattern_ms[0]); i++)
        {{
            GPIO_write({led}, (i % 2 == 0) ? CONFIG_GPIO_LED_ON : CONFIG_GPIO_LED_OFF);
            usleep(pattern_ms[i] * 1000u);
        }}
    }}
}}
"""


@guarded
def build(board, recipe_name, project=None, run=None):
    """Build the person's project with the validated (or explicitly chosen) SDK, SysConfig and compiler."""
    b, r = recipe(board, recipe_name)
    proj = Path(project).expanduser() if project else default_project(board, recipe_name)
    check_project_path(proj)
    if not (proj / r["build_dir"]).is_dir():
        stop("refused", "FRL-E070", f"no project at {proj}", f"run `frl prepare --board {board}` first")
    t = _tools(r, BUILD_TOOLS)          # building needs no probe tool
    t0 = time.time()
    if r.get("builder"):                # build_<builder>.py: SysConfig, the compiler and the SDK's libraries (am13, c28)
        import importlib
        image = importlib.import_module(f"build_{r['builder']}").build(proj, t, r, proj / "frl-build.log")
    else:
        env = dict(os.environ)
        args = [t["make"]["exe"], "-C", str(proj / r["build_dir"]),
                f"{r.get('sdk_var', 'MSPM0_SDK_INSTALL_DIR')}={t['sdk']['path']}", f"SYSCONFIG_TOOL={t['sysconfig']['path']}",
                f"TICLANG_ARMCOMPILER={t['compiler']['path']}", "GCC_ARMCOMPILER="]
        p = subprocess.run(args, capture_output=True, text=True, timeout=600, env=env)
        log = proj / r["build_dir"] / "frl-build.log"
        log.write_text(p.stdout + p.stderr, encoding="utf-8")
        image = proj / r["image"]
        if p.returncode != 0 or not image.is_file():
            tail = (p.stdout + p.stderr).strip().splitlines()[-12:]
            stop("failed", "FRL-E030", "the build failed: " + (tail[-1] if tail else "no output"),
                 f"read the first error in {log}; look it up in the board's AGENTS.md and FAQ.md", log=str(log), tail=tail)
    versions = {k: {"version": v["version"], "validated": v["validated"], "source": v["source"]} for k, v in t.items()}
    info = {"image": str(image), "image_sha256": sha256(image), "seconds": round(time.time() - t0, 1), "tools": versions,
            "unvalidated": [k for k, v in versions.items() if not v["validated"]]}
    if run:
        run.set("built", image=str(image), image_sha256=info["image_sha256"], build_tools=versions)
    return Result.good(f"built {image.name} in {info['seconds']} s", **info)


# ---------------------------------------------------------------- the board

def _lab_board_lock(board):
    """The bench's own per-board lock dir (settings.local.json "lab_lock_dir"; lab machines only): a session
    working by hand holds <dir>/<board>.lock, and frl must not write under it."""
    try:
        s = json.loads((ROOT / "tools" / "benchmark" / "settings.local.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return
    d = s.get("lab_lock_dir")
    if d and (Path(d).expanduser() / f"{board}.lock").exists() and os.environ.get("FRL_LAB_LOCK_OK") != board:
        stop("refused", "FRL-E004", f"another session holds the bench lock {Path(d).expanduser() / (board + '.lock')}",
             "wait for that session, or ask it to release the board; set FRL_LAB_LOCK_OK=<board> only if you hold it yourself")


def _probe_and_lock(board, probe, run, op):
    _lab_board_lock(board)
    serial = probes.select(probe)
    if run:
        run.set(probe=serial)
    return serial, ProbeLock(serial, run.id if run else "", op)


def _stale(lk, run):
    if lk.previous and lk.previous.get("state") == "writing":
        msg = f"the last run on this probe ({lk.previous.get('run_id') or 'unknown'}) stopped while writing; the board state is unknown"
        if run:
            run.log("stale-write", lk.previous)
        return msg
    return ""


def _reprogram_backup(board, b, serial, ident):
    """A board with no read-back (CC35xx): keep the fuse report, the chip's unique ID and what the running program
    prints, and a hashed copy of the signed image the restore will program (FRL_RESTORE_IMAGE: a tool_settings.json,
    or a built .out beside one, whose bytes the signed vendor image must hold). backup.json goes into the backup
    index, so restore refuses an edited one, like every other backup."""
    import time as _t
    import backup as bk
    import toolbox
    img = os.environ.get("FRL_RESTORE_IMAGE")
    settings = toolbox.settings_for(img) if img else None
    if settings and Path(img).suffix == ".out":
        toolbox.check_from_build(img, settings)
    bk.BACKUPS.mkdir(parents=True, exist_ok=True)
    os.chmod(bk.BACKUPS, 0o700)
    folder = bk.BACKUPS / f"{board}-{serial}-{_t.strftime('%Y%m%d-%H%M%S')}"
    folder.mkdir(mode=0o700)
    bundle = toolbox.copy_bundle(settings, folder / "bundle") if settings else None
    (folder / "fuse-data.txt").write_text(toolbox.fuse_data(serial), encoding="utf-8")
    port = target.console_port(b, serial)
    target.start(b, serial)
    raw = checks.read_serial(port, b["console"]["baud"], b["target"].get("banner_seconds", 6))
    (folder / "console.txt").write_bytes(raw)
    banner = next((ln.strip() for ln in raw.decode("ascii", "replace").replace("\r", "\n").split("\n") if ln.strip().strip("=")), "")
    man = {"board": board, "probe": serial, "kind": "reprogram", "identity": ident, "udid": ident.get("udid"), "banner": banner,
           "bundle": bundle, "layout_now": toolbox.known_layout(ident.get("udid")), "when": _t.strftime("%Y-%m-%dT%H:%M:%S")}
    raw_man = (json.dumps(man, indent=1) + "\n").encode()
    (folder / "backup.json").write_bytes(raw_man)
    idx = bk.read_index()
    idx[folder.name] = bk.sha(raw_man)
    tmp = bk.index_path().with_suffix(".tmp")
    tmp.write_text(json.dumps(idx, indent=1) + "\n")
    os.replace(tmp, bk.index_path())
    return folder, man


@guarded
def backup(board, probe=None, run=None):
    """Read the board's restorable regions twice, compare, keep them in the per-user data folder."""
    import backup as bk
    b = board_route(board)
    serial, lk = _probe_and_lock(board, probe, run, "backup")
    if b["target"].get("backup") == "reprogram":
        with lk:
            ident = target.check_identity(b, serial)
            folder, man = _reprogram_backup(board, b, serial, ident)
        if run:
            run.set("backed_up", backup=str(folder), identity=ident)
        note = ("this chip has no read-back: saved its fuse report, its unique ID and what the program prints"
                + (f" ({man['banner'][:60]}); the restore programs the saved copy of {man['bundle']['from']}" if man["bundle"]
                   else "; set FRL_RESTORE_IMAGE to a signed image before the restore can run"))
        return Result.good(f"backed up to {folder}: {note}", backup=str(folder), identity=ident, banner=man["banner"],
                           bundle=man["bundle"], warning=None if man["bundle"] else note)
    with lk:
        ident = target.check_identity(b, serial)
        with contextlib.redirect_stdout(sys.stderr):   # keep stdout for the result (--json, MCP)
            folder = bk.save(board, serial, identity=ident)
    if run:
        run.set("backed_up", backup=str(folder), identity=ident)
    return Result.good(f"backed up to {folder}", backup=str(folder), identity=ident)


@guarded
def flash(board, image, probe=None, image_sha256=None, run=None):
    """Write one image to the selected, identified board, inside the route's writable regions."""
    b = board_route(board)
    image = Path(image).expanduser()
    if not image.is_file():
        stop("refused", "FRL-E031", f"no image at {image}", "build it with `frl build`")
    if image_sha256 and sha256(image) != image_sha256:
        stop("refused", "FRL-E031", "the image changed after it was built", "rebuild with `frl build`")
    if b["target"].get("transport") == "toolbox":      # CC35xx: the bytes written are a signed bundle, not the .out
        return _toolbox_flash(board, b, image, probe, run)
    segs = target.check_writes(b, image)
    serial, lk = _probe_and_lock(board, probe, run, "flash")
    with lk:
        warn = _stale(lk, run)
        if b["target"].get("write") == "dslite_lpf3":     # said before the write, as TI's own tools would
            import lpf3
            print(f"frl: {lpf3.ERASE_WARNING}", file=sys.stderr)
            warn = "; ".join(x for x in (warn, lpf3.ERASE_WARNING) if x)
        ident = target.check_identity(b, serial)
        if run:
            run.set("programming", identity=ident, writing={"image": str(image), "sha256": sha256(image), "segments": segs})
        lk.write_owner("writing")
        t0 = time.time()
        out = target.program(b, serial, str(image))
        took = time.time() - t0
        lk.write_owner("holding")
    write = dict(target.write_report(out), total_s=round(took, 1), path=target.write_path(b))
    if run:
        run.set("programmed", write=write, connect=target.connect_note(serial))
    how = "programmed" if b["target"].get("transport") == "toolbox" else "programmed and read back"   # the Toolbox has no read-back
    return Result.good(f"{how} {image.name} on {b['kit']} (probe {serial}) in {took:.1f} s",
                       probe=serial, identity=ident, segments=segs, image_sha256=sha256(image), warning=warn,
                       write=write, connect=target.connect_note(serial))


@guarded
def verify_image(board, image, probe=None, run=None):
    """Is this exact image on this board? Read-only compare of the flash with the image."""
    b = board_route(board)
    serial, lk = _probe_and_lock(board, probe, run, "verify-image")
    with lk:
        ident = target.check_identity(b, serial)
        same, out = target.verify_image(b, serial, str(Path(image).expanduser()))
    if not same:
        return Result.make("failed", "FRL-E040", f"the flash on probe {serial} does not hold {Path(image).name}",
                           "flash it with `frl flash`, or put the backup back with `frl restore`", identity=ident, openocd=out[-400:])
    return Result.good(f"the flash holds exactly {Path(image).name}", identity=ident, image_sha256=sha256(image))


@guarded
def check(board, recipe_name, probe=None, pattern=None, image=None, seconds=None, run=None):
    """The recipe's own check: this image is on this board, and the board does what was asked."""
    b, r = recipe(board, recipe_name)
    v = r.get("verify") or {}
    if v.get("kind") not in ("pattern", "uart"):
        stop("unverified", "FRL-E041", f"no check exists for {recipe_name} on {b['kit']}",
             "look at the board yourself; this run is recorded as not verified")
    ask, why = ask_of(r, pattern)
    if not ask:
        stop("refused", "FRL-E070", why)
    serial, lk = _probe_and_lock(board, probe, run, "verify")
    with lk:
        ident = target.check_identity(b, serial)
        image_ok = None
        if image:
            image_ok, _ = target.verify_image(b, serial, str(Path(image).expanduser()))
        if v["kind"] == "pattern":
            led = next(x for x in b["leds"] if x["label"] == r["led"])
            s_ = target.sample(b, serial, led, seconds or v.get("seconds", 4.0), image=str(Path(image).expanduser()) if image else None)
            res = checks.pattern_check(s_, ask, v.get("tolerance_ms", 40), led["label"], v.get("min_rate", 100))
        else:
            port = target.console_port(b, serial)
            if image:            # verify_image halted and resumed the core; start the program cleanly
                target.start(b, serial)
            raw = checks.read_serial(port, b["console"]["baud"], seconds or v.get("seconds", 4.0))
            res = checks.message_check(raw, ask, port)
    res["data"].update(identity=ident, image_on_board=image_ok, connect=target.connect_note(serial))
    if image and image_ok is False and res["ok"]:
        res = Result.make("failed", "FRL-E040", "the board does what was asked, but the flash is not the image just built",
                          "flash the new image", **res["data"])
    if run:
        run.set({"ok": "verified", "failed": "failed"}.get(res["outcome"], "unverified"), check=res["data"])
    return res


@guarded
def restore(folder, probe=None, board=None, run=None):
    """Put a backup back on the same board it came from; refuses another board or changed files."""
    import backup as bk
    man = json.loads((Path(folder) / "backup.json").read_text(encoding="utf-8"))
    b = board_route(board or man["board"])
    if man.get("kind") == "reprogram":
        return _reprogram_restore(folder, man, b, probe, run)
    serial, lk = _probe_and_lock(man["board"], probe or man.get("probe"), run, "restore")
    with lk:
        warn = _stale(lk, run)
        ident = target.check_identity(b, serial)
        lk.write_owner("writing")
        with contextlib.redirect_stdout(sys.stderr):
            out = bk.restore(folder, serial, identity=ident)
        lk.write_owner("holding")
    return Result.good(f"restored and read back {Path(folder).name}", identity=ident, log=out, warning=warn)


def _toolbox_flash(board, b, image, probe, run):
    """CC35xx: check the signed images hold the .out, copy and hash them, check the flash layout against the chip,
    program the copy and record the layout. The result names the copy's hash: that is what was written."""
    import tempfile
    import toolbox
    settings = toolbox.settings_for(image)
    toolbox.check_from_build(image, settings)
    serial, lk = _probe_and_lock(board, probe, run, "flash")
    with tempfile.TemporaryDirectory(prefix="frl-cc35-") as tmp, lk:
        bundle = toolbox.copy_bundle(settings, Path(tmp) / "bundle")
        staged = toolbox.check_copy(Path(tmp) / "bundle", bundle)
        warn = _stale(lk, run)
        ident = target.check_identity(b, serial)
        layout = toolbox.check_layout(ident["udid"], bundle["layout"])
        if run:
            run.set("programming", identity=ident, writing={"image": str(image), "sha256": sha256(image), "bundle": bundle["sha256"]})
        lk.write_owner("writing")
        t0 = time.time()
        out = toolbox.program(b, serial, str(staged))
        took = time.time() - t0
        toolbox.record_layout(ident["udid"], bundle["layout"])
        lk.write_owner("holding")
    comps = [{"file": n, "sha256": h} for n, h in bundle["files"].items()]
    write = dict(target.write_report(out), total_s=round(took, 1), path=target.write_path(b), layout=layout)
    if run:
        run.set("programmed", write=write)
    return Result.good(f"programmed the signed images of {image.name} ({bundle['sha256'][:12]}) on {b['kit']} (probe {serial}) in {took:.1f} s; "
                       "the Toolbox has no read-back", probe=serial, identity=ident, segments=comps, image_sha256=sha256(image),
                       bundle={"sha256": bundle["sha256"], "layout": bundle["layout"]}, warning=warn, write=write)


def _reprogram_restore(folder, man, b, probe, run):
    """Program the copy saved at backup time, after its hashes, the chip's unique ID and the flash layout check, then
    check the program prints, as a whole line, the first line it printed then."""
    import backup as bk
    import toolbox
    d = bk.check_folder(folder)
    raw = (d / "backup.json").read_bytes()
    if bk.read_index(d.parent).get(d.name) != bk.sha(raw):
        stop("refused", "FRL-E052", "backup.json changed since it was saved, or was not made by frl backup here",
             "take a new backup with `frl backup`")
    if not man.get("bundle"):
        stop("refused", "FRL-E053", "this backup names no image to put back: the chip has no read-back, so the restore reprograms a signed image",
             "build the image you want back (or take TI's), then run the backup again with FRL_RESTORE_IMAGE=<its tool_settings.json>")
    if not man.get("udid"):
        stop("refused", "FRL-E051", "this backup was taken before frl read the chip's unique ID, so it cannot prove which chip it belongs to",
             "take a new backup with `frl backup`")
    staged = toolbox.check_copy(d / "bundle", man["bundle"])
    serial, lk = _probe_and_lock(man["board"], probe or man.get("probe"), run, "restore")
    if serial != man.get("probe"):
        stop("refused", "FRL-E051", f"this backup was taken through probe {man.get('probe')}, not {serial}", "pick that probe with --probe")
    with lk:
        ident = target.check_identity(b, serial)
        if ident.get("udid") != man["udid"]:
            stop("refused", "FRL-E051", f"this backup belongs to the chip with ID {man['udid']}; probe {serial} is on {ident.get('udid')}",
                 "move the probe back to the board the backup came from", identity=ident)
        layout = toolbox.check_layout(man["udid"], man["bundle"]["layout"])
        lk.write_owner("writing")
        out = toolbox.program(b, serial, str(staged))
        toolbox.record_layout(man["udid"], man["bundle"]["layout"])
        lk.write_owner("holding")
        port = target.console_port(b, serial)
        raw = checks.read_serial(port, b["console"]["baud"], b["target"].get("banner_seconds", 6))
    text = raw.decode("ascii", "replace")
    lines = {ln.strip() for ln in text.replace("\r", "\n").split("\n")}
    same = bool(man.get("banner")) and man["banner"] in lines
    (d / "restore-console.txt").write_bytes(raw)
    if not same:
        return Result.make("failed", "FRL-E040", f"the program put back does not print the line it printed at backup time ({man.get('banner', '')[:60]})",
                           "check that FRL_RESTORE_IMAGE named the image that was on the board; program it again with `frl restore`",
                           identity=ident, console=text[-300:])
    return Result.good(f"programmed the saved copy ({man['bundle']['sha256'][:12]}) back; it prints \"{man['banner'][:60]}\" again",
                       identity=ident, write=dict(target.write_report(out), layout=layout), console=text[-300:])


# ---------------------------------------------------------------- the whole route

@guarded
def run_route(board, recipe_name, pattern=None, probe=None, project=None, backup_first=True, resume=None, run=None):
    """identify -> doctor -> backup -> prepare -> build -> flash -> verify, journaled; a failure stops it."""
    b, r = recipe(board, recipe_name)
    pattern = pattern or r.get("default_pattern") or r.get("default_message")
    rn = journal.Run.load(resume) if resume else journal.Run.new(board=board, recipe=recipe_name, pattern=pattern,
                                                                 route="terminal")
    st = rn.state()
    if st["state"] == "programming":
        stop("failed", "FRL-E033", "this run stopped during a write; the board state is unknown",
             f"check it first: frl verify-image --board {board} --image {st.get('image')} --probe {st.get('probe')}", run_id=rn.id)
    pattern = st.get("pattern") or pattern
    probe = probe or st.get("probe")

    def step(fn, *a, **k):
        res = fn(*a, run=rn, **k)
        if not res["ok"]:
            if res["code"] == "FRL-E033":
                pass            # stays 'programming': the board is unknown until checked
            elif res["outcome"] == "refused":
                rn.set("refused")
            elif res["outcome"] == "unverified":
                rn.set("unverified")
            else:
                rn.set("failed")
            raise Stop(Result.make(res["outcome"], res["code"], res["message"], res.get("next", ""),
                                   run_id=rn.id, step=fn.__name__, **res["data"]))
        return res

    e = step(eligible, board, r["project"])
    sel = probes.select(probe)
    rn.set(probe=sel)
    step(identify, board=board, probe=sel)
    rn.set("identified")
    if st["state"] in ("created", "identified"):
        step(doctor, board, recipe_name, "terminal")
    if backup_first and not rn.state().get("backup"):
        step(backup, board, probe=sel)
    elif not backup_first and not rn.state().get("backup"):
        rn.set("backup_unavailable", step={"name": "backup", "outcome": "skipped", "message": "skipped on request"})
    step(prepare, board, recipe_name, pattern, project=project or st.get("project"))
    built = step(build, board, recipe_name, project=rn.state()["project"])
    step(flash, board, built["data"]["image"], probe=sel, image_sha256=built["data"]["image_sha256"])
    res = check(board, recipe_name, probe=sel, pattern=pattern, image=built["data"]["image"], run=rn)
    st = rn.state()
    res["data"].update(run_id=rn.id, project=st["project"], source=str(Path(st["project"]) / r["source_file"]),
                       backup=st.get("backup"), next_projects=r.get("next", []), summary=rn.summary())
    if res["ok"]:
        what = "message is printing from" if r.get("edit") == "message" else "pattern is running on"
        res["message"] = f"Your new {what} {b['kit']}. " + res["message"]
    elif res["outcome"] == "unverified":
        res["message"] = "Image loaded. Check the board to finish. " + res["message"]
    return res


@guarded
def flash_prebuilt(manifest, probe=None, run=None):
    """Flash a prebuilt image after checking its manifest, then run the recipe's check on it."""
    import prebuilt
    m, img = prebuilt.load(manifest)
    res = flash(m["board"], str(img), probe=probe, image_sha256=m["image"]["sha256"], run=run)
    if not res["ok"]:
        return res
    ask = m["message"] if "message" in m else ",".join(map(str, m["pattern_ms"]))
    chk = check(m["board"], m["recipe"], probe=res["data"]["probe"], pattern=ask, image=str(img), run=run)
    chk["data"].update(manifest=str(manifest), source_commit=m["source"]["repo_commit"], rebuild=m["source"]["rebuild"],
                       release_cleared=m.get("release_cleared", False))
    return chk
