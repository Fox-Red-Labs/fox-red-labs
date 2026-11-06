#!/usr/bin/env python3
"""Generate the SysConfig output the SDKs' IAR projects expect, then push it to the Mac mini.

Runs on the MacBook because SysConfig for macOS is installed there and the container is Linux.
Output is a few KB per project, written to a scratch folder and rsynced to $FRL_TI_ROOT/iar-gen/<sdk> on the build host
(the shim copies it into each project at build time); nothing large is kept on this Mac.

  sysconfig_gen.py --sdk mspm0_sdk_2_11_00_07 --board LP_MSPM0L1306 [--filter empty] [--dry] [--no-push]

For each .ewp it reads SysConfig_Generated_Files.ipcf next to it, finds the folder the project expects
the generated files in (the project folder),
and runs `sysconfig_cli.sh --compiler iar` on the example's .syscfg into the project folder, the same call
the SDK's build step makes (-o $PROJ_DIR$). Inside the container a shim (shim/sysconfig_cli.sh) stands in for SysConfig.
"""
import argparse, glob, os, re, subprocess, sys, tempfile

TI = "/Applications/ti"
SYSCFG_DEFAULT = TI + "/sysconfig_1.28.0/sysconfig_cli.sh"
MINI = os.environ.get("IAR_HOST") or sys.exit("set IAR_HOST to the IAR build machine (ssh host name)"); MINI_ROOT = os.environ.get("FRL_TI_ROOT", "/opt/ti")


def plan(sdk, board, filt):
    root = f"{TI}/{sdk}"
    for ewp in sorted(glob.glob(root + "/**/*.ewp", recursive=True)):
        rel = os.path.relpath(ewp, root)
        if (board and board not in rel) or (filt and filt not in rel):
            continue
        d = os.path.dirname(ewp)
        ipcf = os.path.join(d, "SysConfig_Generated_Files.ipcf")
        if not os.path.exists(ipcf):
            continue
        paths = re.findall(r"<path>([^<]+)</path>", open(ipcf).read())
        if not paths:
            continue
        outdir = d   # the SDK's own build step runs: sysconfig_cli -o $PROJ_DIR$ -s product.json --compiler iar <file>.syscfg
        cands = glob.glob(os.path.join(d, "..", "*.syscfg")) + glob.glob(os.path.join(d, "*.syscfg")) + glob.glob(os.path.join(d, "..", "..", "*.syscfg"))
        if cands:
            yield rel, os.path.normpath(cands[0]), os.path.normpath(outdir)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", required=True); ap.add_argument("--board"); ap.add_argument("--filter")
    ap.add_argument("--sysconfig", default=SYSCFG_DEFAULT); ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--dry", action="store_true"); ap.add_argument("--no-push", action="store_true")
    a = ap.parse_args()
    root = f"{TI}/{a.sdk}"
    stage = tempfile.mkdtemp(prefix="iar-gen-")
    n = fail = 0
    for rel, syscfg, outdir in plan(a.sdk, a.board, a.filter):
        if a.limit and n >= a.limit:
            break
        n += 1
        stage_out = os.path.join(stage, a.sdk, os.path.relpath(outdir, root))
        cmd = [a.sysconfig, "--compiler", "iar", "--product", f"{root}/.metadata/product.json",
               "--output", stage_out, syscfg]
        if a.dry:
            print(" ".join(cmd)); continue
        os.makedirs(stage_out, exist_ok=True)
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            fail += 1; print("FAIL", rel, r.stderr.strip()[-200:], file=sys.stderr)
    print(f"{n} projects, {fail} failed, staged in {stage}", file=sys.stderr)
    if not a.dry and not a.no_push:
        subprocess.check_call(["rsync", "-a", stage + f"/{a.sdk}/", f"{MINI}:{MINI_ROOT}/iar-gen/{a.sdk}/"])
        print("pushed to", MINI)


if __name__ == "__main__":
    main()
