#!/usr/bin/env python3
"""Flash the image with DSLite, then time from DSLite exiting to the first advertisement seen by name.
  ~/.venvs/ble/bin/python host/flash_to_adv.py [runs]
The scan starts a moment after DSLite exits (Python start-up of the scanner is inside the number).
"""
import asyncio, subprocess, sys, time, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import nordic_test as n
HEX = pathlib.Path(__file__).resolve().parents[1] / "out" / "frl_nordic_cc2340r5.hex"
CCXML = pathlib.Path(__file__).resolve().parents[3] / "cc2340r5" / "CC2340R5.ccxml"

async def main(runs):
    for i in range(runs):
        t0 = time.perf_counter()
        subprocess.run(["/Applications/ti/uniflash_9.6.0/dslite.sh", "--mode", "flash", "-c", str(CCXML), "-e", "-f", "-u", str(HEX)],
                       check=True, capture_output=True)
        t1 = time.perf_counter()
        t2 = time.perf_counter()
        dev, adv, dt = await n.find(None, 20)
        t3 = time.perf_counter()
        print(f"run {i+1}: flash {t1-t0:.1f} s; first advertisement {t3-t1:.2f} s after DSLite exited ({dt:.2f} s of it inside the scan)", flush=True)

asyncio.run(main(int(sys.argv[1]) if len(sys.argv) > 1 else 3))
