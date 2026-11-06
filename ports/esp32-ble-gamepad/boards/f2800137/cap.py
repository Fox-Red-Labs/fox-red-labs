import os, sys, termios, time, select
fd = os.open("/dev/cu.usbmodemCL3710811", os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
a[4] = a[5] = termios.B115200
a[6][termios.VMIN] = 0; a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
end = time.time() + float(sys.argv[1]); out = b""
while time.time() < end:
    if select.select([fd], [], [], 0.2)[0]:
        out += os.read(fd, 4096)
sys.stdout.buffer.write(out)
