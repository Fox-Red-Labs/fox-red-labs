#!/usr/bin/env python3
"""Host check for any port of MaJerle/stm32-usart-uart-dma-rx-tx (usart_rx_idle_line_irq_ringbuff_tx).

The same script runs against every board, so a result is comparable. It reads only what the board sends on its
serial console; nothing here knows which chip answers.

  check.py PORT [--baud 115200] [--reset "shell command"] [--tests banner,packet,junk,chunks,burst,latency] [--json out.json]

Contract the firmware keeps (observable behaviour of the upstream example):
  banner   after reset the board sends two lines:
             "USART DMA example: DMA HT & TC + USART IDLE LINE interrupts\\r\\n"
             "Start sending data to STM32\\r\\n"
  packet   START 0x55, CMD, LEN, LEN data bytes, STOP 0xAA  ->  reply 55 FF 00 AA (4 bytes)
           anything else (wrong STOP byte) gives no reply
Exit code 0 when every selected test passes.
"""
import argparse, json, random, statistics, subprocess, sys, time
import serial

BANNER = b"USART DMA example: DMA HT & TC + USART IDLE LINE interrupts\r\nStart sending data to STM32\r\n"
OK = bytes([0x55, 0xFF, 0x00, 0xAA])

def pkt(cmd=1, data=b"", stop=0xAA):
    return bytes([0x55, cmd, len(data)]) + data + bytes([stop])

def read_for(s, secs, want=None):
    t0 = time.time(); buf = b""
    while time.time() - t0 < secs:
        n = s.in_waiting
        if n:
            buf += s.read(n)
            if want is not None and len(buf) >= want:
                break
        else:
            time.sleep(0.0005)
    return buf

def t_banner(s, a):
    s.reset_input_buffer()
    if a.reset:
        subprocess.run(a.reset, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
    buf = read_for(s, 3.0, want=len(BANNER))
    ok = BANNER in buf
    return ok, {"got_bytes": len(buf), "text": buf[:120].decode("latin1")}

def t_packet(s, a):
    res = {}
    ok = True
    for name, p in (("len1", pkt(1, b"\xff")), ("len0", pkt(2)), ("len8", pkt(3, bytes(range(8))))):
        s.reset_input_buffer(); s.write(p)
        r = read_for(s, 0.6, want=4)
        res[name] = r.hex()
        ok &= (r == OK)
    return ok, res

def t_junk(s, a):
    s.reset_input_buffer(); s.write(pkt(1, b"\x00", stop=0xBB)); r = read_for(s, 0.4)
    s.write(pkt(1, b"\x01"))  # a good one afterwards must still be answered
    r2 = read_for(s, 0.6, want=4)
    return (r == b"" and r2 == OK), {"after_bad_stop": r.hex(), "next_good": r2.hex()}

def t_chunks(s, a):
    rnd = random.Random(7); s.reset_input_buffer()
    n = 200; sent = 0; buf = b""
    for i in range(n):
        p = pkt(rnd.randrange(256), bytes(rnd.randrange(256) for _ in range(rnd.randrange(0, 9))))
        j = 0
        while j < len(p):
            k = rnd.randrange(1, len(p) - j + 1)
            s.write(p[j:j + k]); j += k
            time.sleep(rnd.choice((0.0, 0.0, 0.003, 0.02)))
        sent += 1
        buf += read_for(s, 0.02)
    buf += read_for(s, 1.0, want=4 * n - len(buf))
    got = len(buf) // 4
    return (buf == OK * n), {"sent": n, "replies": got, "bytes": len(buf)}

def t_burst(s, a):
    n = 500; s.reset_input_buffer()
    blob = pkt(1, b"\x07") * n   # 5 bytes each (cmd 1, len 1, one data byte -> 5 bytes)
    t0 = time.time(); s.write(blob); s.flush()
    buf = b""; deadline = t0 + 4 + len(blob) * 10 / a.baud * 2
    last = time.time()
    while time.time() < deadline and len(buf) < 4 * n:
        k = s.in_waiting
        if k: buf += s.read(k); last = time.time()
        elif time.time() - last > 0.5 and buf: break
        else: time.sleep(0.0005)
    t1 = last
    replies = len(buf) // 4
    dt = t1 - t0
    return (replies == n and buf == OK * n), {"sent": n, "replies": replies, "lost": n - replies,
        "seconds": round(dt, 4), "packets_per_s": round(replies / dt, 1) if dt > 0 else None,
        "line_limit_pps": round(a.baud / 10 / len(pkt(1, b"\x07")), 1)}

def t_latency(s, a):
    ts = []; ok = True
    for i in range(100):
        s.reset_input_buffer(); t0 = time.perf_counter(); s.write(pkt(1, b"\x01")); s.flush()
        buf = b""
        while len(buf) < 4 and time.perf_counter() - t0 < 0.5:
            k = s.in_waiting
            if k: buf += s.read(k)
        if buf != OK: ok = False; continue
        ts.append((time.perf_counter() - t0) * 1000); time.sleep(0.02)
    if not ts: return False, {"ok": 0}
    ts.sort()
    return ok, {"n": len(ts), "median_ms": round(statistics.median(ts), 2), "p95_ms": round(ts[int(len(ts) * .95) - 1], 2),
                "min_ms": round(ts[0], 2), "frame_ms": round(10 * 9 / a.baud * 1000, 2)}

TESTS = dict(banner=t_banner, packet=t_packet, junk=t_junk, chunks=t_chunks, burst=t_burst, latency=t_latency)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("port"); ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--reset"); ap.add_argument("--tests", default="banner,packet,junk,chunks,burst,latency")
    ap.add_argument("--json"); a = ap.parse_args()
    s = serial.Serial(a.port, a.baud, timeout=0, write_timeout=5)
    out = {"port": a.port, "baud": a.baud, "tests": {}}; allok = True
    for name in a.tests.split(","):
        try: ok, info = TESTS[name](s, a)
        except Exception as e: ok, info = False, {"error": repr(e)}
        allok &= ok; out["tests"][name] = {"pass": bool(ok), **info}
        print(("PASS " if ok else "FAIL ") + name, json.dumps(info))
        time.sleep(0.2)
    print("RESULT", "PASS" if allok else "FAIL")
    if a.json: (__import__("os").makedirs(__import__("os").path.dirname(a.json) or ".", exist_ok=True), json.dump(out, open(a.json, "w"), indent=1))
    sys.exit(0 if allok else 1)

if __name__ == "__main__":
    main()
