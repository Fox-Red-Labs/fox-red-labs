#!/usr/bin/env python3
"""The reverse-path check: TI's own `ot-cli-ftd` on the CC1354P10's on-chip radio, joined to the same home Thread
network, pinging out and being pinged. If pings out of this image work where the border-router host's did not, the
difference is the RCP link; if they fail the same way, it is the stack or the border router.

  python3 ftd_reverse_test.py <ot-cli-ftd.out> [network name]

Loads the image hands-free (uhubctl power cycle of the CC1354's hub port, then tools/ccs_load_retry.py), reads the
home network's dataset from Home Assistant (never logged), joins, then: 3 rounds of 10 pings to the parent, 3 rounds
to this Mac, and 30 pings from the Mac to the board through the Apple TV. Leaves the board on ot-cli-ftd.
Results in out/border-router-m1/ftd-<time>/.
"""
import asyncio
import fcntl
import json
import os
import pathlib
import re
import select
import statistics
import struct
import subprocess
import sys
import termios
import time

import websockets

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PORT = "/dev/cu.usbmodemLS45013A1"
CCXML = ROOT / "cc1354p10/CC1354P10.ccxml"
CCS_RUN = "/Applications/ti/ccs2101/ccs/scripting/python/run.sh"
HUB, HUBPORT = "20-4", "1"

image = pathlib.Path(sys.argv[1]).resolve()
want = sys.argv[2] if len(sys.argv) > 2 else os.environ.get("FRL_THREAD_NETWORK") or sys.exit("give the Thread network name (argument 2 or FRL_THREAD_NETWORK)")
RUN = pathlib.Path.cwd() / "out/border-router-m1" / time.strftime("ftd-%Y%m%d-%H%M%S")
RUN.mkdir(parents=True)
LOG = open(RUN / "ftd.log", "a")
T0 = time.time()


def log(msg):
    line = f"{time.strftime('%H:%M:%S')} {time.time() - T0:7.1f} {msg}"
    print(line, flush=True)
    LOG.write(line + "\n")
    LOG.flush()


def sh(args, timeout=600):
    p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def load_image():
    log(f"power-cycling hub {HUB} port {HUBPORT} and loading {image.name}")
    sh(["uhubctl", "-l", HUB, "-p", HUBPORT, "-a", "off"])
    time.sleep(6)
    sh(["uhubctl", "-l", HUB, "-p", HUBPORT, "-a", "on"])
    for _ in range(40):
        if os.path.exists(PORT):
            break
        time.sleep(0.5)
    time.sleep(1)
    rc, out = sh([CCS_RUN, str(HERE / "tools/ccs_load_retry.py"), str(CCXML), str(image), "60"])
    ok = "running" in out
    log(f"load {'ok' if ok else 'FAILED'}: {out.strip().splitlines()[-1][:100] if out.strip() else ''}")
    if not ok:
        sys.exit(1)
    time.sleep(4)


def ha_tlv():
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
    base = re.sub(r"/mcp_server/sse$", "", mcp["url"])
    token = mcp["headers"]["Authorization"].split()[-1]

    async def go():
        async with websockets.connect(base.replace("http", "ws", 1) + "/api/websocket") as ws:
            await ws.recv()
            await ws.send(json.dumps({"type": "auth", "access_token": token}))
            await ws.recv()
            await ws.send(json.dumps({"id": 1, "type": "thread/list_datasets"}))
            r = json.loads(await ws.recv())
            d = [x for x in r["result"]["datasets"] if x["network_name"] == want][0]
            await ws.send(json.dumps({"id": 2, "type": "thread/get_dataset_tlv", "dataset_id": d["dataset_id"]}))
            return json.loads(await ws.recv())["result"]["tlv"]

    return asyncio.run(go())


fd = None


def open_console():
    global fd
    fd = os.open(PORT, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd)
    a[4] = a[5] = termios.B115200
    a[2] |= termios.CREAD | termios.CLOCAL
    a[3] = 0; a[1] = 0; a[0] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    fcntl.ioctl(fd, 0x80045402, struct.pack("L", 921600))
    termios.tcflush(fd, termios.TCIOFLUSH)


def cmd(c, wait=1.5, quiet=False):
    termios.tcflush(fd, termios.TCIFLUSH)
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
    lines = [l for l in buf.decode("latin-1").replace("\r", "").split("\n") if l.strip() and not l.startswith("> ") and not l.startswith("[")]
    if lines and lines[0].strip() == c.strip():
        lines = lines[1:]
    if quiet:
        log(f"> {c.split()[0]} ... -> {lines[-1] if lines else '(no reply)'}")
    else:
        log(f"> {c}\n" + "\n".join("  " + l for l in lines))
    return lines


def main():
    load_image()
    open_console()
    v = cmd("version")
    if not v:
        sys.exit("console does not answer")
    tlv = ha_tlv()
    cmd("thread stop"); cmd("ifconfig down")
    cmd(f"dataset set active {tlv}", quiet=True)
    cmd("ifconfig up")
    t0 = time.time()
    cmd("thread start")
    st = ""
    while time.time() - t0 < 120:
        l = cmd("state", 1.0)
        st = l[0].strip() if l else ""
        if st in ("child", "router", "leader"):
            break
        time.sleep(0.5)
    log(f"state {st} after {time.time() - t0:.1f} s")
    cmd("parent"); cmd("rloc16")
    addrs = cmd("ipaddr")
    mleid = (cmd("ipaddr mleid", 1.0) or [""])[0].strip()
    ml = ":".join(mleid.split(":")[:4])
    omr = next((l.strip() for l in addrs if re.match(r"^[0-9a-f:]+$", l.strip()) and not l.strip().startswith("fe80") and not l.strip().startswith(ml)), None)
    parent_rloc = next((l.split(":")[1].strip() for l in cmd("parent", 1.5) if l.strip().startswith("Rloc")), None)
    mac = subprocess.run(["ifconfig", "en0"], capture_output=True, text=True).stdout
    gua = next((l.split()[1] for l in mac.splitlines() if "inet6" in l and "fe80" not in l and "temporary" not in l and "deprecated" not in l), None)
    results = []
    for name, dst in (("parent (mesh)", f"{ml}:0:ff:fe00:{parent_rloc}" if parent_rloc else None), ("this Mac", gua)):
        if not dst:
            continue
        got = sent = 0; rtts = []
        for _ in range(3):
            txt = " ".join(cmd(f"ping {dst} 32 10 1", 14.0))
            m = re.search(r"(\d+) packets transmitted, (\d+) packets received", txt)
            if m:
                sent += int(m.group(1)); got += int(m.group(2))
            r = re.search(r"Round-trip min/avg/max = ([0-9.]+)/([0-9.]+)/([0-9.]+)", txt)
            if r:
                rtts.append(float(r.group(2)))
        results.append(f"board to {name}: {got} of {sent}" + (f", avg {statistics.mean(rtts):.0f} ms" if rtts else ""))
    if omr:
        p = subprocess.run(["ping6", "-c", "30", "-i", "0.3", omr], capture_output=True, text=True)
        results.append("Mac to board: " + " ".join(p.stdout.strip().splitlines()[-2:]))
    log("RESULTS: " + " | ".join(results))
    (RUN / "RESULTS.txt").write_text("\n".join(results) + "\n")


if __name__ == "__main__":
    main()
