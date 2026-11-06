#!/usr/bin/env python3
"""Join the border-router host (CC1354P10 + CC2340R5 RCP) to the Thread network already in the house, through the
border router that is already there (an Apple TV or HomePod), and prove it from the Mac on Wi-Fi.

  python3 join_home_network.py [network name]

Steps: read the Thread dataset from Home Assistant (websocket `thread/list_datasets`, then `thread/get_dataset_tlv`;
Home Assistant holds it once the phone app has synced the Apple credentials); hand it to the host over its console
with `dataset set active`; `ifconfig up`, `thread start`; wait for child or router; read the addresses; ping the
host's off-mesh-routable address from the Mac (the Apple border router forwards it); ping the Mac from the host;
register an SRP service and look for it with `dns-sd` on the Mac (the border router's advertising proxy publishes it).

The dataset carries the network key. It is never printed, never written to disk, and the console replies that would
echo it (`dataset active`) are never requested. Everything else is logged to out/border-router-m1/join-<time>/.
Home Assistant URL and token come from the home-assistant MCP entry in ~/.claude.json, as tools/ha.py does.
"""
import asyncio
import fcntl
import json
import os
import pathlib
import re
import select
import struct
import subprocess
import sys
import termios
import time

import websockets

PORT = "/dev/cu.usbmodemLS45013A1"
BAUD = 921600
IOSSIOSPEED = 0x80045402
RUN = pathlib.Path.cwd() / "out/border-router-m1" / time.strftime("join-%Y%m%d-%H%M%S")
RUN.mkdir(parents=True)
LOG = open(RUN / "join.log", "a")
T0 = time.time()


def log(msg):
    line = f"{time.time() - T0:8.3f} {msg}"
    print(line, flush=True)
    LOG.write(line + "\n")
    LOG.flush()


# ---- console -----------------------------------------------------------------------------------------------------
fd = os.open(PORT, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd)
a[4] = a[5] = termios.B115200
a[2] |= termios.CREAD | termios.CLOCAL
a[3] = 0; a[1] = 0; a[0] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
fcntl.ioctl(fd, IOSSIOSPEED, struct.pack("L", BAUD))
termios.tcflush(fd, termios.TCIOFLUSH)


def read_until_done(timeout=3.0):
    buf, end = b"", time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            buf += os.read(fd, 4096)
            if b"\nDone" in buf or b"\nError" in buf:
                time.sleep(0.05)
                r, _, _ = select.select([fd], [], [], 0.05)
                if r:
                    buf += os.read(fd, 4096)
                break
    text = buf.decode("latin-1").replace("\r", "")
    lines = [l for l in text.split("\n") if l.strip() and not l.startswith("> ") and not l.startswith("[")]
    return lines


def cmd(c, quiet=False, timeout=3.0):
    termios.tcflush(fd, termios.TCIFLUSH)
    os.write(fd, c.encode() + b"\r\n")
    lines = read_until_done(timeout)
    if lines and lines[0].strip() == c.strip():
        lines = lines[1:]
    if quiet:
        log(f"> {c.split()[0]} {c.split()[1] if len(c.split()) > 1 else ''} ... -> {lines[-1] if lines else '(no reply)'}")
    else:
        log(f"> {c}\n" + "\n".join("  " + l for l in lines))
    return lines


# ---- Home Assistant ------------------------------------------------------------------------------------------------
def ha_config():
    cfg = json.load(open(pathlib.Path.home() / ".claude.json"))

    def find(o):
        if isinstance(o, dict):
            if "home-assistant" in o and isinstance(o["home-assistant"], dict) and "url" in o["home-assistant"]:
                return o["home-assistant"]
            for v in o.values():
                r = find(v)
                if r:
                    return r

    mcp = find(cfg)
    if mcp is None:
        sys.exit("no home-assistant MCP entry in ~/.claude.json")
    base = re.sub(r"/mcp_server/sse$", "", mcp["url"])
    return base.replace("http", "ws", 1) + "/api/websocket", mcp["headers"]["Authorization"].split()[-1]


