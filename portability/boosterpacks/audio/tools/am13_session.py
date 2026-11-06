#!/usr/bin/env python3
"""One bench session for the BOOSTXL-AUDIO on the LP-AM13E230 (experiment E237). Holds the probe lock for the whole run.

  AM13_CLEARED=1 python3 tools/am13_session.py --image <audio.out> --out <folder> [--play-demo]

Steps: back up main flash twice and compare, flash the audio image, run the matrix over the backchannel UART (frame type by
amplifier line state, each with a microphone capture), optionally play the audible demo, then flash the backup back and read it
back. The board holds a motor guard image: this script refuses to run unless AM13_CLEARED=1 is set after the owner has said no
motor stage or supply is wired to the board. Needs the bench venv (pyserial): ~/.venvs/labgrid/bin/python."""
import argparse, hashlib, os, subprocess, sys, time, json
from pathlib import Path

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO / "tools" / "frl"))
from lock import ProbeLock  # noqa: E402

DSLITE = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
CCXML = REPO / "am13e230" / "AM13E23019_AL130001.ccxml"
PORT = "/dev/cu.usbmodemAL1300011"
FLASH_WORDS = "0x20000"  # 512 kB


def sh(argv, timeout=300):
    p = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
    return p.returncode, (p.stdout + p.stderr)


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def read_flash(path):
    code, out = sh([DSLITE, "memory", "-c", str(CCXML), "-r", f"0x0,{FLASH_WORDS}", "-o", str(path)], 600)
    if code != 0 or not Path(path).exists() or Path(path).stat().st_size != 4 * int(FLASH_WORDS, 16):
        raise RuntimeError("flash read failed: " + out[-300:])


class Con:
    def __init__(self):
        import serial
        self.s = serial.Serial(PORT, 115200, timeout=0.1)
        self.log = ""

    def pump(self, secs):
        end = time.time() + secs
        while time.time() < end:
            d = self.s.read(4096)
            if d:
                self.log += d.decode("latin1")

    def cmd(self, keys, wait=0.4):
        for k in keys:
            self.s.write(k.encode()); self.pump(0.15)
        self.pump(wait)

    def capture(self, tag):
        mark = len(self.log)
        self.log += f"\n#### {tag}\n"
        self.s.write(b"c")
        end = time.time() + 12
        while time.time() < end and "END" not in self.log[mark:]:
            self.pump(0.2)
        return "END" in self.log[mark:]


MATRIX = [  # (tag, keys before the capture)
    ("baseline silent, amp floating, mic on", "0zm"),
    ("split8 tone, amp floating", "z8"), ("split8 tone, amp low", "a8"), ("split8 tone, amp high", "A8"),
    ("tone16, amp floating", "z1"), ("tone16, amp low", "a1"), ("tone16, amp high", "A1"),
    ("tune16, amp low", "a2"), ("tune16, amp high", "A2"),
    ("silent, amp low", "a0"),
]


EXTRA = [("silent baseline, amp low, mic on", "a0m"), ("tone16 amp low, volume 4", "a1vvvvvvvv"), ("tune16 amp low, volume 4", "2"), ("tone16 amp low, volume 8", "1VVVV"), ("tone16 amp low, volume 12 again", "VVVV"), ("tune16 amp low, volume 12", "2")]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", required=True); ap.add_argument("--out", required=True)
    ap.add_argument("--play-demo", action="store_true"); ap.add_argument("--amp", default="a", help="amp key for the demo: a or A")
    ap.add_argument("--skip-restore", action="store_true"); ap.add_argument("--no-matrix", action="store_true")
    a = ap.parse_args()
    if os.environ.get("AM13_CLEARED") != "1":
        print("Refusing: set AM13_CLEARED=1 only after the owner says no motor stage or supply is wired to the AM13E230."); return 2
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    rep = {"started": time.strftime("%Y-%m-%dT%H:%M:%S")}
    with ProbeLock("AL130001", "audio-am13", "audio session") as lk:
        if lk.previous and lk.previous.get("state") == "writing":
            rep["warning"] = "the last run on this probe stopped while writing"
        lk.write_owner("writing")
        # 1. backup twice
        read_flash(out / "backup1.bin"); read_flash(out / "backup2.bin")
        rep["backup_sha256"] = sha(out / "backup1.bin"); rep["backup_identical"] = sha(out / "backup1.bin") == sha(out / "backup2.bin")
        if not rep["backup_identical"]:
            rep["error"] = "the two backup reads differ; stopped before any write"; print(json.dumps(rep, indent=1)); return 1
        rep["backup_nonblank_bytes"] = sum(1 for b in (out / "backup1.bin").read_bytes() if b != 0xFF)
        # 2. flash audio
        t0 = time.time()
        code, o = sh([DSLITE, "flash", "-c", str(CCXML), "-e", "-f", "-v", "-r", "1", "-u", "-t", "60", a.image], 240)
        rep["flash_s"] = round(time.time() - t0, 1); rep["flash_ok"] = code == 0 or "Program verification successful" in o
        rep["flash_tail"] = o.strip().splitlines()[-3:]
        try:
            time.sleep(1.5)
            con = Con(); con.pump(1.0)
            con.log += "\n#### boot\n"; con.cmd("p", 0.5)
            rep["captures"] = {}
            for tag, keys in (EXTRA if a.no_matrix else MATRIX):
                con.cmd(keys, 0.6)
                rep["captures"][tag] = con.capture(tag)
            if a.play_demo:
                con.cmd(a.amp + "m", 0.3)
                con.cmd("1", 8.0); con.cmd("0", 1.0); con.cmd("2", 27.0); con.cmd("2", 27.0)
                con.cmd("0", 0.3)
            con.cmd("0zM", 0.3)
            (out / "console.log").write_text(con.log)
            con.s.close()
        except Exception as e:
            rep["session_error"] = f"{type(e).__name__}: {e}"
            (out / "console.log").write_text(getattr(locals().get("con"), "log", ""))
        # 3. restore
        if not a.skip_restore:
            code, o = sh([DSLITE, "flash", "-c", str(CCXML), "-e", "-f", "-v", "-r", "1", "-u", "-t", "60", f"{out / 'backup1.bin'},0x0"], 600)
            rep["restore_ok"] = code == 0 or "Program verification successful" in o
            rep["restore_tail"] = o.strip().splitlines()[-3:]
            time.sleep(1.0)
            read_flash(out / "after.bin")
            rep["after_sha256"] = sha(out / "after.bin"); rep["restore_identical"] = rep["after_sha256"] == rep["backup_sha256"]
        lk.write_owner("holding")
    (out / "report.json").write_text(json.dumps(rep, indent=1)); print(json.dumps(rep, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
