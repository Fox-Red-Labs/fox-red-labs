import serial, sys, time, re, datetime
MAIN=open("ports.txt").read().split("MAIN=")[1].strip() if False else "/dev/cu.SLAB_USBtoUART1"
MODE=sys.argv[1] if len(sys.argv)>1 else "unzip"   # unzip | gzwrite
LAST=int(sys.argv[2]) if len(sys.argv)>2 else 30
s=serial.Serial(MAIN,115200,timeout=0)
log=open(f"write_from_stick_{datetime.datetime.now():%H%M%S}.log","ab",buffering=0)
def cmd(c,done=re.compile(rb"=> ?$"),timeout=600,quiet=False):
    s.write((c+"\n").encode()); out=b""; t=time.time()
    while time.time()-t<timeout:
        d=s.read(65536)
        if d: out+=d; log.write(d); t2=time.time()
        if done.search(out[-6:]) and len(out)>len(c): break
        time.sleep(0.02)
    txt=out.decode(errors="replace")
    if not quiet: print(txt[-400:].strip().splitlines()[-3:] if txt.strip() else "", flush=True)
    return txt
s.write(b"\x03\n"); time.sleep(0.5); s.read(65536)
print(cmd("usb reset",timeout=30)[-300:])
print(cmd("ls usb 0:1 /tda4",timeout=30)[-600:])
cmd("mmc dev 1")
t0=time.time()
for i in list(range(0,31))+[32,40,41,48]:
    name=f"wic.{i:03d}.gz"; blk=i*524288
    r=cmd(f"load usb 0:1 0x82000000 /tda4/{name}",timeout=300)
    m=re.search(r"(\d+) bytes read",r)
    if not m: print("LOAD FAILED",name,r[-300:]); sys.exit(1)
    if MODE=="gzwrite":
        r=cmd(f"gzwrite mmc 1 0x82000000 {m.group(1)} 0 {blk*512:#x}",timeout=600)
        ok="rror" not in r
    else:
        r=cmd("unzip 0x82000000 0xB0000000",timeout=600)
        m2=re.search(r"Uncompressed size: (\d+)",r)
        if not m2: print("UNZIP FAILED",name,r[-300:]); sys.exit(1)
        size=int(m2.group(1)); cnt=(size+511)//512
        r=cmd(f"mmc write 0xB0000000 {blk:#x} {cnt:#x}",timeout=600)
        ok=f"{cnt} blocks written: OK" in r
    print(f"[{time.time()-t0:5.0f}s] {name} -> blk {blk:#x} {'OK' if ok else 'FAIL'}",flush=True)
    if not ok: print(r[-400:]); sys.exit(1)
print("all non-zero chunks written; all-zero chunks skipped")
print(cmd("mmc part",timeout=30)[-500:])
