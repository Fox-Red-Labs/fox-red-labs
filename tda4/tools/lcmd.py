import serial, sys, time, re, datetime
s=serial.Serial("/dev/cu.SLAB_USBtoUART1",115200,timeout=0)
log=open("linux_session.log","ab",buffering=0)
def rd(secs, stop=None):
    out=b""; t=time.time()
    while time.time()-t<secs:
        d=s.read(65536)
        if d: out+=d; log.write(d)
        if stop and re.search(stop,out[-80:]): break
        time.sleep(0.03)
    return out.decode(errors="replace")
s.write(b"\n"); r=rd(2)
if "login:" in r: s.write(b"root\n"); r=rd(4, rb"# $")
for c in sys.argv[1:]:
    s.write((c+"\n").encode()); out=rd(20, rb"\n[^\n]*# $")
    print(out.strip()); print("~~~")
