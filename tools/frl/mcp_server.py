#!/usr/bin/env python3
"""The frl operations as an MCP server over stdio, for desktop assistants (Codex, Claude Desktop, Claude Code).

  python3 tools/frl/mcp_server.py        (or: python3 tools/frl/frl.py mcp)

Standard library only: newline-delimited JSON-RPC 2.0 on stdin and stdout, nothing on the network. Each tool
calls the same function the CLI calls. A result that is not ok comes back with isError true, so a client
cannot read a failed or unverified check as success. Tools that write to a board (flash, restore) need the
probe serial named in the call; they never pick one. frl_host is harmless: it says which computer the
server runs on and whether it can see USB probes, which is the first thing to check in a new app or mode.
The site_* tools serve the site's own data read-only (tools/frl/site_data.py): boards, benchmark rows, builds, parts.
"""
import argparse
import json
import os
import sys
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import journal  # noqa: E402
import ops  # noqa: E402
import probes  # noqa: E402
import site_data  # noqa: E402
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "bench"))
from bench_api import bench_run  # noqa: E402
from core import VERSION, Result, host  # noqa: E402

PROTOCOL = "2024-11-05"
CATALOG_ONLY = False
CATALOG_TOOLS = frozenset({"site_boards", "site_results", "site_builds", "site_build", "site_parts", "site_doc", "site_ports", "site_support", "site_api", "site_questions"})
B = {"board": {"type": "string", "description": "board id, e.g. mspm0l1306 (frl_identify finds it from the printed name)"}}
R = {"recipe": {"type": "string", "default": "blink-pattern"}}
P = {"probe": {"type": "string", "description": "XDS110 serial from frl_detect"}}
PAT = {"pattern": {"type": "string", "description": "blink-pattern: LED on,off,on,off ... ms, e.g. 150,150,150,900; print-message: the text to print (1 to 40 characters)"}}


def schema(props, required=()):
    return {"type": "object", "properties": props, "required": list(required), "additionalProperties": False}


