#!/usr/bin/env python3
"""Configuration port of named DriverLib examples between the MSPM0 G3519 SDK and the MSPM33 SDK, either way.

Same method as crossport_mspm0_mspm33.py (X29): the app .c/.h files travel untouched, the example's own .syscfg
is retargeted (device header swapped, pin assignments unlocked so SysConfig picks pins on the new package, the
source chip's clock-tree lines dropped, every line SysConfig rejects commented out and listed), then built with
the target SDK's makefile. This file adds three things X29 lacks: the reverse direction (MSPM33 to MSPM0),
a named-example list, and a base project to start from when the target SDK has no twin by that name (the target's
`empty` project: its makefile, linker file and startup, with the source's app and configuration put in).

  crossport_pair.py m0_to_m33|m33_to_m0 <work dir> <results jsonl> <example> [<example> ...] [--base twin|empty]

One JSON line per example: src, dst, base, unlocked, dropped, syscfg_ok, ok, text, first_error, out.
"""
import glob, json, os, re, shutil, subprocess, sys, tempfile

SDK = {"m0": "/Applications/ti/mspm0_sdk_2_11_00_07", "m33": "/Applications/ti/mspm33_sdk_1_04_00_00"}
BOARD = {"m0": "LP_MSPM0G3519", "m33": "LP_MSPM33C321A"}
CGT = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
CLI = "/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh"
HEADER = {
    "m33": ['//@cliArgs --device "MSPM33C321A" --package "LQFP-100(PFA)" --part "Default"',
            '//@v2CliArgs --device "MSPM33C321A" --package "LQFP-100(PFA)"',
            '// @cliArgs --board /ti/boards/LP_MSPM33C321A --rtos nortos'],
    "m0": ['//@cliArgs --device "MSPM0G351X" --package "LQFP-100(PZ)" --part "Default"',
           '//@v2CliArgs --device "MSPM0G3519" --package "LQFP-100(PZ)"',
           '// @cliArgs --board /ti/boards/LP_MSPM0G3519 --rtos nortos'],
}
UNLOCK = re.compile(r"\.(pin|peripheral|[a-zA-Z]+Pin|[a-zA-Z]+Port)\.\$assign\s*=|\.assignedPort\s*=|\.assignedPin\s*=|\.\$assign\s*=|\.assignedPortSegment\s*=")
CLOCK = re.compile(r"^\s*SYSCTL\.(clockTreeEn|forceDefaultClkConfig|\w*PLL\w*|MCLK\w*|\w*Clk\w*|validateClkCfg)\b")
PROJCFG = ['const ProjectConfig = scripting.addModule("/ti/project_config/ProjectConfig");',
           'ProjectConfig.genLinker  = true;', 'ProjectConfig.genStartup = true;', 'ProjectConfig.genOpts    = true;', 'ProjectConfig.genLibs    = true;']
MAX_DROPS = 40


def prop_of(code):
    m = re.match(r"\s*[\w$]+\.([\w$]+(?:\[\d+\])?(?:\.[\w$]+)*)", code)
    if m:
        return m.group(1)
    m = re.match(r"\s*const\s+(\w+)\s*=\s*(\w+)\.addInstance", code)
    return ("instance:" + m.group(2)) if m else code.strip()[:40]


