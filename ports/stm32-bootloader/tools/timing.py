#!/usr/bin/env python3
"""Update time of the bootloader on the G3519: seconds from the first packet sent (after the receiver's 'C') to the ACK of EOT,
which includes the erase of the application area. Run after the check; three updates of each image.

  timing.py [PORT]     needs the board in a state where erase_app works (probe MG000001)
"""
import os, statistics, subprocess, sys, time
import serial
H = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, H)
import xsend
port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodemMG0000011"
s = serial.Serial(port, 115200, timeout=0.05)
res = {}
for name, blk in (("app-v1.bin", 128), ("app-v2.bin", 1024)):
    img = open(f"{H}/images/{name}", "rb").read(); ts = []
    for _ in range(3):
        subprocess.run([sys.executable, f"{H}/g3519.py", "erase"], stdout=subprocess.DEVNULL)
        s.reset_input_buffer(); subprocess.run([sys.executable, f"{H}/g3519.py", "reset"], stdout=subprocess.DEVNULL)
        buf = b""; t = time.time()
        while b"Xmodem protocol to update the firmware." not in buf and time.time() - t < 10: buf += xsend.read_for(s, 0.2)
        assert xsend.wait_c(s, 20)
        t0 = time.time(); r = xsend.send(s, img, blk); t1 = time.time()
        xsend.read_for(s, 1.0)
        assert r["eot_ack"], r
        ts.append(t1 - t0)
    res[name] = ts; print(name, blk, [round(t, 2) for t in ts], "median", round(statistics.median(ts), 2))
