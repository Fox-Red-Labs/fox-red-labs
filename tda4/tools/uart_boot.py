import serial, subprocess, sys, time, datetime, os
PORTS=["/dev/cu.SLAB_USBtoUART","/dev/cu.SLAB_USBtoUART1","/dev/cu.SLAB_USBtoUART2","/dev/cu.SLAB_USBtoUART4"]
log=open(f"uart_boot_py_{datetime.datetime.now():%H%M%S}.log","ab",buffering=0)
def say(m):
    line=f"[{datetime.datetime.now():%H:%M:%S}] {m}"; print(line,flush=True); log.write((line+"\n").encode())
ss={p:serial.Serial(p,115200,timeout=0) for p in PORTS}
def drain(s,secs):
    b=b""; t=time.time()
    while time.time()-t<secs:
        d=s.read(4096)
        if d: b+=d
        time.sleep(0.02)
    if b: log.write(b)
    return b
def waitC(cands,timeout):
    # return the port whose stream has settled into a run of 'C' (quiet 0.4 s, tail all C)
    t=time.time(); tails={p:b"" for p in cands}; last={p:0 for p in cands}
    while time.time()-t<timeout:
        for p in cands:
            d=ss[p].read(4096)
            if d:
                log.write(f"\n<{p}> ".encode()+d); tails[p]=(tails[p]+d)[-16:]; last[p]=time.time()
            tl=tails[p]
            if len(tl)>=3 and tl[-1] in b"C\x15" and set(tl)<= {ord("C"),0x15} and time.time()-last[p]>0.4:
                return p
        time.sleep(0.05)
    return None
def send(proto,f,p):
    s=ss[p]; say(f"sending {f} ({proto}) on {p}")
    t0=time.time(); fd=s.fileno()
    r=subprocess.run(["lsb",f"--{proto}",f],stdin=fd,stdout=fd,stderr=subprocess.PIPE)
    say(f"lsb rc={r.returncode} in {time.time()-t0:.0f} s: {r.stderr.decode(errors='replace').strip()[-300:]}")
    return r.returncode==0
mcu=waitC(PORTS,900)
if not mcu: say("no C from ROM"); sys.exit(1)
say(f"ROM C stream on {mcu} = MCU_UART0")
others=[p for p in PORTS if p!=mcu]
if not send("xmodem","tiboot3.bin",mcu): sys.exit(1)
say("tiboot3 sent; waiting for R5 SPL C (sysfw) on MCU_UART0")
if not waitC([mcu],60): say("no C for sysfw"); sys.exit(1)
if not send("ymodem","sysfw.itb",mcu): sys.exit(1)
say("sysfw sent; waiting for C on another port (MAIN_UART0, tispl)")
main=waitC(others,90)
if not main: say("no C on MAIN"); sys.exit(1)
say(f"MAIN_UART0 is {main}")
if not send("ymodem","tispl.bin",main): sys.exit(1)
say("tispl sent; waiting for A72 SPL C (u-boot.img) on MAIN")
if not waitC([main],90): say("no C for u-boot.img"); sys.exit(1)
if not send("ymodem","u-boot.img",main): sys.exit(1)
say("u-boot.img sent; tapping keys to stop autoboot")
out=b""; t=time.time()
while time.time()-t<25:
    ss[main].write(b" "); d=ss[main].read(65536)
    if d: out+=d; log.write(d)
    if b"=>" in out[-200:]: break
    time.sleep(0.2)
print(out.decode(errors="replace")[-1200:])
open("ports.txt","w").write(f"MCU={mcu}\nMAIN={main}\n")
say("done")
