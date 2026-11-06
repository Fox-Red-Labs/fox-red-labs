#!/usr/bin/env python3
"""Configuration port: an MSPM0 DriverLib example carried to the LP-MSPM33C321A with its own SysConfig file.

X28 showed the DriverLib API is shared and what breaks is the generated configuration names (the LED is
USER_LED_1 in the MSPM0 example and PASS_LED in TI's MSPM33 twin). So instead of dropping the MSPM0 app into
the MSPM33 project (X28), this takes the whole MSPM0 example, app and .syscfg, and retargets the .syscfg to
the MSPM33 the way port_mspm0.py does inside MSPM0 ("way 3"): swap the device header, unlock every hard pin
and peripheral assignment so SysConfig picks its own on the new package, add the MSPM33 project-config block,
then run SysConfig and comment out each line it rejects (recorded, nothing dropped silently), then build.
The app's names survive because they come from its own configuration. Rung 3 of the ladder: no app edits.

  crossport_mspm0_mspm33.py <work dir> <results jsonl> [FROM board] [category ...]
Writes one JSON line per example: ok, dropped (lines SysConfig rejected), unlocked (pin lines removed),
text size, first error; and keeps <work>/port/<cat>/<ex>/ticlang/<ex>.out for the run on the board.
"""
import json, os, re, shutil, subprocess, sys, glob, tempfile
M0 = "/Applications/ti/mspm0_sdk_2_11_00_07"; M33 = "/Applications/ti/mspm33_sdk_1_04_00_00"
CGT = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
CLI = "/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh"; PRODUCT = M33 + "/.metadata/product.json"
HEADER = ['//@cliArgs --device "MSPM33C321A" --package "LQFP-100(PFA)" --part "Default"',
          '//@v2CliArgs --device "MSPM33C321A" --package "LQFP-100(PFA)"',
          '// @cliArgs --board /ti/boards/LP_MSPM33C321A --rtos nortos']
UNLOCK = re.compile(r"\.(pin|peripheral|[a-zA-Z]+Pin|[a-zA-Z]+Port)\.\$assign\s*=|\.assignedPort\s*=|\.assignedPin\s*=|\.\$assign\s*=|\.assignedPortSegment\s*=")
PROJCFG = ['const ProjectConfig = scripting.addModule("/ti/project_config/ProjectConfig");',
           'ProjectConfig.genLinker  = true;', 'ProjectConfig.genStartup = true;', 'ProjectConfig.genOpts    = true;', 'ProjectConfig.genLibs    = true;']
MAX_DROPS = 40
# MSPM0 clock-tree settings do not apply to the MSPM33 clock tree (its SysConfig rejects the PLL dividers);
# dropped up front and listed. forceDefaultClkConfig is benign (port_mspm0.py); clockTreeEn and the PLL lines are a lost setting.
CLOCK = re.compile(r"^\s*SYSCTL\.(clockTreeEn|forceDefaultClkConfig|\w*PLL\w*|MCLK\w*|\w*Clk\w*|validateClkCfg)\b")

def prop_of(code):
    m = re.match(r"\s*[\w$]+\.([\w$]+(?:\[\d+\])?(?:\.[\w$]+)*)", code)
    if m: return m.group(1)
    m = re.match(r"\s*const\s+(\w+)\s*=\s*(\w+)\.addInstance", code)
    return ("instance:" + m.group(2)) if m else code.strip()[:40]

