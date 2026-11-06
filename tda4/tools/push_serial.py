#!/usr/bin/env python3
"""Push a local file to the SK-TDA4VM over the Linux console as base64 (76-column lines), then check sha256.
Usage: push.py <local file> <remote path>"""
import serial, sys, time, re, base64, hashlib, os
PORT=__import__("os").environ.get("TDA4_PORT","/dev/cu.SLAB_USBtoUART1")
src, dst = sys.argv[1], sys.argv[2]
data=open(src,"rb").read(); sha=hashlib.sha256(data).hexdigest()
b64=base64.b64encode(data).decode(); lines=[b64[i:i+76] for i in range(0,len(b64),76)]
s=serial.Serial(PORT,115200,timeout=0)
def rd(secs, stop=None):
    out=b""; t=time.time()
    while time.time()-t<secs:
        d=s.read(65536)
        if d: out+=d
        if stop and re.search(stop,out[-120:]): break
        time.sleep(0.02)
    return out.decode(errors="replace")
s.write(b"\x03"); time.sleep(0.3); s.write(b"\n"); r=rd(2.5, rb"# $|login: $")
if "login:" in r: s.write(b"root\n"); rd(5, rb"# $")
s.write(b"stty -echo\n"); rd(1, rb"# $")
s.write(f"base64 -d > {dst}.b64tmp << 'EOF_B64'\n".encode()); time.sleep(0.2)
t0=time.time(); sent=0
for i,l in enumerate(lines):
    s.write((l+"\n").encode()); sent+=len(l)+1
    if i%20==19: time.sleep(0.03)   # the console echo is off; keep the UART FIFO happy
    s.read(65536)
s.write(b"EOF_B64\n"); time.sleep(0.5)
s.write(b"stty echo\n"); rd(2, rb"# $")
s.write(f"mv {dst}.b64tmp {dst}; sha256sum {dst}\n".encode()); out=rd(10, rb"\n[^\n]*# $")
ok = sha in out
print(f"{src} -> {dst}: {len(data)} bytes, {len(lines)} lines in {time.time()-t0:.1f} s; sha256 {'OK' if ok else 'MISMATCH'}")
if not ok: print(out); sys.exit(1)
