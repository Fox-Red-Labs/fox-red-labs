#!/usr/bin/env python3
"""FRL-GP1 serial check for the ESP32-BLE-Gamepad port matrix (Fox Red Labs).

  gp_serial.py <board> [secs] [--golden FILE] [--write-golden FILE]

Reads the board's console for <secs> seconds (default 12; the firmware prints one block every 3 s),
parses the last complete FRL-GP1 block and compares its MAP and RPT1..RPT4 lines with the golden file
taken from the ESP32-C6 running the unmodified library. Prints PASS/FAIL per line and the TIME/INFO lines.
Opens the port exclusively; needs pyserial.
"""
import sys, time, os, re, serial
PORTS = {"esp32c6": ("/dev/cu.usbmodem142401", 115200), "cc2340r5": ("/dev/cu.usbmodemLS470GTU1", 115200),
         "cc35x1": ("/dev/cu.usbmodemE10000FQ1", 115200), "mspm0g3519": ("/dev/cu.usbmodemMG0000011", 115200),
         "f2800137": ("/dev/cu.usbmodemCL3710811", 115200), "nucleo-u083rc": ("/dev/cu.usbmodem1414102", 115200)}
HERE = os.path.dirname(os.path.abspath(__file__))
a = sys.argv[1:]; board = a.pop(0)
golden = os.path.join(HERE, "golden.txt"); wg = None
if "--golden" in a: i = a.index("--golden"); golden = a[i + 1]; del a[i:i + 2]
if "--write-golden" in a: i = a.index("--write-golden"); wg = a[i + 1]; del a[i:i + 2]
secs = float(a[0]) if a else 12.0
port, baud = PORTS[board]
if board == "cc35x1" and os.environ.get("GP_BAUD"): baud = int(os.environ["GP_BAUD"])
def opn():
    for _ in range(40):
        try: return serial.Serial(port, baud, timeout=0.2, exclusive=True)
        except Exception: time.sleep(0.5)
    raise SystemExit("cannot open " + port)
s = opn()
end = time.time() + secs; buf = b""; lines = []
while time.time() < end:
    try: buf += s.read(4096)
    except Exception:
        try: s.close()
        except Exception: pass
        s = opn(); continue
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); lines.append(l.decode("utf-8", "replace").rstrip("\r"))
s.close()
blocks = []; cur = None
for l in lines:
    if l.strip() == "FRL-GP1 begin": cur = []
    elif l.strip() == "FRL-GP1 end":
        if cur is not None: blocks.append(cur)
        cur = None
    elif cur is not None: cur.append(l.strip())
print(f"{len(lines)} lines read from {port}, {len(blocks)} complete blocks")
for l in lines:
    if l.strip() and not l.startswith(("RPT", "MAP")) and "FRL-GP1" not in l: print("  console:", l[:120])
if not blocks: print("GP SERIAL CHECK: FAIL (no FRL-GP1 block)"); sys.exit(1)
b = blocks[-1]; d = {}
for l in b:
    k = l.split(" ", 1)[0]; d[k] = l
if wg:
    open(wg, "w").write("\n".join(l for l in b if l.startswith(("MAP", "RPT"))) + "\n"); print("golden written", wg)
ok = True
if os.path.exists(golden):
    g = {l.split(" ", 1)[0]: l.strip() for l in open(golden) if l.strip()}
    for k in ["MAP", "RPT1", "RPT2", "RPT3", "RPT4"]:
        same = d.get(k) == g.get(k)
        ok &= same
        print(f"  {k}: {'SAME' if same else 'DIFFERENT'}" + ("" if same else f"\n    board : {d.get(k)}\n    golden: {g.get(k)}"))
else: print("no golden file; printing the block"); [print("  ", l) for l in b]
for l in b:
    if l.startswith(("TIME", "INFO")): print(" ", l)
print("GP SERIAL CHECK:", "PASS" if ok else "FAIL")