TOOLS = {
    "frl_host": ("Which computer this server runs on, its OS and CPU, whether it looks like a VM or container, the interface version, and whether USB probes are visible. Read-only; touches no board.",
                 schema({}), lambda a: host_info()),
    "frl_detect": ("List XDS110 debug probes on USB. A probe is not a board. Read-only.", schema({}), lambda a: ops.detect()),
    "frl_identify": ("Match the kit name printed on the board, or (with board and probe) read the chip behind a probe and check it is that board. Read-only.",
                     schema({"name": {"type": "string"}, **B, **P}), lambda a: ops.identify(a.get("name"), a.get("board"), a.get("probe"))),
    "frl_eligible": ("Has this project run on this board? Answer before installing anything.",
                     schema({**B, "project": {"type": "string"}}, ["board", "project"]), lambda a: ops.eligible(a["board"], a["project"])),
    "frl_doctor": ("Check the tools this board, recipe and route need. Read-only.",
                   schema({**B, **R, "route": {"type": "string", "enum": ["terminal", "claude", "codex"]}}, ["board"]),
                   lambda a: ops.doctor(a["board"], a.get("recipe", "blink-pattern"), a.get("route", "terminal"))),
    "frl_prepare": ("Copy TI's example into the person's project folder (once) and set the LED pattern in its source.",
                    schema({**B, **R, **PAT, "project": {"type": "string"}}, ["board"]),
                    lambda a: ops.prepare(a["board"], a.get("recipe", "blink-pattern"), a.get("pattern"), a.get("project"))),
    "frl_build": ("Build the project with the validated SDK, SysConfig and compiler.", schema({**B, **R, "project": {"type": "string"}}, ["board"]),
                  lambda a: ops.build(a["board"], a.get("recipe", "blink-pattern"), a.get("project"))),
    "frl_backup": ("Read the board's flash twice, compare, and keep it in the per-user data folder. Reads only.",
                   schema({**B, **P}, ["board", "probe"]), lambda a: ops.backup(a["board"], a["probe"])),
    "frl_flash": ("WRITES the board: program one image to the named probe's board after checking the chip and the write limits.",
                  schema({**B, **P, "image": {"type": "string"}}, ["board", "probe", "image"]),
                  lambda a: ops.flash(a["board"], a["image"], a["probe"])),
    "frl_verify": ("Check the result on the board through the debugger: this image is on it and the LED follows the pattern. Read-only.",
                   schema({**B, **R, **P, **PAT, "image": {"type": "string"}}, ["board", "probe"]),
                   lambda a: ops.check(a["board"], a.get("recipe", "blink-pattern"), a["probe"], a.get("pattern"), a.get("image"))),
    "frl_verify_image": ("Is this exact image on the board? Read-only compare.", schema({**B, **P, "image": {"type": "string"}}, ["board", "probe", "image"]),
                         lambda a: ops.verify_image(a["board"], a["image"], a["probe"])),
    "frl_restore": ("WRITES the board: put a backup back on the board it was read from; refuses any other board.",
                    schema({"folder": {"type": "string"}, **P}, ["folder", "probe"]), lambda a: ops.restore(a["folder"], a["probe"])),
    "frl_status": ("The latest run (or a named one) as a short summary a fresh session can act on.",
                   schema({"run": {"type": "string"}}), lambda a: status(a.get("run"))),
    "bench_run": ("WRITES the board: one call on a bench board (lab bench only): take the probe lock, flash the image, read the console for secs seconds, check the expect regex, release. Boards: tools/bench/boards.json. Returns the console text, image sha256 and flash time.",
                  schema({**B, "image": {"type": "string", "description": "image file (or an ESP-IDF build folder for esp32c6); omit to read the console only"}, "secs": {"type": "number", "default": 5},
                          "expect": {"type": "string", "description": "regex the console must match"}, "offset": {"type": "string", "description": "flash address for a .bin (hex)"}, **P,
                          "record": {"type": "string", "description": "test id: append the flash time to data/results/<board>.csv"}}, ["board"]),
                  lambda a: bench_run(a)),
    # the site's data, read-only (BRIEF.md step 8): the same files the site pages are drawn from
    "site_boards": ("Every board with its core, clock, flash and RAM, its site page and its results file. Read-only.", schema({}), lambda a: site_data.boards()),
    "site_results": ("Benchmark rows for one board, as stored in data/results/<board>.csv (every measured number, and every test that could not run with its reason). Read-only.",
                     schema({**B, "test": {"type": "string", "description": "a test id from data/benchmark-suite.json, e.g. coremark or control_loop"}}, ["board"]),
                     lambda a: site_data.results(a["board"], a.get("test"))),
    "site_builds": ("Every build on the site: title, the boards it works on and the boards it is on the bench for, its page and its Markdown text. Read-only.",
                    schema({}), lambda a: site_data.builds()),
    "site_build": ("One build: each version's parts, pin table, steps with their commands, expected result and limits, and the status per board. Read-only.",
                   schema({"slug": {"type": "string", "description": "from site_builds, e.g. handwritten-digits"}}, ["slug"]), lambda a: site_data.build(a["slug"])),
    "site_parts": ("Parts from any maker that the builds use: what each is, how to hook it up, where to buy it and which builds use it. Read-only.",
                   schema({"query": {"type": "string"}}), lambda a: site_data.parts(a.get("query"))),
    # content tools (2026-10-17): the text an assistant used to get only as a path
    "site_doc": ("Read one document as text: a board folder's AGENTS.md, FAQ.md, TRAPS.md, README.md or START.md, or a page under docs/, portability/, ports/ or recipes/. Read-only; feedback logs are not served.",
                 schema({"path": {"type": "string", "description": "repository path, e.g. mspm0l1306/AGENTS.md or docs/methodology.md"}}, ["path"]), lambda a: site_data.doc(a["path"])),
    "site_ports": ("Every port of another ecosystem's code onto TI chips: what was ported, from where, what ran and its limits. Read-only.", schema({}), lambda a: site_data.ports()),
    "site_support": ("The works-with grid: per TI family, which tools, operating systems and maker platforms ran, built, ship from the vendor, or are untried, with the evidence path. Read-only.",
                     schema({"family": {"type": "string", "description": "filter, e.g. MSPM0"}}), lambda a: site_data.support_matrix(a.get("family"))),
    "site_questions": ("The question contract: every question the site answers for a person or an assistant, where each answer lives (files), what a correct answer contains, and whether a model is needed (never on our side). Read-only.", schema({}), lambda a: site_data.questions()),
    "site_api": ("The versioned data API: schema version, generation date and the files under site/api/v1/ with their row counts and checksums. Read-only.", schema({}), lambda a: site_data.api_index()),
}

try:   # optional tools; not in every copy
    import design_tools
    TOOLS.update(design_tools.mcp_tools(schema))
    CATALOG_TOOLS = CATALOG_TOOLS | design_tools.MCP_NAMES
    OPTIONAL_TOOLS = design_tools.MCP_NAMES   # served, but not listed on the public assistant page
except ImportError:
    OPTIONAL_TOOLS = frozenset()