def generate(path, out):
    p = subprocess.run([CLI, "--compiler", "ticlang", "--product", PRODUCT, "--output", out, path], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr

def fix(path):
    dropped, ok, last = [], False, ""
    for _ in range(MAX_DROPS):
        with tempfile.TemporaryDirectory() as out:
            rc, log = generate(path, out)
        nerr = re.search(r"(\d+) error\(s\)", log)
        if rc == 0 and (nerr is None or nerr.group(1) == "0") and not re.search(r"^\s*error:", log, re.M):
            ok = True; break
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
                last = next((l for l in log.splitlines() if re.match(r"\s*error:", l)), log.strip()[-200:])[:200]; break
        else:
            n = int(m.group(1))
        if lines[n - 1].lstrip().startswith("// PORT-DROPPED:"):
            last = "same line failed twice"; break
        msg = next((l for l in log.splitlines() if re.match(r"\s*(\w*Error|Error):", l)), "")[:160]
        code = lines[n - 1].strip()
        dropped.append({"line": n, "code": code[:160], "property": prop_of(code), "why": msg.strip()})
        lines[n - 1] = "// PORT-DROPPED: " + lines[n - 1]
        open(path, "w").write("\n".join(lines))
    else:
        last = f"gave up after {MAX_DROPS} drops"
    return ok, dropped, last

def port_syscfg(src, dst):
    lines = open(src).read().split("\n")
    out, unlocked, in_head = [], 0, True
    has_pc = any("project_config/ProjectConfig" in l for l in lines)
    for l in lines:
        if l.startswith("//@cliArgs") or l.startswith("//@v2CliArgs") or l.startswith("// @cliArgs"):
            continue
        if UNLOCK.search(l) and not l.lstrip().startswith("//"):
            unlocked += 1; out.append("// PORT-UNLOCKED: " + l); continue
        if CLOCK.search(l) and not l.lstrip().startswith("//"):
            out.append("// PORT-DROPPED: " + l); continue
        out.append(l)
    # header first, then the body; ProjectConfig block at the end if the MSPM0 file has none
    body = "\n".join(out)
    i = body.find("\n */\n")
    body = body[:i + 5] + "\n".join(HEADER) + "\n" + body[i + 5:] if i > 0 else "\n".join(HEADER) + "\n" + body
    if not has_pc:
        body += "\n" + "\n".join(PROJCFG) + "\n"
    open(dst, "w").write(body)
    return unlocked

def main():
    W, OUT = sys.argv[1], sys.argv[2]; FROM = sys.argv[3] if len(sys.argv) > 3 else "LP_MSPM0G3519"
    cats = sys.argv[4:] or ["driverlib"]
    done = set()
    if os.path.exists(OUT):                      # resume: skip examples already recorded
        done = {json.loads(l)["src"] for l in open(OUT) if l.strip()}
    res = open(OUT, "a")
    for cat in cats:
        A = f"{M0}/examples/nortos/{FROM}/{cat}"; B = f"{M33}/examples/nortos/LP_MSPM33C321A/{cat}"
        for e in sorted(set(os.listdir(A)) & set(os.listdir(B))):
            if f"{FROM}:{cat}/{e}" in done: continue
            d = f"{W}/port/{cat}/{e}"; shutil.rmtree(d, ignore_errors=True); os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copytree(f"{B}/{e}", d)
            if not os.path.isdir(f"{d}/ticlang"): continue
            # Both SDKs ship a pre-generated copy of ti_msp_dl_config.c/.h beside the app; the makefile's -I.. puts that copy
            # ahead of SysConfig's fresh output, so the stale MSPM0 header would win. Only the app files travel.
            for f in glob.glob(f"{A}/{e}/*.c") + glob.glob(f"{A}/{e}/*.h"):
                if os.path.basename(f).startswith("ti_msp_dl_config"): continue
                shutil.copy(f, d)
            for f in glob.glob(f"{d}/ti_msp_dl_config.*"): os.remove(f)
            hand = False
            os.makedirs(f"{d}/ti/driverlib/m0p", exist_ok=True)         # the shim: MSPM0's core header name on the M33
            open(f"{d}/ti/driverlib/m0p/dl_core.h", "w").write("/* shim: MSPM0 core header name on the MSPM33 */\n#include <ti/driverlib/m33/dl_core.h>\n")
            srcs = glob.glob(f"{A}/{e}/*.syscfg"); row = dict(src=f"{FROM}:{cat}/{e}", dst=f"LP_MSPM33C321A:{cat}/{e}", mode="config-port", hand_config=hand)
            if not srcs:
                row.update(ok=False, first_error="MSPM0 example has no .syscfg"); res.write(json.dumps(row) + "\n"); continue
            for old in glob.glob(f"{d}/*.syscfg"): os.remove(old)
            dst = f"{d}/{e}.syscfg"; row["unlocked"] = port_syscfg(srcs[0], dst)
            ok, dropped, last = fix(dst)
            dropped += [dict(line=i + 1, code=l[16:176], property=prop_of(l[16:]), why="MSPM0 clock-tree setting, not applicable") for i, l in enumerate(open(dst).read().split("\n")) if l.startswith("// PORT-DROPPED: SYSCTL.")]
            row["dropped"] = dropped; row["syscfg_ok"] = ok
            if not ok:
                row.update(ok=False, first_error="syscfg: " + last); res.write(json.dumps(row) + "\n"); res.flush(); print(e, "SYSCFG FAIL", last[:100]); continue
            log = f"{d}/build.log"
            p = subprocess.run(["make", "-C", f"{d}/ticlang", f"MSPM33_SDK_INSTALL_DIR={M33}", f"SYSCONFIG_TOOL={CLI}", f"TICLANG_ARMCOMPILER={CGT}", "GCC_ARMCOMPILER="], capture_output=True, text=True)
            open(log, "w").write(p.stdout + p.stderr)
            outs = glob.glob(f"{d}/ticlang/*.out"); ok = p.returncode == 0 and bool(outs)
            err = next((l for l in (p.stdout + p.stderr).splitlines() if "error:" in l or "undefined symbol" in l), "")
            err = re.sub(r"^.*error: ", "", err)[:200]
            size = None
            if ok:
                s = subprocess.run([f"{CGT}/bin/tiarmsize", outs[0]], capture_output=True, text=True).stdout.strip().splitlines()[-1].split()
                size = int(s[0]); row["out"] = outs[0]
            row.update(ok=ok, text=size, first_error=err, date="2026-10-10")
            res.write(json.dumps(row) + "\n"); res.flush()
            print(e, "ok" if ok else "FAIL", size or "", f"unlocked={row['unlocked']} dropped={len(dropped)}", err[:90])
            for f in glob.glob(f"{d}/ticlang/*.obj"): os.remove(f)
    res.close()

if __name__ == "__main__":
    main()
