#!/usr/bin/env python3
"""Run ported MSPM0 apps on the LP-CC1354P10-1 next to TI's own CC1354 build of each example, then put
the renamed Project Zero ("Fox Red 1354") back. Same method as run_cc23.py: UART captured from before the
flash, then GPIO output (DOUT31_0) and output-enable (DOE31_0) read back.
Usage: run_cc1354.py <build dir> <restore .hex> <results jsonl> [seconds]
<build dir> holds baseline/<app>/... and from_mspm0_shim/<app>/... from to_f2.sh."""
import json, sys, time, pathlib, glob
import run_cc23 as r

HERE = pathlib.Path(__file__).resolve().parent
r.CCXML = HERE.parent.parent / "cc1354p10/CC1354P10.ccxml"
r.PORT = "/dev/cu.usbmodemLS45013A1"
DOUT, DOE = 0x40022080, 0x400220D0
APPS = ["empty", "gpiointerrupt", "spicontroller", "spiperipheral"]

bdir, restore, out = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
secs = float(sys.argv[4]) if len(sys.argv) > 4 else 6
scratch = pathlib.Path(out).with_suffix(".tmp.bin")
try:
    for app in APPS:
        for variant, sub in (("native", "baseline"), ("ported", "from_mspm0_shim")):
            img = glob.glob(str(bdir / sub / app / "freertos/ticlang/*.out"))[0]
            (ok, fsecs, flog), text = r.uart_during(lambda: r.flash(img), secs)
            regs = {}
            if ok:
                try:
                    regs = {"dout": r.read_mem(DOUT, 4, scratch).hex(), "doe": r.read_mem(DOE, 4, scratch).hex()}
                except RuntimeError as e:
                    regs = {"error": str(e)[-160:]}
            rec = {"app": app, "variant": variant, "flashed": ok, "flash_secs": fsecs, "uart": text[-1500:], "gpio": regs,
                   "flash_log": None if ok else flog, "when": time.strftime("%Y-%m-%d %H:%M:%S")}
            open(out, "a").write(json.dumps(rec) + "\n")
            print(app, variant, "flashed" if ok else "FLASH FAILED", repr(text[:100]), regs)
finally:
    ok, fsecs, flog = r.flash(restore)
    print("restore Fox Red 1354:", "ok" if ok else "FAILED " + flog, fsecs, "s")
