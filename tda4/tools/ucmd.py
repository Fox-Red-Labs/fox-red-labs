import serial, sys, time
MAIN="/dev/cu.SLAB_USBtoUART1"
s=serial.Serial(MAIN,115200,timeout=0)
def rd(secs):
    b=b""; t=time.time()
    while time.time()-t<secs: b+=s.read(65536); time.sleep(0.03)
    return b
def cmd(c,secs=3):
    s.write((c+"\n").encode()); out=rd(secs)
    sys.stdout.write(out.decode(errors="replace")); sys.stdout.flush()
    open("uboot_session.log","ab").write(out)
    return out
for _ in range(3): s.write(b"\x03"); time.sleep(0.4)
rd(1.5); s.write(b"\n"); print(rd(1).decode(errors="replace")[-300:])
for c in sys.argv[1:]:
    cmd(c, 6 if c.startswith(("mmc info","usb")) else 3)
