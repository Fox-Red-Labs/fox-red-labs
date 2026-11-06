import asyncio, time, sys
from bleak import BleakScanner, BleakClient
async def main():
    dev = await BleakScanner.find_device_by_name("Data Stream", timeout=15)
    print("found", dev)
    t = time.time()
    async with BleakClient(dev) as c:
        print(f"connected + services in {time.time()-t:.2f} s, mtu {c.mtu_size}")
        for s in c.services:
            print("S", s.uuid)
            for ch in s.characteristics: print("  C", ch.uuid, ch.properties, ch.handle)
asyncio.run(main())
