#!/usr/bin/env python3
"""Load one image per R5F core on the LP-AM263P and capture the UART0 console until a regex appears.
The console is opened before the first load (the SDK prints once at start). The first image goes on with the GEL
connection file (chip setup). MC_GEL picks the file for the later ones:
  all (default): the GEL file for every core. Every load worked, including after a probe reset; in these runs core 2's
       shared-memory console lines never reached core 0's console, though core 2 ran (pass results through memory).
  none: the no-GEL file. Worked until a probe reset; after it, R5FSS1 refused loads with "Does not match the target
       endianness" until each core had one GEL connect (seen 2026-10-11 on core 2).
  prep: GEL-load cores 1..3 first, then load all in order without GEL for 1..3. In 2 of 2 tries nothing came back.
The GEL connect on core 0 runs TI's chip setup, which zeroes on-chip RAM (L2 init): programs already running on cores
1..3 stop and shared memory is cleared (seen 2026-10-11). To add core 0 last, put a GEL load on core 0 first and the
later core-0 load without GEL. MC_FIRST_GEL=0 makes the first load use the no-GEL file too (collect from a running set).
mcload.py <done_regex> <wait_s> <log> <core>=<image.out> [<core>=<image.out> ...]     cores: 0..3 (R5FSS0-0 ... R5FSS1-1)
Prints each load's result and the console lines with times from the last load."""
import os, re, subprocess, sys, threading, time
import serial
HERE = os.path.dirname(os.path.abspath(__file__))
DSLITE = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
SERIAL = os.environ.get("AM263P_SERIAL", "S26E0426")   # the XDS110 probe serial
GEL = os.path.join(HERE, "..", f"AM263Px_ZCZ_C_{SERIAL}.ccxml")
NOGEL = os.path.join(HERE, "..", f"AM263Px_nogel_{SERIAL}.ccxml")
PORT = f"/dev/cu.usbmodem{SERIAL}1"

def run(done, wait_s, loads, baud=115200):
    pat = re.compile(done.encode()); buf = bytearray(); ev = []; stop = [False]; t0 = time.time()
    def rd():
        s = serial.Serial(PORT, baud, timeout=0.2)
        while not stop[0]:
            d = s.read(4096)
            if d: buf.extend(d); ev.append((time.time() - t0, len(d)))
        s.close()
    th = threading.Thread(target=rd); th.start(); time.sleep(0.5)
    results = []
    if os.environ.get("MC_GEL", "all") == "prep":
        for core, img in loads[1:]:
            subprocess.run([DSLITE, "load", "-c", GEL, "-n", str(core), "-t", "120", img], capture_output=True, text=True, timeout=200)
    for i, (core, img) in enumerate(loads):
        cfg = GEL if ((i == 0 and os.environ.get("MC_FIRST_GEL", "1") != "0") or os.environ.get("MC_GEL", "all") == "all") else NOGEL
        r = subprocess.run([DSLITE, "load", "-c", cfg, "-n", str(core), "-t", "120", img],
                           capture_output=True, text=True, timeout=200)
        ok = r.returncode == 0 and "Success" in r.stdout
        results.append((core, ok, round(time.time() - t0, 2), "" if ok else (r.stdout + r.stderr)[-400:]))
    tl = time.time() - t0
    end = time.time() + wait_s
    while time.time() < end and not pat.search(bytes(buf)): time.sleep(0.2)
    stop[0] = True; th.join()
    lines = []; cur = b""; pos = 0
    for tt, n in ev:
        cur += bytes(buf[pos:pos + n]); pos += n
        while b"\n" in cur:
            l, cur = cur.split(b"\n", 1); lines.append((tt - tl, l.decode(errors="replace").rstrip("\r")))
    return results, lines

if __name__ == "__main__":
    done, wait_s, log = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    loads = [(int(a.split("=", 1)[0]), a.split("=", 1)[1]) for a in sys.argv[4:]]
    results, lines = run(done, wait_s, loads)
    for r in results: print("load core %d ok=%s at %.2f s %s" % r)
    txt = "\n".join("%.3f %s" % x for x in lines) + "\n"
    open(log, "w").write("\n".join("load core %d ok=%s at %.2f s %s" % r for r in results) + "\n" + txt)
    print(txt)
