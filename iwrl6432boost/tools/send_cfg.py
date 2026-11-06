#!/usr/bin/env python3
"""Send a TI mmWave demo .cfg to the radar's CLI, one line at a time, and print each answer.
   send_cfg.py <port> <file.cfg> [--keep-baud] [--log out.txt]
Comment lines (%) and blank lines are skipped. The line `baudRate N` is skipped unless --keep-baud
(the link stays at 115200, which the Mac reads without a custom rate). Each line waits up to 2 s for
"Done" or "Error"; the demo's CLI prompt is `mmwDemo:/>`. Needs pyserial.
Prints a line `CFG_DONE t=<seconds since the first line>` when the last line (sensorStart) answered."""
import sys, time, re, serial
port, cfg = sys.argv[1], sys.argv[2]
keep = "--keep-baud" in sys.argv
log = sys.argv[sys.argv.index("--log") + 1] if "--log" in sys.argv else None
s = serial.Serial(port, 115200, timeout=0.1)
s.reset_input_buffer()
out, t0, bad = [], time.time(), 0
def txt(b): return " | ".join(x.decode() for x in re.findall(rb"[\x20-\x7e]{3,}", b))
for raw in open(cfg):
    line = raw.strip()
    if not line or line.startswith("%"): continue
    if line.startswith("baudRate") and not keep:
        out.append(f"SKIP {line}"); print(out[-1]); continue
    s.write((line + "\n").encode())
    t, b = time.time(), b""
    while time.time() - t < 2.0:
        b += s.read(4096)
        if b"Done" in b or b"rror" in b:
            time.sleep(0.05); b += s.read(4096); break
    r = txt(b)[:200]
    if "Done" not in r: bad += 1
    out.append(f"{line} -> {r}"); print(out[-1], flush=True)
print(f"CFG_DONE t={time.time()-t0:.2f} not_done={bad}")
if log: open(log, "w").write("\n".join(out) + f"\nCFG_DONE t={time.time()-t0:.2f} not_done={bad}\n")
