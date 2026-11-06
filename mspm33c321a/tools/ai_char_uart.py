#!/usr/bin/env python3
"""TI's character_recognition example waits for 784 floats (a 28 x 28 picture) on the board's serial port at 115200
baud and answers with 10 floats (the digit scores), then two times in ms. Flash it, send one hand-made vertical stroke
(a '1'), read the answer, then read the cycle counter struct `ai` from RAM.   ai_char_uart.py <image.out> [port]
Only the port of probe M3010001 is opened."""
import os, select, struct, subprocess, sys, termios, time
HERE = os.path.dirname(os.path.abspath(__file__))
img = sys.argv[1]; port = sys.argv[2] if len(sys.argv) > 2 else "/dev/cu.usbmodemM30100011"
subprocess.run([sys.executable, os.path.join(HERE, "run_image.py"), img, "5", "--until", "ai=0xa1c0de01"], check=True)
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd); a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
a[4] = a[5] = termios.B115200; a[6][termios.VMIN] = 0; a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
img784 = [0.0] * 784
for y in range(4, 24):
    for x in (13, 14, 15): img784[y * 28 + x] = 1.0
data = b"".join(struct.pack("<f", v) for v in img784); i = 0
while i < len(data):
    i += os.write(fd, data[i:i + 64]); termios.tcdrain(fd)
buf = b""; t = time.time()
while time.time() - t < 3 and len(buf) < 48:
    if select.select([fd], [], [], 0.2)[0]: buf += os.read(fd, 64)
os.close(fd)
f = struct.unpack("<12f", buf[:48]) if len(buf) >= 48 else None
print("scores", f[:10] if f else None); print("model_ms_own_timer", f[10] if f else None, "full_ms_own_timer", f[11] if f else None)
print("class", max(range(10), key=lambda k: f[k]) if f else None)
subprocess.run([sys.executable, os.path.join(HERE, "run_image.py"), img, "1", "ai:32", "--no-flash"], check=True)
