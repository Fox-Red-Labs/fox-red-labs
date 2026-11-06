#!/usr/bin/env python3
"""Packet loss over a fixed time for the CC1407P weather-station link: logs both serial ports with
timestamps and, at the end, compares the receiver radio core's counters with the sender's count.
usage: loss_run.py <seconds> <outdir> [sender port] [receiver port]"""
import os, sys, termios, time, select, re, threading, json
secs = float(sys.argv[1]); out = sys.argv[2]
tx_port = sys.argv[3] if len(sys.argv) > 3 else "/dev/cu.usbmodemMG0200011"
rx_port = sys.argv[4] if len(sys.argv) > 4 else "/dev/cu.usbmodemLS45013A1"
os.makedirs(out, exist_ok=True)
def grab(port, path):
    fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0; a[0] = 0; a[1] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    t0 = time.time(); part = b""
    with open(path, "w") as f:
        while time.time() - t0 < secs:
            r, _, _ = select.select([fd], [], [], 0.5)
            if r:
                try: part += os.read(fd, 4096)
                except BlockingIOError: pass
                while b"\n" in part:
                    l, part = part.split(b"\n", 1)
                    f.write(f"{time.time():.2f} {l.decode('utf8', 'replace').strip()}\n"); f.flush()
    os.close(fd)
ts = [threading.Thread(target=grab, args=(tx_port, out + "/tx.log")), threading.Thread(target=grab, args=(rx_port, out + "/rx.log"))]
[t.start() for t in ts]; [t.join() for t in ts]
tx = [l for l in open(out + "/tx.log") if " tx FRL n=" in l]
# the XDS110 bridge replays old output when a port opens: skip anything logged in the first 3 s
t_open = min(float(l.split()[0]) for l in open(out + "/rx.log"))
cnt = [l for l in open(out + "/rx.log") if " count ok=" in l and float(l.split()[0]) > t_open + 3]
def counts(l): return {k: int(v) for k, v in re.findall(r"(ok|crc_bad|ignored)=(\d+)", l)}
def seq(l): return int(re.search(r"n=(\d+)", l).group(1))
# window: first and last receiver count lines; sender packets sent between those times (after skipping 2 s of buffered lines)
t0 = float(tx[0].split()[0]) + 3
txw = [l for l in tx if float(l.split()[0]) > t0]
c0, c1 = counts(cnt[0]), counts(cnt[-1]); tc0, tc1 = float(cnt[0].split()[0]), float(cnt[-1].split()[0])
sent = sum(1 for l in txw if tc0 < float(l.split()[0]) <= tc1)
notsent = sum(1 for l in txw if "NOT sent" in l)
got = c1["ok"] - c0["ok"]; bad = c1["crc_bad"] - c0["crc_bad"]
res = {"seconds": round(tc1 - tc0, 1), "sent": sent, "received_ok": got, "crc_bad": bad, "lost": sent - got,
       "loss_pct": round(100.0 * (sent - got) / sent, 3) if sent else None, "sender_not_sent": notsent}
json.dump(res, open(out + "/result.json", "w"), indent=1); print(json.dumps(res))
