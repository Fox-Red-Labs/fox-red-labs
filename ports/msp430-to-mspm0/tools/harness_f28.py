#!/usr/bin/env python3
"""Run one scripted test on the LAUNCHXL-F2800137 and keep a timestamped serial log. Every probe action goes through the probe lock.

  ~/.venvs/ble/bin/python harness_f28.py --log results/f28-x.log flash:path/to.out wait:6 reset wait:4

Loader: CCStudio scripting by default (erases the entire flash); F28_LOADER=dslite loads with only the sectors the image uses erased.
Steps: flash:<image>  reset  wait:<s>  readlog:<file>. The serial port (the probe's first one, 115200) is opened before the first step and
read by a thread; each received line is stored with seconds since open. Probe 'CL371081' only.
"""
import argparse, os, subprocess, sys, threading, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SERIAL, TTY = "CL371081", "/dev/cu.usbmodemCL3710811"
RUN = "/Applications/ti/ccs2101/ccs/scripting/run.sh"
JS = str(Path(__file__).resolve().parent / "f28probe.js")


DSLITE = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
CCXML = str(ROOT / "c2000-launchpad/tools/F2800137_CL371081.ccxml")


def dslite_flash(img):
    """Load with DSLite and 'Necessary Sectors Only' (the scripting loader erases the entire flash, log sector included)."""
    r = subprocess.run([sys.executable, str(ROOT / "tools/frl/lock.py"), "exec", "--serial", SERIAL, "--op", "msp430-port-test", "--", DSLITE, "flash",
                        "-c", CCXML, "-f", "-v", "-u", "-r", "1", "-t", "120", "-s", "FlashEraseSelection=1", img], capture_output=True, text=True, timeout=300)
    return r.returncode, r.stdout + r.stderr


def probe(args):
    r = subprocess.run([sys.executable, str(ROOT / "tools/frl/lock.py"), "exec", "--serial", SERIAL, "--op", "msp430-port-test", "--", RUN, JS] + args,
                       capture_output=True, text=True, timeout=300, env=dict(os.environ, FRL_ROOT=str(ROOT)))
    return r.returncode, r.stdout + r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--log", required=True)
    ap.add_argument("steps", nargs="+")
    a = ap.parse_args()
    import serial
    ser = serial.Serial(TTY, a.baud, timeout=0.1)
    t0 = time.time(); lines = []; buf = bytearray(); stop = threading.Event()

    def pump():
        while not stop.is_set():
            d = ser.read(256)
            if d:
                now = time.time() - t0
                buf.extend(d)
                while b"\n" in buf:
                    i = buf.index(b"\n"); ln = bytes(buf[:i]).decode("utf-8", "replace").rstrip("\r"); del buf[:i + 1]
                    lines.append(f"[{now:8.3f}] {ln}")
    th = threading.Thread(target=pump, daemon=True); th.start()
    notes = []
    for s in a.steps:
        k, _, v = s.partition(":")
        now = time.time() - t0
        if k == "wait":
            time.sleep(float(v))
        elif k == "flash":
            img = str(Path(v).expanduser())
            if os.environ.get("F28_LOADER") == "dslite":
                rc, out = dslite_flash(img)
                ok = rc == 0
            else:
                rc, out = probe(["flash", img])
                ok = rc == 0 and "loaded" in out
            notes.append(f"[{now:8.3f}] # flash {'ok' if ok else 'FAILED'} {Path(img).name} ({time.time()-t0-now:.1f} s)")
            if not ok:
                notes.append(out[-800:])
        elif k == "reset":
            rc, out = probe(["reset"])
            notes.append(f"[{now:8.3f}] # reset rc={rc} ({time.time()-t0-now:.1f} s)")
        elif k == "readlog":
            rc, out = probe(["readlog", str(Path(v).expanduser())])
            notes.append(f"[{now:8.3f}] # readlog rc={rc} -> {v}")
        else:
            raise SystemExit("unknown step " + s)
    time.sleep(0.3); stop.set(); th.join(1); ser.close()
    if buf:
        notes.append(f"[{time.time()-t0:8.3f}] # unterminated bytes: {bytes(buf)!r}")
    allv = sorted(lines + notes)
    Path(a.log).parent.mkdir(parents=True, exist_ok=True)
    Path(a.log).write_text("\n".join(allv) + "\n")
    print("\n".join(allv))


if __name__ == "__main__":
    main()
