#!/usr/bin/env python3
"""Mac as BLE central against TI's data_stream example on the LP-EM-CC2755P10 (bleak).
Connect + service discovery time, then echo round trips (write with response, wait for the echo notification;
the example flips the case of letters) at 20, 64 and 128 bytes, 50 each, content checked."""
import asyncio, statistics, time
from bleak import BleakScanner, BleakClient
W, N = "f000c0c1-0451-4000-b000-000000000000", "f000c0c2-0451-4000-b000-000000000000"
async def main():
    dev = await BleakScanner.find_device_by_name("Data Stream", timeout=15)
    print("found", dev)
    t = time.time(); got = asyncio.Queue()
    async with BleakClient(dev) as c:
        print(f"connected + services in {time.time()-t:.2f} s, mtu {c.mtu_size}")
        await c.start_notify(N, lambda _, d: got.put_nowait((time.time(), bytes(d))))
        for size in (20, 64, 128):
            msg = (b"HelloTI" * 30)[:size]; rtts = []; bad = 0
            for _ in range(50):
                await asyncio.sleep(0.3)                      # let any late fragment of the last echo arrive
                while not got.empty(): got.get_nowait()
                t0 = time.time(); await c.write_gatt_char(W, msg, response=True)
                d = b""
                while len(d) < size:                          # an echo longer than the notification size comes in pieces
                    t1, part = await asyncio.wait_for(got.get(), 3); d += part
                rtts.append((t1 - t0) * 1000)
                if d != msg.swapcase(): bad += 1
            print(f"echo {size:3d} B x50: rtt median {statistics.median(rtts):.1f} ms min {min(rtts):.1f} max {max(rtts):.1f}; wrong echoes {bad}; goodput {size*8/statistics.median(rtts):.1f} kbit/s each way")
asyncio.run(main())
