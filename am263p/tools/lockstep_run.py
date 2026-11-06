#!/usr/bin/env python3
"""Run an image on R5FSS0-0 in lockstep (tools/lockstep.js) and capture UART0. Needs pyserial.
lockstep_run.py <image.out> <run_seconds> <log>   (the log gets the FRL_LS lines and the console, with times)"""
import os, subprocess, sys, threading, time
import serial
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capload
RUN = "/Applications/ti/ccs2101/ccs/scripting/run.sh"
img, secs, log = os.path.abspath(sys.argv[1]), float(sys.argv[2]), sys.argv[3]
buf = bytearray(); stop = [False]
def rd():
    p = serial.Serial(capload.PORT, 115200, timeout=0.2)
    while not stop[0]: buf.extend(p.read(4096))
    p.close()
th = threading.Thread(target=rd); th.start(); time.sleep(0.5)
env = dict(os.environ, IMAGE=img, RUN_MS=str(int(secs * 1000)))
r = subprocess.run([RUN, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lockstep.js")], capture_output=True, text=True, env=env, timeout=secs + 300)
stop[0] = True; th.join()
ls = [l for l in (r.stdout + r.stderr).splitlines() if "FRL_LS" in l or "rror" in l]
out = "\n".join(ls) + "\n--- console\n" + buf.decode(errors="replace").replace("\r", "")
open(log, "w").write(out); print(out)