def generate(path, out, product):
    p = subprocess.run([CLI, "--compiler", "ticlang", "--product", product, "--output", out, path], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def fix(path, product):
    dropped, ok, last = [], False, ""
    for _ in range(MAX_DROPS):
        with tempfile.TemporaryDirectory() as out:
            rc, log = generate(path, out, product)
        nerr = re.search(r"(\d+) error\(s\)", log)
        if rc == 0 and (nerr is None or nerr.group(1) == "0") and not re.search(r"^\s*error:", log, re.M):
            ok = True
            break
        m = re.search(re.escape(path) + r":(\d+):\d+", log)
        lines = open(path).read().split("\n")
        if not m:
            mm = re.search(r"^\s*error: (\w+)\(/ti/[\w/.]+\) (\w+):", log, re.M)
            n = None
            if mm:
                inst, prop = mm.group(1), mm.group(2)
                var = next((re.match(r"\s*(\w+)\.\$name\s*=\s*\"" + inst + "\"", l).group(1) for l in lines if re.match(r"\s*(\w+)\.\$name\s*=\s*\"" + inst + "\"", l)), None)
                cands = [i for i, l in enumerate(lines) if re.search(r"\." + prop + r"\s*=", l) and not l.lstrip().startswith("//")]
                pref = [i for i in cands if var and lines[i].lstrip().startswith(var + ".")]
                n = (pref or cands or [None])[0]
                n = n + 1 if n is not None else None
            if n is None:
                last = next((l for l in log.splitlines() if re.match(r"\s*error:", l)), log.strip()[-200:])[:200]
                break
        else:
            n = int(m.group(1))
        if lines[n - 1].lstrip().startswith("// PORT-DROPPED:"):
            last = "same line failed twice"
            break
        msg = next((l for l in log.splitlines() if re.match(r"\s*(\w*Error|Error):", l)), "")[:160]
        code = lines[n - 1].strip()
        dropped.append({"line": n, "code": code[:160], "property": prop_of(code), "why": msg.strip()})
        lines[n - 1] = "// PORT-DROPPED: " + lines[n - 1]
        open(path, "w").write("\n".join(lines))
    else:
        last = f"gave up after {MAX_DROPS} drops"
    return ok, dropped, last


def reserve_block(pins):
    """A GPIO instance that holds pins as plain inputs so SysConfig does not hand them to a peripheral."""
    out = ['', 'const GPIO_RSV = scripting.addModule("/ti/driverlib/GPIO", {}, false);', 'const GPIO_RSV1 = GPIO_RSV.addInstance();',
           'GPIO_RSV1.$name = "GPIO_RESERVED";', f'GPIO_RSV1.associatedPins.create({len(pins)});']
    for i, pin in enumerate(pins):
        out += [f'GPIO_RSV1.associatedPins[{i}].$name = "RSV_{pin}";', f'GPIO_RSV1.associatedPins[{i}].direction = "INPUT";',
                f'GPIO_RSV1.associatedPins[{i}].pin.$assign = "{pin}";']
    return "\n".join(out) + "\n"


def port_syscfg(src, dst, to, reserve=()):
    lines = open(src).read().split("\n")
    out, unlocked = [], 0
    has_pc = any("project_config/ProjectConfig" in l for l in lines)
    for l in lines:
        if l.startswith("//@cliArgs") or l.startswith("//@v2CliArgs") or l.startswith("// @cliArgs"):
            continue
        if UNLOCK.search(l) and not l.lstrip().startswith("//"):
            unlocked += 1
            out.append("// PORT-UNLOCKED: " + l)
            continue
        if CLOCK.search(l) and not l.lstrip().startswith("//"):
            out.append("// PORT-DROPPED: " + l)
            continue
        out.append(l)
    body = "\n".join(out)
    i = body.find("\n */\n")
    hdr = "\n".join(HEADER[to]) + "\n"
    body = body[:i + 5] + hdr + body[i + 5:] if i > 0 else hdr + body
    if to == "m33" and not has_pc:
        body += "\n" + "\n".join(PROJCFG) + "\n"
    if reserve:
        body += reserve_block(reserve)
    open(dst, "w").write(body)
    return unlocked


def build(d, to):
    sdk = SDK[to]
    var = "MSPM33_SDK_INSTALL_DIR" if to == "m33" else "MSPM0_SDK_INSTALL_DIR"
    p = subprocess.run(["make", "-C", f"{d}/ticlang", f"{var}={sdk}", f"SYSCONFIG_TOOL={CLI}", f"TICLANG_ARMCOMPILER={CGT}", "GCC_ARMCOMPILER="], capture_output=True, text=True)
    open(f"{d}/build.log", "w").write(p.stdout + p.stderr)
    return p


def main():
    args = sys.argv[1:]
    base_mode = "twin"
    reserve = ()
    add = []
    while "--add" in args:
        i = args.index("--add")
        add.append(args[i + 1])
        del args[i:i + 2]
    if "--reserve" in args:
        i = args.index("--reserve")
        reserve = tuple(args[i + 1].split(","))
        del args[i:i + 2]
    if "--base" in args:
        i = args.index("--base")
        base_mode = args[i + 1]
        del args[i:i + 2]
    direction, W, OUT, names = args[0], args[1], args[2], args[3:]
    frm, to = ("m0", "m33") if direction == "m0_to_m33" else ("m33", "m0")
    cat = "driverlib"
    A = f"{SDK[frm]}/examples/nortos/{BOARD[frm]}/{cat}"
    B = f"{SDK[to]}/examples/nortos/{BOARD[to]}/{cat}"
    product = SDK[to] + "/.metadata/product.json"
    res = open(OUT, "a")
    for e in names:
        if not os.path.isdir(f"{A}/{e}"):
            print(e, "not in source SDK")
            continue
        base = e if (base_mode == "twin" and os.path.isdir(f"{B}/{e}")) else "empty"
        d = f"{W}/{direction}/{e}"
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copytree(f"{B}/{base}", d)
        # rename the base project's files to the example's name when the base is `empty`
        if base != e:
            for root, _, files in os.walk(d):
                for f in files:
                    if "empty" in f and not f.endswith((".md", ".html")):
                        os.rename(os.path.join(root, f), os.path.join(root, f.replace("empty", e)))
            for root, _, files in os.walk(d):
                for f in files:
                    if f in ("makefile",) or f.endswith(".projectspec"):
                        p = os.path.join(root, f)
                        t = open(p, errors="replace").read().replace("empty", e)
                        open(p, "w").write(t)
        for f in glob.glob(f"{d}/*.c") + glob.glob(f"{d}/*.h") + glob.glob(f"{d}/*.syscfg"):
            os.remove(f)
        for f in glob.glob(f"{A}/{e}/*.c") + glob.glob(f"{A}/{e}/*.h"):
            if os.path.basename(f).startswith("ti_msp_dl_config"):
                continue
            shutil.copy(f, d)
        if to == "m33":
            os.makedirs(f"{d}/ti/driverlib/m0p", exist_ok=True)
            open(f"{d}/ti/driverlib/m0p/dl_core.h", "w").write("/* shim: MSPM0 core header name on the MSPM33 */\n#include <ti/driverlib/m33/dl_core.h>\n")
        else:
            os.makedirs(f"{d}/ti/driverlib/m33", exist_ok=True)
            open(f"{d}/ti/driverlib/m33/dl_core.h", "w").write("/* shim: MSPM33 core header name on the MSPM0 */\n#include <ti/driverlib/m0p/dl_core.h>\n")
        row = dict(src=f"{BOARD[frm]}:{cat}/{e}", dst=f"{BOARD[to]}:{cat}/{e}", base=base, mode="config-port")
        srcs = glob.glob(f"{A}/{e}/*.syscfg")
        if not srcs:
            row.update(ok=False, first_error="source example has no .syscfg")
            res.write(json.dumps(row) + "\n")
            continue
        dst = f"{d}/{e}.syscfg"
        row["unlocked"] = port_syscfg(srcs[0], dst, to, reserve)
        if add:
            open(dst, "a").write("\n// PORT-ADDED (a configuration line the source example did not need):\n" + "\n".join(add) + "\n")
            row["added"] = add
        ok, dropped, last = fix(dst, product)
        dropped += [dict(line=i + 1, code=l[16:176], property=prop_of(l[16:]), why="source clock-tree setting, not applicable") for i, l in enumerate(open(dst).read().split("\n")) if l.startswith("// PORT-DROPPED: SYSCTL.")]
        row["dropped"] = dropped
        row["syscfg_ok"] = ok
        if not ok:
            row.update(ok=False, first_error="syscfg: " + last)
            res.write(json.dumps(row) + "\n")
            res.flush()
            print(e, "SYSCFG FAIL", last[:100])
            continue
        p = build(d, to)
        outs = glob.glob(f"{d}/ticlang/*.out")
        ok = p.returncode == 0 and bool(outs)
        err = next((l for l in (p.stdout + p.stderr).splitlines() if "error:" in l or "undefined symbol" in l), "")
        err = re.sub(r"^.*error: ", "", err)[:200]
        size = None
        if ok:
            s = subprocess.run([f"{CGT}/bin/tiarmsize", outs[0]], capture_output=True, text=True).stdout.strip().splitlines()[-1].split()
            size = int(s[0])
            row["out"] = outs[0]
        row.update(ok=ok, text=size, first_error=err)
        res.write(json.dumps(row) + "\n")
        res.flush()
        print(e, "ok" if ok else "FAIL", size or "", f"base={base} unlocked={row['unlocked']} dropped={len(dropped)}", err[:90])
    res.close()


if __name__ == "__main__":
    main()
