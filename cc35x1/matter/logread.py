"""Print a serial port's output at 115200 baud, 8N1, until Ctrl+C.

    python3 cc35x1/matter/logread.py /dev/cu.usbmodem<probe serial>1

On macOS the baud rate is set by the program that opens the port, so this sets it and reads in the same
process (stty in one command and cat in another reads at the wrong rate). Pass a different rate as the
second argument, for example 921600 for the Wi-Fi CSI presence program.
"""
import os
import sys
import termios

rate = getattr(termios, "B" + (sys.argv[2] if len(sys.argv) > 2 else "115200"))
fd = os.open(sys.argv[1], os.O_RDONLY | os.O_NOCTTY)
a = termios.tcgetattr(fd)
a[0] = a[1] = a[3] = 0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[4] = a[5] = rate
a[6][termios.VMIN], a[6][termios.VTIME] = 1, 0
termios.tcsetattr(fd, termios.TCSANOW, a)
try:
    while True:
        sys.stdout.buffer.write(os.read(fd, 256))
        sys.stdout.buffer.flush()
except KeyboardInterrupt:
    pass
