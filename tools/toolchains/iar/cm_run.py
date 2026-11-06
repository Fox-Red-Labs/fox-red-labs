#!/usr/bin/env python3
"""Flash one IAR CoreMark image (coremark.hex beside coremark.map) to the LP-MSPM33C321A, wait for CoreMark to
finish, read the report out of RAM and the flash-wait-state registers, print one JSON line.
  python3 tools/frl/lock.py exec --serial M3010001 -- python3 tools/toolchains/iar/cm_run.py <dir with coremark.hex> [--serial S] [--limit SEC] [--no-flash]
Symbol addresses come from the IAR map ("cm_log  0x3000'0010  0x800  Data ..."). Only the named probe is touched."""
import argparse, json, os, re, subprocess, sys, tempfile, time
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
DSL = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"

def rd(ccxml, addr, n):
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        r = subprocess.run([DSL, "memory", "-c", ccxml, "-r", "0x%x,%d" % (addr, n), "-s", "8", "-o", f.name], capture_output=True, text=True, timeout=120)
        if r.returncode: raise SystemExit("read failed: " + r.stdout[-200:])
        return open(f.name, "rb").read()

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("dir"); ap.add_argument("--serial", default=os.environ.get("PROBE"), required=not os.environ.get("PROBE"))
    ap.add_argument("--limit", type=float, default=60); ap.add_argument("--no-flash", action="store_true")
    a = ap.parse_args()
    ccxml = os.path.join(REPO, "mspm33c321a", "MSPM33C321A_%s.ccxml" % a.serial)
    mp = open(os.path.join(a.dir, "coremark.map"), errors="replace").read()
    sym = lambda n: int(re.search(r"^%s\s+0x([0-9a-f']+)\s" % n, mp, re.M).group(1).replace("'", ""), 16)
    t0 = time.time()
    if not a.no_flash:
        r = subprocess.run([DSL, "flash", "-c", ccxml, "-e", "-f", "-v", os.path.join(a.dir, "coremark.hex")], capture_output=True, text=True, timeout=300)
        if r.returncode: print(r.stdout[-500:], r.stderr[-300:]); sys.exit(1)
    tf = time.time() - t0
    cr = sym("cm_result"); res = b""
    while time.time() - t0 < tf + a.limit:
        time.sleep(3); res = rd(ccxml, cr, 16)
        if int.from_bytes(res[:4], "little") == 0xC0DE0001: break
    done = int.from_bytes(res[:4], "little") == 0xC0DE0001
    log = rd(ccxml, sym("cm_log"), 400).split(b"\0")[0].decode("latin1")
    regs = rd(ccxml, 0x40029000, 16); ctl = rd(ccxml, 0x4002900C, 4)
    g = lambda k: (re.search(k + r"\s*:\s*(\S+)", log) or [None, None])[1]
    out = dict(done=done, flash_s=round(tf, 1), cpu_hz=int.from_bytes(res[4:8], "little"),
               ticks=int.from_bytes(res[8:16], "little"), iterations=g("Iterations"), crcfinal=g("crcfinal"),
               frdcntl="0x%08x" % int.from_bytes(regs[:4], "little"), frd_intf_ctrl="0x%08x" % int.from_bytes(ctl, "little"))
    if done and out["ticks"]:
        out["coremark"] = round(int(out["iterations"]) * out["cpu_hz"] / out["ticks"], 2)
    print(json.dumps(out))

main()
