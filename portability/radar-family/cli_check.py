#!/usr/bin/env python3
"""Check a radar .cfg against the CLI argument counts of an mmWave L SDK demo, offline.

Reads the demo's command table (cmd -> handler), the condition in front of each handler's
"Error: Invalid usage" line, and the highest argv[n] it reads, then reports every cfg line the firmware
built from that source would reject, or whose last values it would drop.

Usage: cli_check.py <demo source dir> <file.cfg> [...]   e.g.
  cli_check.py ~/ti/mmwave_l_sdk_05_05_04_02/examples/mmw_demo/motion_and_presence_detection/source iwrl6432boost/live/cfg/*.cfg
       cli_check.py --selftest
Exit 1 if any line is rejected or has values dropped; a count that depends on a run-time value is a note only. Limits: other `#ifdef` branches are read as written (the GTRACK_3D branch is kept, which is
what the demo builds); a value that lands in a different field than intended is not caught (read the handler)."""
import ast, operator, pathlib, re, sys

MACROS = {}  # numeric #defines from the SDK headers, for conditions such as argc != 1 + 2*TX*RX

_OPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
        ast.Eq: operator.eq, ast.NotEq: operator.ne, ast.Lt: operator.lt, ast.LtE: operator.le,
        ast.Gt: operator.gt, ast.GtE: operator.ge}

def _eval(expr, names):
    """Evaluate a C argc condition without eval(): only numbers, names, + - *, comparisons, and/or/not."""
    def ev(n):
        if isinstance(n, ast.Expression):
            return ev(n.body)
        if isinstance(n, ast.Constant) and isinstance(n.value, int):
            return n.value
        if isinstance(n, ast.Name):
            if n.id not in names:
                raise NameError(n.id)
            return names[n.id]
        if isinstance(n, ast.BinOp) and type(n.op) in _OPS:
            return _OPS[type(n.op)](ev(n.left), ev(n.right))
        if isinstance(n, ast.UnaryOp) and isinstance(n.op, ast.Not):
            return not ev(n.operand)
        if isinstance(n, ast.BoolOp):
            vals = [ev(v) for v in n.values]
            return all(vals) if isinstance(n.op, ast.And) else any(vals)
        if isinstance(n, ast.Compare) and all(type(o) in _OPS for o in n.ops):
            left = ev(n.left)
            for o, r in zip(n.ops, n.comparators):
                right = ev(r)
                if not _OPS[type(o)](left, right):
                    return False
                left = right
            return True
        raise ValueError(f"unsupported expression: {ast.dump(n)[:60]}")
    return ev(ast.parse(expr, mode="eval"))

def _cond(c):
    c = re.sub(r"(\d+)[uU]\b", r"\1", c)
    c = c.replace("&&", " and ").replace("||", " or ")
    return "(" + re.sub(r"!(?!=)", " not ", c).replace("\n", " ") + ")"

def table(src):
    """cmd -> (list of reject conditions as Python expressions in argc, highest argv index read)."""
    text = "".join(p.read_text(errors="replace") for p in sorted(pathlib.Path(src).rglob("*.c")))
    sdk = pathlib.Path(src).resolve()
    while sdk.parent != sdk and not (sdk / "firmware").is_dir():
        sdk = sdk.parent
    for h in list(pathlib.Path(src).rglob("*.h")) + list((sdk / "source").rglob("*.h")):
        for k, v in re.findall(r"^\s*#define\s+(\w+)\s+\(?(\d+)[uU]?\)?(?=\s|/|$)", h.read_text(errors="replace"), re.M):
            MACROS.setdefault(k, int(v))
    text = re.sub(r"#if(?:def)?\s+GTRACK_3D\s*\n(.*?)#else.*?#endif", r"\1", text, flags=re.S)  # demo builds 3D
    cmds = dict(re.findall(r'\.cmd\s*=\s*"(\w+)";.*?\.cmdHandlerFxn\s*=\s*(\w+);', text, re.S))
    rules = {}
    for cmd, fn in cmds.items():
        m = re.search(r"int32_t\s+%s\s*\(\s*int32_t\s+argc[^)]*\)\s*\{(.*?)\n\}" % fn, text, re.S)
        if not m:
            continue
        body = m.group(1)
        rej = [_cond(c) for c in re.findall(r"if\s*\(([^{;]*?argc[^{;]*?)\)\s*\{?\s*CLI_write\s*\(\s*\"Error", body)]
        used = [int(n) for n in re.findall(r"argv\[(\d+)", body)]
        loops = bool(re.search(r"argv\[\s*[a-zA-Z_]", body))  # argv[i]: reads a variable count
        rules[cmd] = (rej, None if loops else max(used, default=0))
    return rules

