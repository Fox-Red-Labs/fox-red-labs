#!/usr/bin/env python3
"""Pull a directory or file from the SK-TDA4VM over the console: tar+gzip+base64 on the board, decode here, sha256 check.
Usage: pull.py <remote path> <local.tgz>"""
import serial, sys, time, re, base64, hashlib
PORT=__import__("os").environ.get("TDA4_PORT","/dev/cu.SLAB_USBtoUART1")
remote, local = sys.argv[1], sys.argv[2]
s=serial.Serial(PORT,115200,timeout=0)
def rd(secs, stop=None):
    out=b""; t=time.time()
    while time.time()-t<secs:
        d=s.read(65536)
        if d: out+=d; t2=time.time()
        if stop and re.search(stop,out[-200:]): break
        time.sleep(0.02)
    return out
s.write(b"\x03"); time.sleep(0.3); s.write(b"\n"); r=rd(2.5, rb"# $|login: $")
if b"login:" in r: s.write(b"root\n"); rd(5, rb"# $")
# the markers are split in the command text so the console's echo of the command cannot match them
cmd=f"tar czf /tmp/_pull.tgz -C $(dirname {remote}) $(basename {remote}) && sha256sum /tmp/_pull.tgz | cut -c1-64 && echo BEGIN_B6''4 && base64 -w 76 /tmp/_pull.tgz && echo END_B6''4\n"
s.write(cmd.encode()); out=rd(600, rb"END_B64\r?\n")
txt=out.decode(errors="replace").replace("\r","")
m=re.search(r"([0-9a-f]{64})\nBEGIN_B64\n(.*?)\nEND_B64", txt, re.S)
if not m: print("no payload"); print(txt[-500:]); sys.exit(1)
data=base64.b64decode(m.group(2).replace("\n",""))
ok=hashlib.sha256(data).hexdigest()==m.group(1)
open(local,"wb").write(data)
print(f"{remote} -> {local}: {len(data)} bytes; sha256 {'OK' if ok else 'MISMATCH'}")
sys.exit(0 if ok else 1)