async def fetch_dataset(want_name):
    url, token = ha_config()
    async with websockets.connect(url, max_size=2**22) as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": token}))
        assert json.loads(await ws.recv())["type"] == "auth_ok"
        await ws.send(json.dumps({"id": 1, "type": "thread/list_datasets"}))
        r = json.loads(await ws.recv())
        datasets = r["result"]["datasets"]
        log("HA datasets: " + ", ".join(f"{d.get('network_name')} (source {d.get('source')}, channel {d.get('channel')}, "
                                        f"pan {d.get('pan_id')}, preferred {d.get('preferred')})" for d in datasets))
        pick = None
        for d in datasets:
            if want_name and d.get("network_name") == want_name:
                pick = d
        if pick is None:
            pick = next((d for d in datasets if d.get("preferred")), datasets[0] if datasets else None)
        if pick is None:
            sys.exit("Home Assistant holds no Thread dataset. Sync the Apple credentials from the phone app first.")
        await ws.send(json.dumps({"id": 2, "type": "thread/get_dataset_tlv", "dataset_id": pick["dataset_id"]}))
        r = json.loads(await ws.recv())
        tlv = r["result"]["tlv"]
        log(f"using dataset for {pick.get('network_name')} (channel {pick.get('channel')}, pan {pick.get('pan_id')}, "
            f"ext pan {pick.get('extended_pan_id')}); TLV length {len(tlv) // 2} bytes, not logged")
        return tlv, pick


def mac_ipv6(ifname="en0"):
    out = subprocess.run(["ifconfig", ifname], capture_output=True, text=True).stdout
    gua = [l.split()[1] for l in out.splitlines() if "inet6" in l and "fe80" not in l and "temporary" not in l and "deprecated" not in l]
    return gua


def main():
    want = sys.argv[1] if len(sys.argv) > 1 else None
    tlv, _info = asyncio.run(fetch_dataset(want))

    cmd("version")
    cmd("thread stop")
    cmd("ifconfig down")
    reply = cmd(f"dataset set active {tlv}", quiet=True)
    if not reply or reply[-1].strip() != "Done":
        sys.exit("dataset set active did not answer Done")
    cmd("ifconfig up")
    t_start = time.time()
    cmd("thread start")
    state = ""
    for _ in range(150):
        time.sleep(1.0)
        lines = cmd("state")
        state = lines[0].strip() if lines else ""
        if state in ("child", "router", "leader"):
            break
    dt = time.time() - t_start
    log(f"state {state} after {dt:.1f} s")
    if state == "leader":
        log("LEADER: the host formed its own network instead of joining; the Apple network was not heard "
            "(wrong channel or dataset, or out of range)")
    cmd("panid"); cmd("channel"); cmd("networkname"); cmd("extpanid"); cmd("rloc16")
    cmd("parent" if state == "child" else "router table")
    cmd("leaderdata")
    cmd("netdata show")
    cmd("neighbor table")
    addrs = cmd("ipaddr")
    mleid = (cmd("ipaddr mleid") or [""])[0].strip()
    ml_prefix = ":".join(mleid.split(":")[:4]) if mleid else "fd00"
    ipv6 = re.compile(r"^[0-9a-f:]+$")
    omr = [l.strip() for l in addrs if ipv6.match(l.strip()) and ":" in l and not l.strip().startswith("fe80")
           and not l.strip().startswith(ml_prefix)]
    log(f"mesh-local prefix {ml_prefix}; off-mesh-routable addresses: {omr}")

    # Mac -> host, through the Apple border router
    for addr in omr:
        p = subprocess.run(["ping6", "-c", "10", "-i", "0.5", addr], capture_output=True, text=True)
        tail = "\n".join(p.stdout.strip().splitlines()[-2:])
        log(f"Mac ping6 {addr}:\n  " + tail.replace("\n", "\n  "))

    # host -> Mac
    for gua in mac_ipv6()[:2]:
        cmd(f"ping {gua} 32 5 1", timeout=8.0)

    # SRP service, then look for it from the Mac
    cmd("srp client host name frl-cc1354-host")
    cmd("srp client host address auto")
    cmd("srp client service add frl-cc1354 _frl._udp 12345")
    cmd("srp client autostart enable")
    time.sleep(6)
    cmd("srp client host"); cmd("srp client service")
    try:
        p = subprocess.run(["dns-sd", "-B", "_frl._udp", "local"], capture_output=True, text=True, timeout=6)
        out = p.stdout
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode() if isinstance(e.stdout, bytes) else (e.stdout or "")
    hits = [l for l in out.splitlines() if "frl" in l.lower()]
    log("dns-sd -B _frl._udp on the Mac: " + (hits[0].strip() if hits else "nothing in 6 s"))
    cmd("counters mac")
    log(f"run dir {RUN}")


if __name__ == "__main__":
    main()
