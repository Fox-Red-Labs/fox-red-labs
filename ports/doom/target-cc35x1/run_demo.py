#!/usr/bin/env python3
"""Reset the LP-EM-CC35X1, pick a demo over the probe's serial port, and log what the Doom image prints.
  python3 -I run_demo.py <demo 1|2|3> <log prefix> [seconds=120]
Writes <prefix>.raw (every byte received) and <prefix>.txt (text lines), and <prefix>-fNNNNN.ppm for each downscaled frame (80 x 50).
Needs pyserial (the venv at ~/.venvs/csi has it). Run it through the probe lock."""
import sys, time, subprocess, re, serial
demo, prefix = sys.argv[1], sys.argv[2]; secs = float(sys.argv[3]) if len(sys.argv) > 3 else 120
PROBE = 'E10000FQ'; PORT = '/dev/cu.usbmodem%s1' % PROBE
RST = '/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset'
s = serial.Serial(PORT, 921600, timeout=0.2)
s.reset_input_buffer()
subprocess.run([RST, '-s', PROBE], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
buf = bytearray(); lastsend = 0; t0 = time.time(); done = False
while time.time() - t0 < secs and not done:
    d = s.read(4096)
    if d: buf += d
    if b'PICK demo' in buf and b'RUN demo' not in buf and time.time() - lastsend > 0.3:
        s.write(demo.encode()); s.flush(); lastsend = time.time()
    if b'\nDONE' in buf or b'DONE\r' in buf: time.sleep(1); buf += s.read(4096); done = True
open(prefix + '.raw', 'wb').write(buf)
# frames: "FRM n 80 50\r\n" + 4000 bytes + "\r\nEND\r\n"
txt = bytearray(); i = 0; nfr = 0
pat = re.compile(rb'\r\nFRM (\d+) 80 50\r\n')
while True:
    m = pat.search(buf, i)
    if not m: txt += buf[i:]; break
    txt += buf[i:m.start()]
    px = buf[m.end():m.end() + 4000]
    if len(px) == 4000:
        rgb = bytearray()
        for b in px: rgb += bytes([(b >> 5) * 255 // 7, ((b >> 2) & 7) * 255 // 7, (b & 3) * 255 // 3])
        open('%s-f%05d.ppm' % (prefix, int(m.group(1))), 'wb').write(b'P6\n80 50\n255\n' + bytes(rgb)); nfr += 1
    i = m.end() + 4000 + 7
open(prefix + '.txt', 'wb').write(bytes(txt))
print(bytes(txt).decode('latin1').replace('\r', ''))
print('frames saved:', nfr, 'elapsed %.1f s' % (time.time() - t0), 'done' if done else 'NOT done')