def check(rules, cfg):
    bad = []
    for i, line in enumerate(pathlib.Path(cfg).read_text().splitlines(), 1):
        line = line.split("%")[0].strip()
        if not line:
            continue
        tok = line.split(); cmd, argc = tok[0], len(tok)
        if cmd not in rules:
            if cmd not in ("sensorStart", "sensorStop", "version", "help"):
                bad.append((i, cmd, argc, "not a command in this firmware"))
            continue
        rej, top = rules[cmd]
        hit, unread = [], []
        for c in rej:
            try:
                if _eval(c, dict(MACROS, argc=argc)):
                    hit.append(c)
            except (NameError, ValueError, SyntaxError):  # run-time value (expectedArgc) or not a plain condition
                unread.append(c)
        if hit:
            bad.append((i, cmd, argc, "rejected: " + hit[0].strip()))
        elif unread and not rej[len(unread):]:
            bad.append((i, cmd, argc, "count depends on a run-time value, not checked: " + unread[0].strip()))
        elif top is not None and argc - 1 > top and cmd not in ("sensorStop", "sensorStart"):
            bad.append((i, cmd, argc, f"values after the {top}th are ignored"))
    return bad

def selftest():
    import tempfile
    d = pathlib.Path(tempfile.mkdtemp())
    (d / "a.c").write_text(
        'x.cmd = "fooCfg"; x.cmdHandlerFxn = Foo;\ny.cmd = "barCfg"; y.cmdHandlerFxn = Bar;\n'
        'int32_t Foo (int32_t argc, char* argv[])\n{\n    if (argc < 5)\n    {\n        CLI_write ("Error: x");\n        return -1;\n'
        '    }\n    a = argv[1]; a = argv[5];\n}\n'
        'int32_t Bar (int32_t argc, char* argv[])\n{\n    if (!((argc == 3) || (argc == 4)))\n    {\n'
        '        CLI_write ("Error: x");\n        return -1;\n    }\n    a = argv[3];\n}\n')
    (d / "t.cfg").write_text("fooCfg 1 2 3 4\nfooCfg 1 2 3 4 5 6 7\nbarCfg 1 2\nbarCfg 1\nbazCfg 0\n")
    bad = check(table(d), d / "t.cfg")
    assert [b[0] for b in bad] == [2, 4, 5], bad
    try:
        _eval("__import__('os')", {"argc": 1}); raise AssertionError("call accepted")
    except ValueError:
        pass
    print("selftest OK")

if __name__ == "__main__":
    if sys.argv[1:] == ["--selftest"]:
        selftest(); sys.exit(0)
    if len(sys.argv) < 3 or not pathlib.Path(sys.argv[1]).is_dir():
        print(__doc__.split("Usage:")[1].split("Exit")[0].strip()); sys.exit(2)
    rules = table(sys.argv[1]); rc = 0
    for cfg in sys.argv[2:]:
        bad = check(rules, cfg)
        hard = [b for b in bad if not b[3].startswith("count depends")]
        print(f"{cfg}: {'OK' if not hard else str(len(hard)) + ' problem(s)'}")
        for i, cmd, argc, why in bad:
            print(f"  line {i}: {cmd} ({argc - 1} values): {why}")
            rc |= not why.startswith("count depends")
    sys.exit(rc)
