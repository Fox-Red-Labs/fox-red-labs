#!/usr/bin/env python3
"""Latency of the Smoke CO Alarm over Matter: writes `SMOKE 1`/`CO 1` (and the clearing `0`) lines to the board's console UART and
times the attribute report that the matter.js shell prints from `subscribe <node>`. Both stamps are the Mac's clock.

  measure.py --shell-dir DIR --node 1 [--events 20] [--serial /dev/cu.usbmodemE10000FQ1] [--storage-node 0]

DIR is the folder where `npm i @matter/nodejs-shell@0.17.9` was run, with node on PATH. The node must already be commissioned
into the shell's storage (see the README). One event = a raise (0 -> 1) followed by a clear (1 -> 0); each is timed.
Output: one CSV line per transition on stdout, then the median and worst per kind."""
import argparse, os, pty, re, select, statistics, subprocess, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument('--shell-dir', required=True); ap.add_argument('--node', default='1')
ap.add_argument('--events', type=int, default=20); ap.add_argument('--serial', default='/dev/cu.usbmodemE10000FQ1')
ap.add_argument('--storage-node', default='0'); ap.add_argument('--gap', type=float, default=3.0)
ap.add_argument('--timeout', type=float, default=10.0)
a = ap.parse_args()

ser = serial.Serial(a.serial, 115200, timeout=0.1)
pid, fd = pty.fork()
if pid == 0:
    os.chdir(a.shell_dir)
    os.execvp('node', ['node', 'node_modules/@matter/nodejs-shell/dist/cjs/app.js', a.storage_node])
buf = ''
def pump(t):
    """read shell output for up to t seconds; returns list of (time, line)"""
    global buf
    out = []; end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.02)
        if r:
            try: d = os.read(fd, 65536).decode('utf8', 'replace')
            except OSError: break
            now = time.time(); buf += d
            while '\n' in buf:
                line, buf = buf.split('\n', 1); out.append((now, line.rstrip('\r')))
    return out
def wait_for(rx, t):
    end = time.time() + t; rx = re.compile(rx)
    while time.time() < end:
        for ts, l in pump(0.2):
            if rx.search(l): return ts, l
    return None, None
def send(s): os.write(fd, (s + '\n').encode())

pump(8); send('subscribe ' + a.node)
ts, l = wait_for(r'Attribute .*SmokeState|Attribute .*ExpressedState|Attribute .*/[0-9]+/', 20)
pump(3)  # let the priming report settle
rows = []
rx_attr = re.compile(r'Attribute \d+/(\d+)/(\d+)/(\w+) changed to (.*)')
for kind, tx_on, tx_off, attr in (('smoke', 'SMOKE 1', 'SMOKE 0', 'smokeState'), ('co', 'CO 1', 'CO 0', 'coState')):
    for i in range(a.events):
        for phase, line, want in (('raise', tx_on, '2'), ('clear', tx_off, '0')):
            pump(0.3)
            t0 = time.time(); ser.write((line + '\n').encode()); ser.flush()
            t1 = None; end = t0 + a.timeout
            while time.time() < end and t1 is None:
                for ts, l in pump(0.05):
                    m = rx_attr.search(l)
                    if m and m.group(3).lower() == attr.lower() and m.group(4).strip() == want:
                        t1 = ts; break
            ms = (t1 - t0) * 1000 if t1 else float('nan')
            rows.append((kind, i, phase, ms)); print('%s,%d,%s,%.0f' % (kind, i, phase, ms), flush=True)
            time.sleep(a.gap)
for kind in ('smoke', 'co'):
    for phase in ('raise', 'clear'):
        v = [r[3] for r in rows if r[0] == kind and r[2] == phase and r[3] == r[3]]
        lost = sum(1 for r in rows if r[0] == kind and r[2] == phase) - len(v)
        if v: print('SUMMARY %s %s n=%d lost=%d median=%.0f worst=%.0f ms' % (kind, phase, len(v), lost, statistics.median(v), max(v)))
        else: print('SUMMARY %s %s n=0 lost=%d' % (kind, phase, lost))
os.kill(pid, 15)
