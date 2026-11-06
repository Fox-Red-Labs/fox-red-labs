#!/usr/bin/env python3
"""Xmodem-CRC sender (128-byte SOH or 1024-byte STX packets) for testing a UART bootloader.

  xsend.py PORT IMAGE.bin [--baud 115200] [--block 128|1024] [--corrupt N[,N...]]

Waits for the receiver's 'C', sends IMAGE padded with 0x1A to whole packets, prints each reply byte
(ACK 06, NAK 15, CAN 18), sends EOT and prints the text the board sends afterwards.
--corrupt: flip a CRC byte of those packet numbers (1-based) on the first attempt only (tests the receiver's NAK path).
Imported by check.py as a module; needs pyserial.
"""
import argparse, sys, time
import serial

SOH, STX, EOT, ACK, NAK, CAN, C = 1, 2, 4, 6, 0x15, 0x18, 0x43

def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

def read_for(s, secs):
    t0 = time.time(); buf = b""
    while time.time() - t0 < secs:
        n = s.in_waiting
        if n: buf += s.read(n)
        else: time.sleep(0.002)
    return buf

def wait_reply(s, secs):
    """next reply byte that is not a stray 'C' (the receiver repeats 'C' until the first packet arrives)."""
    t0 = time.time()
    while time.time() - t0 < secs:
        b = s.read(1)
        if b and b[0] != C: return b[0]
    return None

def packets(data, block):
    data = data + b"\x1a" * (-len(data) % block)
    return [data[i:i + block] for i in range(0, len(data), block)]

def frame(n, chunk, bad_crc=False):
    c = crc16(chunk)
    if bad_crc: c ^= 0x0100
    return bytes([SOH if len(chunk) == 128 else STX, n & 0xFF, 255 - (n & 0xFF)]) + chunk + bytes([c >> 8, c & 0xFF])

def wait_c(s, secs=20):
    t0 = time.time()
    while time.time() - t0 < secs:
        b = s.read(1)
        if b and b[0] == C: return True
    return False

def send(s, data, block=128, corrupt=(), log=None, reply_timeout=20, stop_after_replies=None):
    """returns dict: replies (list of bytes), acked, eot_ack, aborted. stop_after_replies: stop at that many replies."""
    log = log if log is not None else []
    res = {"replies": log, "acked": 0, "eot_ack": False, "aborted": False, "no_c": False}
    if not wait_c(s): res["no_c"] = True; return res
    tried = set()
    for n, chunk in enumerate(packets(data, block), 1):
        while True:
            bad = n in corrupt and n not in tried
            tried.add(n)
            s.write(frame(n, chunk, bad)); s.flush()
            r = wait_reply(s, reply_timeout); log.append(r)
            if r == ACK: res["acked"] += 1; break
            if r == NAK: continue
            res["aborted"] = True; return res
    s.write(bytes([EOT])); s.flush()
    r = wait_reply(s, 5); log.append(r); res["eot_ack"] = (r == ACK)
    return res

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("port"); ap.add_argument("image")
    ap.add_argument("--baud", type=int, default=115200); ap.add_argument("--block", type=int, default=128, choices=(128, 1024))
    ap.add_argument("--corrupt", default="")
    a = ap.parse_args()
    s = serial.Serial(a.port, a.baud, timeout=0.05)
    bad = {int(x) for x in a.corrupt.split(",") if x}
    r = send(s, open(a.image, "rb").read(), a.block, bad)
    print({k: (bytes(x for x in v if x is not None).hex() if k == "replies" else v) for k, v in r.items()})
    print(read_for(s, 4).decode("latin1"))
    return 0 if r["eot_ack"] else 1

if __name__ == "__main__":
    sys.exit(main())
