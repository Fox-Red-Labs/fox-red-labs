#!/usr/bin/env python3
"""Two TI radios on one Thread network, over the air: LP-CC1354P10-1 running TI's ot-cli-ftd (leader), and
LP-EM-CC2340R5 running TI's ot-rcp driven by OpenThread's POSIX ot-cli on the Mac (joiner, full Thread device).
Measures: time to leader, time for the second device to attach, and ping round trip between the two radios.
Both links to the Mac are the boards' own debugger UARTs; the three jumper wires between the boards are unused here.
Usage: thread_two_radios.py <ot-cli posix binary> <out dir>"""
import os, re, select, subprocess, sys, termios, threading, time, fcntl, struct
from pathlib import Path

HOST_PORT, RADIO_PORT = "/dev/cu.usbmodemLS45013A1", "/dev/cu.usbmodemLS470GTU1"
otcli, out = sys.argv[1], Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
log = open(out / "session.log", "w")
def note(s): print(s, flush=True); log.write(f"{time.time():.3f} {s}\n"); log.flush()

class Leader:
    """TI ot-cli-ftd on the CC1354 over its UART at 921600."""
    def __init__(self, port):
        self.fd = os.open(port, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
        a = termios.tcgetattr(self.fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0; a[1] = 0; a[0] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, a); fcntl.ioctl(self.fd, 0x80045402, struct.pack("L", 921600)); termios.tcflush(self.fd, termios.TCIOFLUSH)
    def cmd(self, c, wait=1.5):
        os.write(self.fd, c.encode() + b"\r\n"); b, end = b"", time.time() + wait
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.1)
            if r: b += os.read(self.fd, 4096)
        t = b.decode("latin-1").replace("\r", ""); lines = [l.strip() for l in t.split("\n") if l.strip() and l.strip() != c and l.strip() != ">"]
        note(f"[leader] > {c} :: {' | '.join(lines)[:200]}"); return lines

class Joiner:
    """OpenThread POSIX ot-cli on the Mac driving the CC2340R5 RCP."""
    def __init__(self, binary, port):
        self.p = subprocess.Popen([binary, f"spinel+hdlc+uart://{port}?uart-baudrate=921600"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.buf = []; threading.Thread(target=lambda: [self.buf.append(l) for l in self.p.stdout], daemon=True).start(); time.sleep(4)
    def cmd(self, c, wait=1.5):
        n = len(self.buf); self.p.stdin.write(c + "\n"); self.p.stdin.flush(); time.sleep(wait)
        lines = [l.strip().lstrip("> ").strip() for l in self.buf[n:] if l.strip() and not l.startswith("Oct")]
        lines = [l for l in lines if l]
        note(f"[joiner] > {c} :: {' | '.join(lines)[:200]}"); return lines
    def close(self):
        try: self.p.stdin.write("exit\n"); self.p.stdin.flush(); time.sleep(0.5)
        finally: self.p.kill()

L = Leader(HOST_PORT); J = Joiner(otcli, RADIO_PORT)
try:
    L.cmd("version"); J.cmd("rcp version")
    # fresh network on the leader. No factoryreset here: it reboots the CC1354, and a program loaded by the debugger
    # does not come back from a reboot without a power cycle (seen 2026-10-06). A just-loaded image has no network anyway.
    st0 = L.cmd("state")
    if not st0: raise SystemExit("leader CLI silent: power-cycle the CC1354P10 and reload ot-cli-ftd")
    if any(x == "leader" for x in st0):
        note("leader already up; reusing its network")
    else:
        L.cmd("dataset init new"); L.cmd("dataset channel 15"); L.cmd("dataset commit active"); L.cmd("ifconfig up")
    t0 = time.time(); L.cmd("thread start")
    role_l = None
    for _ in range(45):
        st = L.cmd("state", 0.8); role_l = next((s for s in st if s in ("leader", "router", "child", "detached", "disabled")), None)
        if role_l == "leader": break
        time.sleep(1)
    t_leader = time.time() - t0; note(f"RESULT leader state {role_l} after {t_leader:.1f} s")
    if role_l != "leader": raise SystemExit("leader did not form")
    ds = L.cmd("dataset active -x", 2); hexds = next((l for l in ds if re.fullmatch(r"[0-9a-f]{40,}", l)), None)
    note(f"RESULT active dataset {'captured' if hexds else 'MISSING'} ({len(hexds or '')} hex chars)")
    # joiner gets the same dataset and attaches
    # factoryreset reboots the RCP; the Mac host reconnects over spinel, which takes seconds. Wait until the CLI
    # answers again before sending the dataset, then confirm it took (channel and PAN must match the leader).
    J.cmd("factoryreset", 3)
    for _ in range(20):
        if any("Done" in l for l in J.cmd("state", 1.0)): break
        time.sleep(1)
    J.cmd(f"dataset set active {hexds}", 2)
    chk = J.cmd("dataset active", 2); note(f"RESULT joiner dataset: {' | '.join(l for l in chk if l.startswith(('Channel','PAN ID','Network Name')))}")
    if not any(l.startswith("Channel: 15") for l in chk): raise SystemExit("joiner did not take the dataset")
    J.cmd("ifconfig up")
    t1 = time.time(); J.cmd("thread start")
    role = None
    for _ in range(60):
        st = J.cmd("state", 0.8); role = next((s for s in st if s in ("child", "router", "leader", "detached", "disabled")), None)
        if role in ("child", "router"): break
        time.sleep(1)
    t_attach = time.time() - t1; note(f"RESULT joiner role {role} after {t_attach:.1f} s")
    # addresses, then pings both ways (mesh-local EIDs)
    lrloc = next((l for l in L.cmd("ipaddr mleid") if ":" in l), None); jrloc = next((l for l in J.cmd("ipaddr mleid") if ":" in l), None)
    note(f"RESULT leader mleid {lrloc}  joiner mleid {jrloc}")
    L.cmd("router table", 2); L.cmd("child table", 2)
    if jrloc: 
        for i in range(3): J.cmd(f"ping {lrloc} 32 1", 3)
        for i in range(3): L.cmd(f"ping {jrloc} 32 1", 3)
    L.cmd("neighbor table", 2); J.cmd("neighbor table", 2)
finally:
    J.close(); os.close(L.fd); log.close()
