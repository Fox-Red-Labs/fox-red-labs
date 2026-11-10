#!/usr/bin/env python3
"""Run one scripted test on a bench board and keep a timestamped serial log. Every probe action goes through the probe lock.

  ~/.venvs/labgrid/bin/python harness.py --board l1306 --log results/x.log --baud 115200 \
      flash:path/to.out  wait:6  reset  wait:4  send:hello  wait:2

Steps: flash:<image>  reset  wait:<s>  send:<text>  sendslow:<text> (one byte per 0.3 s)  sendraw:<hex>.
The serial port is opened before the first step and read by a thread; each received line is stored with seconds since open.
m33: DSLite plus xds110reset (pulses the board's reset line) under tools/frl/lock.py exec; M33_PLAIN_E=1 uses a plain -e (erase all main) on flash,
otherwise only the image's sectors are erased (DSLite FlashEraseSectors settings).
l1306: OpenOCD through mspm0l1306/tools/ocd.sh (takes the lock). am13: DSLite under tools/frl/lock.py exec.
"""
import argparse, glob, os, subprocess, sys, threading, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BOARDS = {
    "l1306": {"serial": "ML130001", "tty": "/dev/cu.usbmodemML1300011"},
    "am13": {"serial": "AL130001", "tty": "/dev/cu.usbmodemAL1300011"},
    "m33": {"serial": "M3010001", "tty": "/dev/cu.usbmodemM30100011"},
}
XDS110RESET = "/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset"
M33_CCXML = str(ROOT / "mspm33c321a/MSPM33C321A_M3010001.ccxml")
DSLITE = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
# Clears the debug-power requests OpenOCD leaves set; while they are set the chip does not enter STOP or STANDBY.
RELEASE_DAP = "mspm0x.dap dpreg 0x04 0x00000000" if not os.environ.get("NO_RELEASE") else "echo keep-dap"
CCXML = str(ROOT / "am13e230/AM13E23019_AL130001.ccxml")


def run_probe(board, argv_or_cmds):
    b = BOARDS[board]
    if board == "l1306":
        env = dict(os.environ, PROBE=b["serial"])
        r = subprocess.run([str(ROOT / "mspm0l1306/tools/ocd.sh")] + argv_or_cmds, env=env, capture_output=True, text=True, timeout=180)
        return r.returncode, r.stdout + r.stderr
    r = subprocess.run([sys.executable, str(ROOT / "tools/frl/lock.py"), "exec", "--serial", b["serial"], "--op", "msp430-port-test", "--"] + argv_or_cmds,
                       capture_output=True, text=True, timeout=240)
    return r.returncode, r.stdout + r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--board", required=True, choices=BOARDS)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--log", required=True)
    ap.add_argument("steps", nargs="+")
    a = ap.parse_args()
    import serial
    ser = serial.Serial(BOARDS[a.board]["tty"], a.baud, timeout=0.1)
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
        elif k == "send":
            v = v.encode().decode('unicode_escape'); ser.write(v.encode()); notes.append(f"[{now:8.3f}] # sent {v!r}")
        elif k == "sendslow":
            v = v.encode().decode('unicode_escape')
            for ch in v:
                ser.write(ch.encode()); time.sleep(0.3)
            notes.append(f"[{now:8.3f}] # sent slowly {v!r}")
        elif k == "sendraw":
            ser.write(bytes.fromhex(v)); notes.append(f"[{now:8.3f}] # sent raw {v}")
        elif k == "flash":
            img = str(Path(v).expanduser())
            if a.board == "m33":
                st = []
                if not os.environ.get("M33_PLAIN_E"):
                    lo, hi = os.environ.get("M33_ERASE_RANGE", "0,65535").split(",")
                    st = ["-s", "FlashEraseSelection=Erase MAIN memory sectors by range (specify below)", "-s", f"FlashEraseSectorsStartAddress={lo}", "-s", f"FlashEraseSectorsEndAddress={hi}"]
                rc, out = run_probe("m33", [DSLITE, "flash", "-c", M33_CCXML, "-e", "-f", "-v", "-u"] + st + [img])
                ok = rc == 0
            elif a.board == "l1306":
                rc, out = run_probe("l1306", ["reset halt", f"program {{{img}}} verify", "reset run", "sleep 100", RELEASE_DAP])
                ok = "Verified OK" in out
            else:
                rc, out = run_probe("am13", [DSLITE, "flash", "-c", CCXML, "-f", "-v", "-r", "1", "-u", "-t", "60", img] + (["-e"] if os.environ.get("AM13_ERASE") else []))
                ok = rc == 0 or "Program verification successful" in out
            notes.append(f"[{now:8.3f}] # flash {'ok' if ok else 'FAILED'} {Path(img).name} ({time.time()-t0-now:.1f} s)")
            if not ok:
                notes.append(out[-600:])
        elif k == "reset":
            if a.board == "m33":
                rc, out = run_probe("m33", [XDS110RESET, "-s", BOARDS["m33"]["serial"], "-a", "toggle", "-d", "200"])
            elif a.board == "l1306":
                rc, out = run_probe("l1306", ["reset run", "sleep 100", RELEASE_DAP])
            else:
                # DSLite has no reset command; TI's OpenOCD (frl's AM13 route) does a reset run.
                ocd = os.path.expanduser("~/ti/ti-openocd")
                rc, out = run_probe("am13", [ocd + "/bin/openocd", "-s", ocd + "/share/openocd/scripts", "-f", "interface/xds110.cfg",
                                             "-c", "adapter serial " + BOARDS["am13"]["serial"], "-f", "target/ti_am13e230x.cfg",
                                             "-c", "init; reset run; sleep 100; shutdown"])
            notes.append(f"[{now:8.3f}] # reset rc={rc} ({time.time()-t0-now:.1f} s) " + out.strip().splitlines()[-1][:120] if out.strip() else f"[{now:8.3f}] # reset rc={rc}")
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
