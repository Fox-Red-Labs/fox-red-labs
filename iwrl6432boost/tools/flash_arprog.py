#!/usr/bin/env python3
"""Flash one .appimage with TI's arprog (SDK tools/boot) over the radar's application UART.
   flash_arprog.py <arprog_dir> <port> <image.appimage> <log.txt>
Runs `arprog_cmdline.py -p PORT -f IMAGE -s SFLASH -t META_IMAGE1` in a scratch cwd (the tool writes
codes.txt and batchstatus.txt there). arprog imports `idna.valid_contextj` without using it, so a stub module
stands in for it. The tool holds a serial break and waits (a very long timeout) for the board to be reset in
flashing mode, so start this first, then flip S1.1 OFF and press S2 (NRST). Prints a timestamped copy of the output;
the last line is FLASH_OK or FLASH_FAIL."""
import sys, os, subprocess, time, tempfile, termios
arp, port, img, log = sys.argv[1:5]
# Guard: arprog reads "any byte equal to 0xCC" as the bootloader's ACK, so a running demo's frame stream
# gives a false "connection succeeded" and the image bytes go to the demo's CLI. Refuse to start if the port is not silent.
fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
t_end, n = time.time() + 2.0, 0
while time.time() < t_end:
    try: n += len(os.read(fd, 4096))
    except BlockingIOError: time.sleep(0.02)
os.close(fd)
if n:
    print(f"REFUSED: {n} bytes arrived on {port} in 2 s; the board is not in flashing mode (reset it with S1.1 OFF first)")
    print("FLASH_FAIL"); sys.exit(2)
work = tempfile.mkdtemp(prefix="arprog-run-")
stub = os.path.join(work, "stub"); os.makedirs(stub)
open(os.path.join(stub, "idna.py"), "w").write("def valid_contextj(*a, **k): return True\n")
env = dict(os.environ, PYTHONPATH=stub + os.pathsep + arp, PYTHONUNBUFFERED="1")
t0 = time.time(); lines = []
p = subprocess.Popen([sys.executable, os.path.join(arp, "arprog_cmdline.py"), "-p", port, "-f", os.path.abspath(img),
                      "-s", "SFLASH", "-t", "META_IMAGE1"], cwd=work, env=env,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
buf = b""
while True:
    c = p.stdout.read(1)
    if not c: break
    if c in b"\n\r":
        l = buf.decode(errors="replace").rstrip(); buf = b""
        if l.strip(): lines.append(f"{time.time()-t0:7.2f}s {l}"); print(lines[-1], flush=True)
    else: buf += c
if buf.strip(): lines.append(f"{time.time()-t0:7.2f}s {buf.decode(errors='replace')}")
rc = p.wait()
cp = os.path.join(work, "codes.txt")
codes = open(cp).read() if os.path.exists(cp) else ""
lines.append(f"rc={rc} codes.txt={codes!r}"); print(lines[-1])
bad = any(w in l for l in lines for w in ("Nack", "NACK", "refused", "ERROR", "Traceback", "Exception"))
final = "FLASH_OK" if (rc == 0 and not bad and any("connection succeeded" in l for l in lines)) else "FLASH_FAIL"
lines.append(final); print(final)
open(log, "w").write("\n".join(lines) + "\n")
