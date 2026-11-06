#!/usr/bin/env python3
"""Print a board's serial output for a while, optionally resetting it first so the boot lines are caught.
    python serialmon.py <port> [seconds] [--baud 115200] [--reset <XDS110 serial>] [--send <line>]...
Needs pyserial (the repo's ble venv has it: ~/.venvs/ble/bin/python)."""
import argparse
import subprocess
import sys
import time

import serial

XDS110RESET = "/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("seconds", nargs="?", type=float, default=10)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--reset", help="XDS110 probe serial to reset after the port is open")
    ap.add_argument("--send", action="append", default=[], help="line to write after the reset (repeatable)")
    a = ap.parse_args()
    s = serial.Serial(a.port, a.baud, timeout=0.2)
    s.reset_input_buffer()
    if a.reset:
        subprocess.run([XDS110RESET, "-s", a.reset], capture_output=True)
    for line in a.send:
        time.sleep(0.3)
        s.write(line.encode() + (b"" if line.endswith("\n") else b"\n"))
    end = time.time() + a.seconds
    while time.time() < end:
        d = s.read(512)
        if d:
            sys.stdout.write(d.decode("utf-8", "replace"))
            sys.stdout.flush()


if __name__ == "__main__":
    main()
