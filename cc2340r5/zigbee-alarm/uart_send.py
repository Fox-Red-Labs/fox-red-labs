#!/usr/bin/env python3
"""Send one text line to the radio board's UART at 115200.  uart_send.py PORT "SMOKE 1" """
import os, sys, termios
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
a[4] = a[5] = termios.B115200; termios.tcsetattr(fd, termios.TCSANOW, a)
os.write(fd, (sys.argv[2] + "\n").encode()); termios.tcdrain(fd)
