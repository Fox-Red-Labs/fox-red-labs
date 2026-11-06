#!/usr/bin/env python3
"""Quiet-hours run (and, with --sound, the audible run) of the BOOSTXL-AUDIO algorithms image on the LP-AM13E230: nothing reaches the speaker.

  ~/.venvs/labgrid/bin/python tools/am13_algos_quiet.py --image <audio.out> --out <folder> [--restore <hex>]

Holds the probe lock for the whole run. Flashes the algos image, sets volume 0 and leaves the amplifier line floating
(the TPA301 stays in shutdown), then runs: b (cycle counts on a synthetic frame), t x5 and s x5 on the quiet room with
the microphone on, d x4 with the amplifier shut down (expects no digit), then microphone off. With --restore it flashes
the given image back and reads the first 64 bytes. Without it the algos image stays running, silent and awake."""
import argparse, json, subprocess, sys, time
from pathlib import Path

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO / "tools" / "frl"))
from lock import ProbeLock  # noqa: E402

DSLITE = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
CCXML = REPO / "am13e230" / "AM13E23019_AL130001.ccxml"
PORT = "/dev/cu.usbmodemAL1300011"


def sh(argv, timeout=300):
    p = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True); ap.add_argument("--out", required=True); ap.add_argument("--restore")
    ap.add_argument("--sound", action="store_true", help="after the quiet steps, play tones and DTMF at volume 4 with the amplifier on")
    a = ap.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    rep = {"started": time.strftime("%Y-%m-%dT%H:%M:%S")}
    import serial
    with ProbeLock("AL130001", "audio-am13-quiet", "algos quiet run") as lk:
        lk.write_owner("writing")
        t0 = time.time()
        code, o = sh([DSLITE, "flash", "-c", str(CCXML), "-f", "-v", "-r", "1", "-u", "-t", "60", a.image], 240)
        rep["flash_s"] = round(time.time() - t0, 1)
        rep["flash_ok"] = code == 0 or "Program verification successful" in o
        rep["flash_tail"] = o.strip().splitlines()[-3:]
        lk.write_owner("holding")
        log = ""
        if rep["flash_ok"]:
            time.sleep(1.5)
            s = serial.Serial(PORT, 115200, timeout=0.1)

            def pump(secs):
                nonlocal log
                end = time.time() + secs
                while time.time() < end:
                    d = s.read(4096)
                    if d:
                        log += d.decode("latin1")

            def cmd(keys, wait):
                for k in keys:
                    s.write(k.encode()); pump(0.12)
                pump(wait)
            pump(0.5); cmd("p", 0.5)
            cmd("0z" + "v" * 16 + "M", 0.5)   # silence, amp floating (shutdown), volume 0, microphone off
            cmd("b", 3.0)
            cmd("m", 1.0)                       # microphone power on, settle
            for _ in range(5): cmd("t", 0.6)
            for _ in range(5): cmd("s", 0.4)
            for _ in range(4): cmd("d", 1.2)
            if a.sound:
                cmd("a" + "V" * 4, 0.3)          # amplifier line low (on), volume 4 (unclipped in E237)
                for key, n in (("1", 3), ("2", 3), ("3", 3), ("4", 3)):
                    cmd(key, 0.6)
                    for _ in range(n): cmd("t", 0.5)
                    cmd("s", 0.4); cmd("s", 0.4)
                cmd("0", 0.3)
                for _ in range(16): cmd("d", 1.0)
                cmd("0z" + "v" * 16, 0.3)          # silence, amplifier off, volume 0
            cmd("M0zp", 0.5)
            s.close()
        (out / "console.log").write_text(log)
        rep["lines"] = [l for l in log.splitlines() if l.startswith(("BENCH", "PITCH", "BARS", "DTMF", "STATE"))]
        if a.restore:
            code, o = sh([DSLITE, "flash", "-c", str(CCXML), "-f", "-v", "-r", "1", "-u", a.restore], 600)
            rep["restore_tail"] = o.strip().splitlines()[-3:]
            time.sleep(2)
            sh([DSLITE, "memory", "-c", str(CCXML), "-r", "0,16", "-o", str(out / "after-first64.bin")])
        lk.write_owner("holding")
    (out / "report.json").write_text(json.dumps(rep, indent=1)); print(json.dumps(rep, indent=1))
    return 0 if rep["flash_ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
