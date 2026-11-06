import os,termios,time,select,sys
fd=os.open('/dev/cu.usbmodemMG0000011',os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
a=termios.tcgetattr(fd)
a[0]=0;a[1]=0;a[3]=0;a[2]=termios.CS8|termios.CLOCAL|termios.CREAD;a[4]=a[5]=termios.B115200
termios.tcsetattr(fd,termios.TCSANOW,a)
t=time.time(); b=b''
while time.time()-t<float(sys.argv[1]):
    r,_,_=select.select([fd],[],[],0.5)
    if r: b+=os.read(fd,4096)
sys.stdout.write(b.decode(errors='replace'))
