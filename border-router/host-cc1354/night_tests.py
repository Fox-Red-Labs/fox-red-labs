#!/usr/bin/env python3
"""Unattended Thread tests on the border-router host once it is joined to the house's Thread network.

  python3 night_tests.py [--soak SECONDS] [--out DIR]

Needs: the host image on the CC1354P10 with the CC2340R5 as RCP, joined to the Apple TV's network (join_home_network.py
does that), and a route on this Mac to the Thread prefix (see README: `sudo route add -inet6 ...`). Nothing on the bench
is touched; the three wires stay. Each test writes its raw log; the summary goes to <out>/SUMMARY.md.

Tests, in order:
  A  stack restart: `thread stop` / `thread start` three times, seconds to child or router each time
  B  ping soak from the Mac through the Apple TV: 1 ping per second for --soak seconds (default 3600), loss and the
     latency spread; `frl status` and `counters mac` every 10 minutes from the host's console
  C  UDP from the Mac to the host through the Apple TV: 200 datagrams at 20 per second for 32, 128, 512 and 1024 bytes,
     counted on the host's console (`udp` commands)
  D  pings out of the host: to its parent on the mesh and to this Mac, 10 each, three rounds (the open reverse-path
     question)
"""
import fcntl
import os
import re
import select
import socket
import statistics
import struct
import subprocess
import sys
import termios
import time
from pathlib import Path

PORT = "/dev/cu.usbmodemLS45013A1"
BAUD = 921600
IOSSIOSPEED = 0x80045402

args = sys.argv[1:]
soak = 3600
out = Path.cwd() / "out/border-router-m1" / time.strftime("night-%Y%m%d-%H%M%S")
if "--soak" in args:
    soak = int(args[args.index("--soak") + 1])
if "--out" in args:
    out = Path(args[args.index("--out") + 1])
out.mkdir(parents=True, exist_ok=True)
LOG = open(out / "night.log", "a")
T0 = time.time()
SUMMARY = []


def log(msg):
    line = f"{time.strftime('%H:%M:%S')} {time.time() - T0:8.1f} {msg}"
    print(line, flush=True)
    LOG.write(line + "\n")
    LOG.flush()


fd = os.open(PORT, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd)
a[4] = a[5] = termios.B115200
a[2] |= termios.CREAD | termios.CLOCAL
a[3] = 0; a[1] = 0; a[0] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
fcntl.ioctl(fd, IOSSIOSPEED, struct.pack("L", BAUD))
termios.tcflush(fd, termios.TCIOFLUSH)
pending = b""


def read_for(seconds):
    """Everything the console prints in the next `seconds`, as lines."""
    global pending
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                pending += os.read(fd, 65536)
            except BlockingIOError:
                pass
    text = pending.decode("latin-1").replace("\r", "")
    pending = b""
    return [l for l in text.split("\n") if l.strip()]


def cmd(c, wait=1.5):
    """Send one CLI command; return its reply lines (the echo and log lines removed), waiting up to `wait` for Done."""
    global pending
    os.write(fd, c.encode() + b"\r\n")
    end = time.time() + wait
    buf = b""
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                buf += os.read(fd, 65536)
            except BlockingIOError:
                pass
        if b"\nDone" in buf or b"\nError" in buf:
            break
    text = buf.decode("latin-1").replace("\r", "")
    lines = [l for l in text.split("\n") if l.strip() and not l.startswith("> ") and not l.startswith("[")]
    if lines and lines[0].strip() == c.strip():
        lines = lines[1:]
    return lines


def state():
    lines = cmd("state", 1.0)
    return lines[0].strip() if lines else "?"


def host_omr():
    lines = cmd("ipaddr", 1.5)
    mleid = (cmd("ipaddr mleid", 1.0) or [""])[0].strip()
    ml = ":".join(mleid.split(":")[:4])
    for l in lines:
        l = l.strip()
        if re.match(r"^[0-9a-f:]+$", l) and not l.startswith("fe80") and not l.startswith(ml):
            return l
    return None


def mac_gua():
    o = subprocess.run(["ifconfig", "en0"], capture_output=True, text=True).stdout
    for l in o.splitlines():
        if "inet6" in l and "fe80" not in l and "temporary" not in l and "deprecated" not in l:
            return l.split()[1]
    return None


def preflight():
    log(f"out {out}")
    routes = subprocess.run(["netstat", "-rn", "-f", "inet6"], capture_output=True, text=True).stdout
    if "fd7c:3830:129e::/64" not in routes:
        sys.exit("this Mac has no route to the Thread prefix; add it first (README)")
    v = cmd("version")
    log("host: " + (v[0] if v else "(no reply)"))
    if not v:
        sys.exit("host console does not answer")
    st = state()
    log(f"state {st}")
    if st not in ("child", "router", "leader"):
        sys.exit("host is not on the network; run join_home_network.py first")
    log("frl status: " + " ".join(cmd("frl status")))


def test_a_restart():
    times = []
    for run in range(1, 4):
        cmd("thread stop")
        time.sleep(2)
        t0 = time.time()
        cmd("thread start")
        st = ""
        while time.time() - t0 < 120:
            st = state()
            if st in ("child", "router", "leader"):
                break
            time.sleep(0.3)
        dt = time.time() - t0
        times.append((st, dt))
        log(f"A restart {run}: {st} after {dt:.1f} s")
        # let it settle so the next stop finds it attached
        time.sleep(10)
    SUMMARY.append(("A. stack restart (`thread stop` / `thread start`), seconds to attached",
                    ", ".join(f"{st} {dt:.1f} s" for st, dt in times)))