# the token label: none = no model anywhere in the call, the tool computes or reads;
# optional = a route of claude or codex checks that assistant's own CLI, the result is the same without one; required never appears
TOKENS = {name: "none" for name in TOOLS}
TOKENS["frl_doctor"] = "optional"
TOKEN_TEXT = {"none": "Tokens: none (no model in the loop).", "optional": "Tokens: optional (a claude or codex route checks that assistant's CLI; the result is the same without one)."}
for _n, (_d, _s, _f) in list(TOOLS.items()):
    TOOLS[_n] = (_d.rstrip() + " " + TOKEN_TEXT[TOKENS[_n]], _s, _f)

WRITES = {"frl_flash", "frl_restore", "bench_run"}            # change the board: clients should ask the person
LOCAL_FILES = {"frl_prepare", "frl_build", "frl_backup"}   # write only the person's project or backup folder


def annotations(name):
    """MCP tool hints. Desktop clients use them to decide when to ask: reads pass, board writes ask."""
    return {"readOnlyHint": name not in WRITES | LOCAL_FILES, "destructiveHint": name in WRITES,
            "idempotentHint": name not in WRITES, "openWorldHint": False, "tokens": TOKENS[name]}


def host_info():
    try:
        ps = probes.list_probes()
        usb = {"probes_visible": len(ps), "serials": [p["serial"] for p in ps]}
    except Exception as e:  # report, do not hide, a host that cannot see USB
        usb = {"probes_visible": None, "error": f"{type(e).__name__}: {e}"}
    return Result.good(f"frl {VERSION} on {host()['node']}", interface=VERSION, host=host(), usb=usb,
                       python=sys.executable, capabilities=sorted(offered()))


def status(run_id):
    r = journal.Run.load(run_id) if run_id else journal.latest()
    if not r:
        return Result.make("refused", "FRL-E070", "no runs yet")
    return Result.good(r.summary(), run=r.state())


def offered():
    """FRL_MCP_TOOLS=frl_host,frl_detect limits the server to those tools (the harmless first-install check)."""
    if CATALOG_ONLY:
        return {n: t for n, t in TOOLS.items() if n in CATALOG_TOOLS}
    want = [t for t in os.environ.get("FRL_MCP_TOOLS", "").split(",") if t]
    return {n: t for n, t in TOOLS.items() if not want or n in want}


def handle(msg):
    mid, method, params = msg.get("id"), msg.get("method"), msg.get("params") or {}
    if method == "initialize":
        return {"protocolVersion": params.get("protocolVersion", PROTOCOL), "capabilities": {"tools": {}},
                "serverInfo": {"name": "fox-red-labs-frl", "version": VERSION},
                "instructions": ("Fox Red Labs catalogue from this local copy. No board tools are available. "
                                 "Cite the returned sources and limits; truncated results are incomplete. " if CATALOG_ONLY else
                                 "Fox Red Labs local board tools and the site's data. Call frl_host first; it says which computer has the board. ") +
                                "The site_* tools read the boards, benchmark rows, builds and parts the site is drawn from, and touch nothing. "
                                "A tool result with isError is a failure or an unverified check: report it as that."}
    if method == "ping":
        return {}
    if method == "tools/list":
        return {"tools": [{"name": n, "description": d, "inputSchema": s, "annotations": annotations(n)}
                          for n, (d, s, _) in offered().items()]}
    if method == "tools/call":
        name, args = params.get("name"), params.get("arguments") or {}
        if name not in offered():
            raise LookupError(f"no tool {name}")
        res = TOOLS[name][2](args)
        return {"content": [{"type": "text", "text": res.text() + "\n\n" + json.dumps(res, default=str)}],
                "structuredContent": json.loads(json.dumps(res, default=str)), "isError": not res["ok"]}
    if mid is None:
        return None          # a notification
    raise NotImplementedError(method)


def serve(inp=sys.stdin, out=sys.stdout):
    real_out = out
    sys.stdout = sys.stderr   # anything printed by an operation goes to stderr, never into the protocol
    for line in inp:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        try:
            result = handle(msg)
            reply = None if msg.get("id") is None else {"jsonrpc": "2.0", "id": msg["id"], "result": result}
        except Exception as e:
            traceback.print_exc()
            code = -32601 if isinstance(e, NotImplementedError) else -32602 if isinstance(e, (LookupError, KeyError)) else -32603
            reply = {"jsonrpc": "2.0", "id": msg.get("id"), "error": {"code": code, "message": f"{type(e).__name__}: {e}"}}
        if reply is not None:
            real_out.write(json.dumps(reply) + "\n")
            real_out.flush()
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Fox Red Labs local MCP tools")
    parser.add_argument("--catalog-only", action="store_true", help="offer only the five read-only catalogue tools; no board operations")
    CATALOG_ONLY = parser.parse_args().catalog_only
    sys.exit(serve())
