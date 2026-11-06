#!/usr/bin/env python3
"""Hold the LP-MSPM0G3519 console open, pulse nRST, log what arrives, optionally send bytes.
  capture.py <log> <seconds> [--send "t:text" ...]   (t = seconds after the reset; \\r in text is a carriage return)
Opens the port first, then restarts the board (the lab's portboard.py, when present), so the banner is not missed. Without it, reset the board by hand.
STAMP=1 puts the host clock (seconds since the reset) in front of every received line, for timing a periodic timer from outside the chip."""
import sys, time, subprocess, os
try:
    import serial
except ImportError:
    sys.exit("error: pyserial missing (python3 -m pip install pyserial)")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
log, secs = sys.argv[1], float(sys.argv[2])
sends = sorted((float(a.split(":", 1)[0]), a.split(":", 1)[1].encode().decode("unicode_escape").encode()) for a in sys.argv[4:] if sys.argv[3] == "--send" for a in [a]) if len(sys.argv) > 3 else []
PORT = os.environ.get("PORT", "/dev/cu.usbmodemMG0000011")   # console of the LP-MSPM0G3519 (probe serial MG000001 + "1")
if not os.path.exists(PORT):
    sys.exit(f"error: PORT not found at {PORT} (set PORT to the board's console, see README: Set up once)")
s = serial.Serial(PORT, 115200, timeout=0.05)
s.reset_input_buffer()
PB = f"{REPO}/tools/ai_runs/portboard.py"
if os.path.exists(PB):
    subprocess.run(["python3", PB, "run", "g3519"], capture_output=True)
else:
    print("note: portboard.py not present; reset the board now")
t0 = time.time(); out = open(log, "wb"); i = 0; buf = b""
while time.time() - t0 < secs:
    while i < len(sends) and time.time() - t0 >= sends[i][0]:
        s.write(sends[i][1]); out.write(f"\n[sent {sends[i][1]!r} at {time.time()-t0:.1f}s]\n".encode()); i += 1
    d = s.read(256)
    if d and os.environ.get("STAMP"):
        now = time.time() - t0; buf += d
        while b"\n" in buf:
            ln, buf = buf.split(b"\n", 1); out.write(f"{now:9.3f} ".encode() + ln + b"\n")
        out.flush()
    elif d: out.write(d); out.flush()
out.close()