def test_b_soak(addr):
    log(f"B soak: ping6 {addr} every 1 s for {soak} s")
    raw = open(out / "soak-ping.txt", "w")
    p = subprocess.Popen(["ping6", "-i", "1", "-c", str(soak), addr], stdout=raw, stderr=subprocess.STDOUT, text=True)
    t0 = time.time()
    next_status = t0 + 600
    while p.poll() is None:
        time.sleep(5)
        if time.time() >= next_status:
            next_status += 600
            st = state()
            frl = " ".join(cmd("frl status"))
            mac = [l.strip() for l in cmd("counters mac", 2.0) if re.match(r"\s*(TxTotal|RxTotal|TxErr\w+|RxErr\w+|TxRetry):", l)]
            log(f"B status at {time.time() - t0:.0f} s: state {st}; {frl}; {' '.join(mac)}")
    raw.close()
    txt = open(out / "soak-ping.txt").read()
    rtts = [float(m) for m in re.findall(r"time=([0-9.]+) ms", txt)]
    m = re.search(r"(\d+) packets transmitted, (\d+) packets received", txt)
    sent, got = (int(m.group(1)), int(m.group(2))) if m else (0, len(rtts))
    if rtts:
        rtts_sorted = sorted(rtts)
        p50 = statistics.median(rtts)
        p95 = rtts_sorted[int(0.95 * (len(rtts_sorted) - 1))]
        p99 = rtts_sorted[int(0.99 * (len(rtts_sorted) - 1))]
        line = (f"{got} of {sent} answered ({100.0 * (sent - got) / max(sent, 1):.2f}% loss); "
                f"median {p50:.1f} ms, 95th {p95:.1f} ms, 99th {p99:.1f} ms, min {rtts_sorted[0]:.1f}, max {rtts_sorted[-1]:.1f}")
    else:
        line = "no replies"
    log("B result: " + line)
    SUMMARY.append((f"B. {soak} s of pings from the Mac through the Apple TV, 1 per second", line))


def test_c_udp(addr):
    results = []
    cmd("udp close")
    cmd("udp open")
    cmd("udp bind :: 1234")
    for size in (32, 128, 512, 1024):
        payload = bytes([0x41 + (i % 26) for i in range(size)])
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        read_for(0.5)  # drain
        n = 200
        t0 = time.time()
        for i in range(n):
            s.sendto(payload, (addr, 1234))
            time.sleep(0.05)
        sent_s = time.time() - t0
        lines = read_for(6.0)
        got = sum(1 for l in lines if f"{size} bytes from" in l)
        s.close()
        bps = got * size * 8 / max(sent_s, 0.001)
        results.append(f"{size} B: {got} of {n} arrived ({bps / 1000:.1f} kbit/s delivered at the 20 per second send rate)")
        log(f"C udp {size} B: {got} of {n}")
    cmd("udp close")
    SUMMARY.append(("C. UDP from the Mac to the host through the Apple TV, 200 datagrams at 20 per second", "; ".join(results)))


def test_d_outbound():
    parent = [l for l in cmd("parent", 1.5) if l.strip().startswith("Rloc")]
    rloc16 = parent[0].split(":")[1].strip() if parent else None
    mleid = (cmd("ipaddr mleid", 1.0) or [""])[0].strip()
    ml = ":".join(mleid.split(":")[:4])
    targets = []
    if rloc16:
        targets.append(("parent (mesh)", f"{ml}:0:ff:fe00:{rloc16}"))
    g = mac_gua()
    if g:
        targets.append(("this Mac (Wi-Fi)", g))
    results = []
    for name, dst in targets:
        got_total, sent_total, rtts = 0, 0, []
        for rnd in range(3):
            lines = cmd(f"ping {dst} 32 10 1", 14.0)
            txt = " ".join(lines)
            m = re.search(r"(\d+) packets transmitted, (\d+) packets received", txt)
            if m:
                sent_total += int(m.group(1)); got_total += int(m.group(2))
            r = re.search(r"Round-trip min/avg/max = ([0-9.]+)/([0-9.]+)/([0-9.]+)", txt)
            if r:
                rtts.append(float(r.group(2)))
            log(f"D {name} round {rnd + 1}: {txt[-120:]}")
        results.append(f"{name}: {got_total} of {sent_total}" + (f", average RTT {statistics.mean(rtts):.0f} ms" if rtts else ""))
    SUMMARY.append(("D. pings out of the host, 3 rounds of 10", "; ".join(results)))


def main():
    preflight()
    addr = host_omr()
    log(f"host off-mesh-routable address {addr}")
    if not addr:
        sys.exit("no off-mesh-routable address on the host")
    test_a_restart()
    time.sleep(5)
    addr = host_omr() or addr
    test_d_outbound()
    test_c_udp(addr)
    test_b_soak(addr)
    test_d_outbound()
    log("final frl status: " + " ".join(cmd("frl status")))
    with open(out / "SUMMARY.md", "w") as f:
        f.write(f"# Overnight Thread tests, {time.strftime('%Y-%m-%d')}, host joined to the Apple TV's network\n\n")
        f.write(f"Host address {addr}. Raw logs beside this file.\n\n| Test | Result |\n|---|---|\n")
        for k, v in SUMMARY:
            f.write(f"| {k} | {v} |\n")
    log(f"summary {out / 'SUMMARY.md'}")


if __name__ == "__main__":
    main()
