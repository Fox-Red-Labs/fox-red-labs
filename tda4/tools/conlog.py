import serial, sys, time, datetime
port, out = sys.argv[1], sys.argv[2]
s = serial.Serial(port, 115200, timeout=0.5)
with open(out, "ab", buffering=0) as f:
    f.write(f"\n### opened {datetime.datetime.now().isoformat(timespec='seconds')} {port}\n".encode())
    while True:
        d = s.read(4096)
        if d:
            f.write(f"[{datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]}] ".encode() + d)
