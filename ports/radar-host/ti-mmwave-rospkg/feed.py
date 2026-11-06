"""Write a recorded stream into a serial port, one frame every FEED_PERIOD seconds (default 0.25, the
radar's frame period at our cfg)."""
import os, sys, time
PERIOD = float(os.environ.get("FEED_PERIOD", "0.25"))
data = open(sys.argv[1], "rb").read(); magic = bytes([2, 1, 4, 3, 6, 5, 8, 7])
starts = [k for k in range(len(data)) if data.startswith(magic, k)] + [len(data)]
with open(sys.argv[2], "wb", buffering=0) as port:
    for a, b in zip(starts, starts[1:]):
        port.write(data[a:b]); time.sleep(PERIOD)
print(f"fed {len(starts) - 1} frames, one every {PERIOD} s")
