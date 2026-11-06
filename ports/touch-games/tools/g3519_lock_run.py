#!/usr/bin/env python3
"""Flash an image on the LP-MSPM0G3519 under the probe lock, let it run, then read memory words with OpenOCD (still under the lock)
and optionally capture the backchannel UART.
  python3 g3519_lock_run.py IMAGE.out [--secs 3] [--mdw 0x20200028:12] [--uart 5]
Prints the memory words and the UART text. Same OpenOCD and probe as the lab bench script."""
import argparse, os, subprocess, sys, time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
from lock import ProbeLock
X = Path("~/zephyr-toolchain/xpack-openocd-0.12.0-7").expanduser()
SERIAL = "MG000001"

def ocd(cmds, timeout=120):
    a = [str(X / "bin" / "openocd"), "-s", str(X / "openocd" / "scripts"), "-c", "gdb_port disabled", "-c", "tcl_port disabled",
         "-c", "telnet_port disabled", "-f", "interface/xds110.cfg", "-c", f"adapter serial {SERIAL}", "-c", "adapter speed 1000",
         "-f", "target/ti_mspm0.cfg", "-c", "init"]
    for c in cmds: a += ["-c", c]
    a += ["-c", "shutdown"]
    r = subprocess.run(a, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout)
    return r.stdout

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image", nargs="?"); ap.add_argument("--secs", type=float, default=3)
    ap.add_argument("--mdw", action="append", default=[]); ap.add_argument("--uart", type=float, default=0)
    ap.add_argument("--no-flash", action="store_true")
    a = ap.parse_args()
    with ProbeLock(SERIAL, "touch-games", "touch games agent") as lk:
        lk.write_owner("writing")
        if not a.no_flash:
            out = ocd(["reset halt", f"program {{{a.image}}} verify", "reset run"])
            if "Verified OK" not in out:       # OpenOCD sometimes reads DEVICEID 0 after a sleeping image; DSLite is the fallback
                ds = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
                r = subprocess.run([ds, "flash", "-c", str(ROOT / "mspm0g3519/MSPM0G3519_MG000001.ccxml"), "-e", "-f", "-v", a.image],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=240)
                out = "Verified OK" if r.returncode == 0 else r.stdout[-600:]
            print("flash:", "Verified OK" if "Verified OK" in out else out[-600:])
        lk.write_owner("holding")
        ser = None
        if a.uart:
            import serial
            ser = serial.Serial(f"/dev/cu.usbmodem{SERIAL}1", 115200, timeout=0.2)
            if not a.no_flash: ocd(["reset run"])
            t = time.time(); buf = b""
            while time.time() - t < a.uart: buf += ser.read(512)
            print("uart:", buf.decode(errors="replace")); ser.close()
        for m in a.mdw:
            addr, n = m.split(":")
            out = ocd(["reset run", f"sleep {int(a.secs*1000)}", "halt", f"mdw {addr} {n}", "resume"])
            print("\n".join(l for l in out.splitlines() if l.startswith("0x")))
if __name__ == "__main__": main()
