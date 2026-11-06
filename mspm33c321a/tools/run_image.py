#!/usr/bin/env python3
"""Flash one image to the LP-MSPM33C321A with DSLite, wait, then read RAM symbols back over the probe.
   run_image.py <image.out> <wait-seconds> <symbol:bytes> [<symbol:bytes> ...] [--serial S] [--no-flash]
Symbol addresses come from the image's linker map (<image>.map beside it, or <name>.map in the same folder).
Prints each symbol as hex; use --text to print a symbol as text. Only the given probe serial is touched."""
import argparse, glob, os, re, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
DSL = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image"); ap.add_argument("wait", type=float); ap.add_argument("syms", nargs="*")
    ap.add_argument("--serial", default=os.environ.get("PROBE"), required=not os.environ.get("PROBE")); ap.add_argument("--no-flash", action="store_true")
    ap.add_argument("--text", action="append", default=[])
    ap.add_argument("--until", help="sym=0xMAGIC: poll the symbol's first word every 3 s until it equals MAGIC (wait is the limit)")
    a = ap.parse_args()
    ccxml = os.path.join(HERE, "..", "MSPM33C321A_%s.ccxml" % a.serial)
    d = os.path.dirname(os.path.abspath(a.image))
    maps = glob.glob(os.path.join(d, "*.map")); mp = open(maps[0]).read() if maps else ""
    t0 = time.time()
    if not a.no_flash:
        r = subprocess.run([DSL, "flash", "-c", ccxml, "-e", "-f", "-v", a.image], capture_output=True, text=True)
        if r.returncode: print(r.stdout[-600:], r.stderr[-600:]); sys.exit(1)
    t1 = time.time(); print("flash %.1f s" % (t1 - t0), file=sys.stderr)
    if a.until:
        us, uv = a.until.split("="); uv = int(uv, 16)
        m = re.search(r"^\s*([0-9a-f]{8})\s+%s\s*$" % re.escape(us), mp, re.M); ua = int(m.group(1), 16)
        t_end = time.time() + a.wait
        while time.time() < t_end:
            time.sleep(3)
            with tempfile.NamedTemporaryFile(suffix=".bin") as f:
                subprocess.run([DSL, "memory", "-c", ccxml, "-r", "0x%x,4" % ua, "-s", "8", "-o", f.name], capture_output=True, text=True)
                if int.from_bytes(open(f.name, "rb").read(4), "little") == uv: break
        print("done after %.1f s from flash end" % (time.time() - t1), file=sys.stderr)
    else:
        time.sleep(a.wait)
    for s in a.syms:
        name, n = s.split(":"); n = int(n)
        if name.startswith("0x"):
            addr = int(name, 16)
        else:
            m = re.search(r"^\s*([0-9a-f]{8})\s+%s\s*$" % re.escape(name), mp, re.M)
            if not m: print(name, "not in map"); continue
            addr = int(m.group(1), 16)
        with tempfile.NamedTemporaryFile(suffix=".bin") as f:
            r = subprocess.run([DSL, "memory", "-c", ccxml, "-r", "0x%x,%d" % (addr, n), "-s", "8", "-o", f.name], capture_output=True, text=True)
            if r.returncode: print(r.stdout[-300:]); sys.exit(1)
            b = open(f.name, "rb").read()
        if name in a.text: print("%s @0x%x:\n%s" % (name, addr, b.split(b"\0")[0].decode("latin1")))
        else: print("%s @0x%x: %s" % (name, addr, b.hex()))
main()
