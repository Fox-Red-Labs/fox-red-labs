#!/usr/bin/env python3
"""Reboot the AM62L EVM from Linux (or watch a normal boot with --log-only SECONDS), stop U-Boot's autoboot on the console (UART A), run U-Boot commands, and
optionally boot by hand and wait for the login prompt. Every byte goes to LOG.
Usage: ub_reboot.py LOG --ssh root@<board ip> [--cmd "printenv"]... [--boot "booti ..."] [--wait-login 300]
       --no-reboot: the board is already resetting (power cycle by hand); just watch and stop autoboot.
Hold the probe lock: python3 tools/frl/lock.py exec --serial S62I0364 --op console -- python3 ub_reboot.py ..."""
import argparse, fcntl, os, select, struct, subprocess, sys, termios, time
PORT = '/dev/cu.usbserial-072518103640'
ap = argparse.ArgumentParser()
ap.add_argument('log'); ap.add_argument('--ssh'); ap.add_argument('--cmd', action='append', default=[])
ap.add_argument('--boot', action='append', default=[]); ap.add_argument('--wait-login', type=float, default=0)
ap.add_argument('--no-reboot', action='store_true'); ap.add_argument('--log-only', type=float, default=0, help='do not stop autoboot; log the normal boot until login: or this many seconds'); ap.add_argument('--autoboot-timeout', type=float, default=240)
a = ap.parse_args()
fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
t = termios.tcgetattr(fd); t[0] = 0; t[1] = 0; t[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; t[3] = 0
termios.tcsetattr(fd, termios.TCSANOW, t); fcntl.ioctl(fd, 0x80045402, struct.pack('i', 115200))
log = open(a.log, 'ab'); buf = b''; t0 = time.time()
def pump(sec):
    global buf
    end = time.time() + sec
    while time.time() < end:
        if select.select([fd], [], [], 0.1)[0]:
            try: d = os.read(fd, 4096)
            except BlockingIOError: continue
            log.write(d); log.flush(); buf += d
def wait_for(pat, sec):
    end = time.time() + sec
    while time.time() < end:
        pump(0.2)
        if pat in buf: return True
    return False
def note(s): print(f'[{time.time()-t0:6.1f}s] {s}', flush=True)
os.read(fd, 65536) if select.select([fd], [], [], 0)[0] else None
if not a.no_reboot:
    note('reboot over ssh')
    subprocess.run(['ssh', '-o', 'ConnectTimeout=8', a.ssh, 'sync; (sleep 1; reboot) >/dev/null 2>&1 &'], timeout=30)
if a.log_only:
    ok = wait_for(b'login:', a.log_only); note('login prompt seen' if ok else 'NO login prompt'); sys.exit(0 if ok else 4)
note('waiting for autoboot prompt')
if not wait_for(b'autoboot', a.autoboot_timeout):
    note('NO autoboot prompt seen'); sys.exit(2)
for _ in range(20): os.write(fd, b' '); pump(0.05)
os.write(fd, b'\r')
if not wait_for(b'=> ', 10): note('NO U-Boot prompt'); sys.exit(3)
note('U-Boot prompt')
def run(cmd, sec=60):
    global buf
    buf = b''; os.write(fd, (cmd + '\r').encode())
    ok = wait_for(b'=> ', sec)
    sys.stdout.write(buf.decode('latin1').replace('\r', '')); sys.stdout.flush()
    return ok
for c in a.cmd: run(c)
if a.boot:
    for c in a.boot[:-1]: run(c)
    buf = b''; note('booting: ' + a.boot[-1]); os.write(fd, (a.boot[-1] + '\r').encode())
    if a.wait_login:
        ok = wait_for(b'login:', a.wait_login)
        note('login prompt seen' if ok else 'NO login prompt'); sys.exit(0 if ok else 4)
