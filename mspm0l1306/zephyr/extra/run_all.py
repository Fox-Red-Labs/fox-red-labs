#!/usr/bin/env python3
"""Flash each extra Zephyr image to the LP-MSPM0L1306 (probe ML130001) and record UART0 for a set time.
  run_all.py <OUT folder of build_all.sh> [names...]
For the shell image it types `demo ping` and expects `pong`. Writes results/<name>.txt."""
import os, select, subprocess, sys, termios, time
OUT = sys.argv[1]; HERE = os.path.dirname(os.path.abspath(__file__))
OCD = os.path.join(HERE, "..", "..", "tools", "ocd.sh"); TTY = "/dev/cu.usbmodemML1300011"
TM = ["basic", "cooperative", "preemptive", "interrupt", "interrupt_preemption", "memory", "message", "synchronization"]
JOBS = [("hello-minlibc", f"{OUT}/hello-minlibc/zephyr/zephyr.hex", 6), ("sync", f"{OUT}/sync/zephyr/zephyr.hex", 8),
        ("logger-deferred", f"{OUT}/logger-deferred/zephyr/zephyr.hex", 10), ("logger-immediate", f"{OUT}/logger-immediate/zephyr/zephyr.hex", 10),
        ("logger-deferred-896", f"{OUT}/logger-deferred-896/zephyr/zephyr.hex", 10),
        ("logger-deferred-lt768", f"{OUT}/logger-deferred-lt768/zephyr/zephyr.hex", 10),
        ("shell", f"{OUT}/shell/zephyr/zephyr.hex", 8), ("latency", f"{OUT}/latency/zephyr/zephyr.hex", 240)] + \
       [(f"tm-{t}", f"{OUT}/tm-{t}.hex", 95) for t in TM]
want = set(sys.argv[2:])
def run(name, hexf, secs):
    fl = subprocess.run([OCD, "reset halt", f"program {hexf} verify"], capture_output=True, text=True).stdout
    if "Verified OK" not in fl:
        return f"FLASH FAILED\n{fl[-600:]}"
    fd = os.open(TTY, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CLOCAL | termios.CREAD; a[3] = a[0] = a[1] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIOFLUSH)
    subprocess.run([OCD, "reset run"], capture_output=True)
    buf, t0, typed = b"", time.time(), False
    while time.time() - t0 < secs:
        if select.select([fd], [], [], 0.3)[0]:
            buf += os.read(fd, 4096)
        if name == "shell" and not typed and time.time() - t0 > 2:
            for c in b"\r\ndemo ping\r\n": os.write(fd, bytes([c])); time.sleep(0.02)
            typed = True
        if name == "latency" and (b"PROJECT EXECUTION SUCCESSFUL" in buf or b"PROJECT EXECUTION FAILED" in buf):
            break
    os.close(fd)
    return buf.decode(errors="replace").replace("\r", "")
for name, hexf, secs in JOBS:
    if want and name not in want: continue
    t0 = time.time(); text = run(name, hexf, secs)
    open(os.path.join(HERE, "results", name + ".txt"), "w").write(text)
    print(f"== {name} ({time.time()-t0:.0f} s, {len(text)} chars)"); print("\n".join(text.strip().splitlines()[-6:]), flush=True)
