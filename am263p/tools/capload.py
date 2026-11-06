#!/usr/bin/env python3
"""Load an image into the LP-AM263P (probe S26E0426) with DSLite and capture UART0 (the XDS110 port ...4261, 115200 8N1).
The port is opened BEFORE the load: the SDK console prints once at start, and a port opened later sees nothing.
Needs pyserial (e.g. ~/.venvs/csi/bin/python). Library:  run(image, done_regex, wait_s) -> (text, load_s, lines_with_times)
CLI:  capload.py <image.out> <done_regex> <wait_s> [log]"""
import os, re, subprocess, sys, threading, time
import serial
DSLITE = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
HERE = os.path.dirname(os.path.abspath(__file__))
SERIAL = os.environ.get("AM263P_SERIAL", "S26E0426")   # the XDS110 probe serial; tools/list_cores.sh makes its .ccxml
CCXML = os.path.join(HERE, "..", f"AM263Px_ZCZ_C_{SERIAL}.ccxml")
PORT = f"/dev/cu.usbmodem{SERIAL}1"

def run(image, done_regex, wait_s, baud=115200, load_timeout=120, drain=0.0):
    """drain > 0: read and discard for that long before the load. The XDS110 holds what the previous program printed
    while the port was closed and delivers it on open; without a drain it is counted as this program's output."""
    pat = re.compile(done_regex.encode()); buf = bytearray(); ev = []; stop = [False]; t0 = time.time()
    def rd():
        s = serial.Serial(PORT, baud, timeout=0.2)
        while not stop[0]:
            d = s.read(4096)
            if d: buf.extend(d); ev.append((time.time() - t0, len(d)))
        s.close()
    th = threading.Thread(target=rd); th.start(); time.sleep(0.5)
    if drain > 0:
        time.sleep(drain); del buf[:]; del ev[:]
    r = subprocess.run([DSLITE, "load", "-c", CCXML, "-n", os.environ.get("AM263P_CORE", "0"), "-t", str(load_timeout), image], capture_output=True, text=True, timeout=load_timeout + 60)
    tl = time.time() - t0
    ok = r.returncode == 0 and "Success" in r.stdout
    end = time.time() + wait_s
    while time.time() < end and not pat.search(bytes(buf)): time.sleep(0.2)
    stop[0] = True; th.join()
    lines = []; cur = b""; pos = 0
    for tt, n in ev:
        cur += bytes(buf[pos:pos + n]); pos += n
        while b"\n" in cur:
            l, cur = cur.split(b"\n", 1); lines.append((tt - tl, l.decode(errors="replace").rstrip("\r")))
    return bytes(buf).decode(errors="replace"), (ok, tl, r.stdout[-300:] + r.stderr[-300:]), lines

if __name__ == "__main__":
    text, info, lines = run(sys.argv[1], sys.argv[2], float(sys.argv[3]))
    print(info[:2]); print("\n".join("%7.3f %s" % x for x in lines))
    if len(sys.argv) > 4: open(sys.argv[4], "w").write("\n".join("%.3f %s" % x for x in lines) + "\n")
