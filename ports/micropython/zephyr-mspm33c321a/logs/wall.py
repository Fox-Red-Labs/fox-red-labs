import serial,time,sys
s=serial.Serial('/dev/cu.usbmodemM30100011',115200,timeout=0.05)
def cmd(c):
    s.write(c.encode()+b'\r'); t=time.time(); b=b''
    while time.time()-t<30:
        b+=s.read(200)
        if b.count(b'>>>')>=1 and b.endswith(b'>>> '): break
    return time.time()-t,b
s.write(b'\x03\x03'); time.sleep(.5); s.read(5000)
cmd('import time')
cmd('exec("def h(n):\\n for i in range(n): pass")')
for n in (100000,400000):
    dt,b=cmd('c=time.ticks_us(); h(%d); print(time.ticks_diff(time.ticks_us(),c))'%n)
    print(n, 'host wall %.3f s'%dt, b.decode(errors='replace').split('\r\n')[1:2])
