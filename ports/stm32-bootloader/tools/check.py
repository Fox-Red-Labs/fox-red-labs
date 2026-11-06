#!/usr/bin/env python3
"""Host check for any port of ferenc-nemeth/stm32-bootloader (UART Xmodem-CRC bootloader), run from the Mac over the console.

  check.py PORT [--baud 115200] --erase "cmd" --dump "cmd" --reset "cmd" [--json out.json] [--tests t1,t2,...]

The same script runs against every board; it reads the console and the application area, nothing else. Application area: 0x00010000
to the end of flash; test application images in images/ (v1 is 1.4 kB, v2 is 4.4 kB; each prints "APP OK v<n>", then "APP TICK" every 250 ms
from a timer interrupt, so a jump that leaves the vector table wrong is caught).

Contract the firmware keeps (the upstream bootloader's behaviour, plus one rule for a host with no button):
  prompt    in update mode it prints "Please send a new binary file with Xmodem protocol to update the firmware." and 'C' (0x43) until the first packet
  update    Xmodem-CRC, 128-byte (SOH) and 1024-byte (STX) packets; ACK each good packet; on EOT: ACK, "Firmware updated!", "Jumping to user application...", then runs the app
  flash     the packets' bytes are in flash at the application start afterwards
  persist   after a reset with a valid application it prints the banner, "Jumping to user application..." and runs the app
  badcrc    a packet with a wrong CRC gets NAK; the sender's retry is accepted
  abort     three errors in a row: CAN CAN, then "Failed... Please try again.", and the prompt again; a good transfer still works
  blank     with a blank application area (first word 0xFFFFFFFF) it stays in update mode (the host cannot press a button)
Exit 0 when every selected test passes.
"""
import argparse, json, os, subprocess, sys, tempfile, time
import serial
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xsend

HERE = os.path.dirname(os.path.abspath(__file__))
PROMPT = b"Please send a new binary file with Xmodem protocol to update the firmware."

def sh(cmd, timeout=90):
    return subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=timeout).returncode

def into_update(s, a):
    """blank the application area, restart; return the console bytes seen up to the first 'C'."""
    sh(a.erase)
    s.reset_input_buffer(); sh(a.reset)
    t0 = time.time(); buf = b""
    while time.time() - t0 < 8:
        buf += xsend.read_for(s, 0.3)
        if b"C" in buf.split(PROMPT)[-1] and PROMPT in buf: break
    return buf

def app_lines(s, version, secs=6, prefix=b""):
    buf = prefix + xsend.read_for(s, secs)
    return buf, (f"APP OK v{version}".encode() in buf and buf.count(b"APP TICK") >= 2)

def flash_equal(a, img, block):
    padded = img + b"\x1a" * (-len(img) % block)
    out = tempfile.mktemp(suffix=".bin")
    sh(f"{a.dump} {len(padded)} {out}")
    got = open(out, "rb").read() if os.path.exists(out) else b""
    if os.path.exists(out): os.remove(out)
    return got == padded

def t_prompt(s, a, st):
    buf = into_update(s, a)
    return (PROMPT in buf and b"C" in buf.split(PROMPT)[-1]), {"text": buf[:160].decode("latin1")}

def t_update(s, a, st):
    img = open(f"{HERE}/images/app-v1.bin", "rb").read()
    into_update(s, a)
    r = xsend.send(s, img, 128)
    tail = xsend.read_for(s, 1.5)
    buf, run = app_lines(s, 1, 6, tail)
    txt = buf
    ok = (r["acked"] == len(xsend.packets(img, 128)) and r["eot_ack"] and b"Firmware updated!" in txt
          and b"Jumping to user application..." in txt and run)
    st["fl1"] = flash_equal(a, img, 128)
    return ok, {"acked": r["acked"], "eot_ack": r["eot_ack"], "aborted": r["aborted"], "app_ran": run, "text": txt[:160].decode("latin1")}

def t_flash(s, a, st):
    return st.get("fl1", False), {}

def t_persist(s, a, st):
    s.reset_input_buffer(); sh(a.reset)
    buf, run = app_lines(s, 1, 7)
    buf = buf[buf.find(b"UART Bootloader"):] if b"UART Bootloader" in buf else buf
    return (b"UART Bootloader" in buf and b"Jumping to user application..." in buf and run), {"text": buf[:200].decode("latin1")}

def t_badcrc(s, a, st):
    img = open(f"{HERE}/images/app-v2.bin", "rb").read()
    into_update(s, a)
    log = []
    r = xsend.send(s, img, 1024, corrupt={2}, log=log)
    tail = xsend.read_for(s, 1.5)
    buf, run = app_lines(s, 2, 6, tail)
    naks = sum(1 for x in log if x == xsend.NAK)
    ok = (naks == 1 and r["eot_ack"] and run and flash_equal(a, img, 1024))
    return ok, {"naks": naks, "acked": r["acked"], "eot_ack": r["eot_ack"], "app_ran": run}

def t_abort(s, a, st):
    img = open(f"{HERE}/images/app-v1.bin", "rb").read()
    into_update(s, a)
    chunk = xsend.packets(img, 128)[0]
    seq = []
    for _ in range(3):
        s.write(xsend.frame(1, chunk, bad_crc=True)); s.flush()
        t0 = time.time(); got = b""
        while time.time() - t0 < 4 and not got.replace(b"C", b""):
            got += s.read(s.in_waiting or 1)
        seq.append(got.replace(b"C", b"")[:2])
    txt = xsend.read_for(s, 3.0)
    cans = seq[2][:2] == b"\x18\x18" or b"\x18\x18" in (seq[2] + txt[:4])
    ok = (seq[0][:1] == b"\x15" and seq[1][:1] == b"\x15" and cans and b"Failed... Please try again." in (seq[2] + txt) and PROMPT in txt)
    r = xsend.send(s, img, 128)
    buf, run = app_lines(s, 1, 7) if r["eot_ack"] else (b"", False)
    return (ok and r["eot_ack"] and run), {"replies": [x.hex() for x in seq], "failed_text": b"Failed... Please try again." in (seq[2] + txt),
                                           "prompt_again": PROMPT in txt, "recovery_ok": bool(r["eot_ack"] and run)}

TESTS = {"prompt": t_prompt, "update": t_update, "flash": t_flash, "persist": t_persist, "badcrc": t_badcrc, "abort": t_abort}

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("port"); ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--erase", required=True); ap.add_argument("--dump", required=True); ap.add_argument("--reset", required=True)
    ap.add_argument("--tests", default=",".join(TESTS)); ap.add_argument("--json")
    a = ap.parse_args()
    s = serial.Serial(a.port, a.baud, timeout=0.05)
    res = {}; st = {}; ok = True
    for name in a.tests.split(","):
        try: p, d = TESTS[name](s, a, st)
        except Exception as e: p, d = False, {"error": repr(e)}
        res[name] = {"pass": bool(p), **d}; ok &= bool(p)
        print(f"{name:8s} {'PASS' if p else 'FAIL'}  {json.dumps(d)[:300]}")
    n = sum(v["pass"] for v in res.values())
    print(f"RESULT: {'PASS' if ok else 'FAIL'} {n}/{len(res)}")
    if a.json: json.dump({"pass": ok, "passed": n, "total": len(res), "tests": res}, open(a.json, "w"), indent=1)
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
