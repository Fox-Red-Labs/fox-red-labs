#!/usr/bin/env python3
"""Talk to uLisp on the back-channel serial port and check answers. Prints one line per check and a JSON-free summary.
  ~/.venvs/labgrid/bin/python ports/ulisp/tests/repl_check.py /dev/cu.usbmodemMG0200011 [--bench] [--blink-start]
Take the probe lock first if you also flash. Reads only; --blink-start leaves the LED loop running (send '~' to stop it)."""
import re, sys, time, serial

port = sys.argv[1]
bench = "--bench" in sys.argv
s = serial.Serial(port, 115200, timeout=0.2)

def drain(wait=0.5):
    end = time.time() + wait; out = b""
    while time.time() < end:
        out += s.read(256)
    return out.decode("latin1")

def send(line, timeout=5.0):
    """Send one line, collect until the next prompt (digits then '> ')."""
    s.reset_input_buffer()
    for ch in line + "\r":
        s.write(ch.encode()); time.sleep(0.002)   # a pasted block in one burst is also fine; this is gentler
    end = time.time() + timeout; buf = ""
    while time.time() < end:
        buf += s.read(256).decode("latin1")
        if re.search(r"\n\d+> $", buf) or re.search(r"\n\d+ : \d+> $", buf):
            break
    return buf

fails = 0
def check(name, line, want, timeout=5.0):
    global fails
    out = send(line, timeout)
    body = out.replace("\r", "").strip()
    ok = re.search(want, body, re.I) is not None
    fails += (not ok)
    print(("PASS " if ok else "FAIL ") + name + " | " + line + " -> " + " / ".join(body.split("\n")[:4]))
    return body

s.write(b"~"); drain(0.5)           # break out of anything running
s.write(b"\r"); out = drain(0.8)
print("prompt:", repr(out[-20:]))
check("add", "(+ 1 2)", r"\b3\b")
check("float", "(/ 1.0 3)", r"0\.33")
check("defun", "(defun sq (x) (* x x))", r"\bsq\b")
check("call", "(sq 12)", r"\b144\b")
check("list", "(mapcar sq '(1 2 3 4))", r"\(1 4 9 16\)")
check("string", "(concatenate 'string \"Fox\" \"Red\")", r"FoxRed")
check("room", "(room)", r"\b\d{3,5}\b")
check("fib-def", "(defun fib (n) (if (< n 3) 1 (+ (fib (- n 1)) (fib (- n 2)))))", r"\bfib\b")
check("fib20", "(fib 20)", r"\b6765\b")
check("time-fib20", "(time (fib 20))", r"6765", 20)
check("tak-def", "(defun tak (x y z) (if (not (< y x)) z (tak (tak (1- x) y z) (tak (1- y) z x) (tak (1- z) x y))))", r"\btak\b")
check("tak-small", "(tak 12 8 4)", r"\b5\b", 20)
check("pinmode", "(pinmode :led-builtin :output)", r"nil|NIL|:output|\bT\b|\d")
check("led-on", "(digitalwrite :led-builtin t)", r"\bt\b")
check("led-read", "(digitalread :led-builtin)", r"\bt\b")
check("led-off", "(digitalwrite :led-builtin nil)", r"nil")
check("led-read0", "(digitalread :led-builtin)", r"nil")
check("err", "(car 5)", r"[Ee]rror")
check("recover", "(+ 40 2)", r"\b42\b")
if bench:
    print("--- benchmarks (times are what uLisp's own (time ...) prints)")
    check("TAK 18 12 6", "(time (tak 18 12 6))", r"Time:", 120)
    check("FIB 23", "(time (fib 23))", r"Time:", 120)
    check("Q2 7 8", "(defun q2 (x y) (if (or (< x 1) (< y 1)) 1 (+ (q2 (- x (q2 (1- x) y)) y) (q2 x (- y (q2 x (1- y)))))))", r"q2")
    check("q2 run", "(time (q2 7 8))", r"Time:", 200)
    check("gc", "(time (gc))", r"Time:", 20)
if "--blink-start" in sys.argv:
    s.write(b"(loop (digitalwrite :led-builtin t) (delay 150) (digitalwrite :led-builtin nil) (delay 150))\r")
    time.sleep(0.5)
    print("LED loop started; stop it with '~' on the port")
print("fails:", fails)
sys.exit(1 if fails else 0)
