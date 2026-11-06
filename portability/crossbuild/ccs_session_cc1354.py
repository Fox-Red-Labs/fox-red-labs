"""One CCStudio scripting session for the LP-CC1354P10-1, so the chip is never released between runs.
DSLite needs a fresh connection per step, and the CC1354 refuses a second connection once a program has
run (error -242) until a power cycle. Run with CCStudio's own Python:
  /Applications/ti/ccs2101/ccs/scripting/python/run.sh ccs_session_cc1354.py <build dir> <restore dir> <results jsonl> [seconds]
<build dir>: to_f2.sh output (baseline/ and from_mspm0_shim/). <restore dir>: main.bin (flash from 0) and ccfg.bin.
Prints one JSON line per run; UART is captured by a separate listener (see run_cc1354_session.sh)."""
import glob, json, re, sys, time
from pathlib import Path
from scripting import initScripting

HERE = Path(__file__).resolve().parent
CCXML = str(HERE.parent.parent / "cc1354p10/CC1354P10.ccxml")
DOUT, DOE = 0x40022080, 0x400220D0
APPS = ["empty", "gpiointerrupt", "spicontroller", "spiperipheral"]
bdir, rdir, out = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
secs = float(sys.argv[4]) if len(sys.argv) > 4 else 6

ds = initScripting()
ds.setScriptingTimeout(120000)
ds.configure(CCXML)
s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
s.target.connect()
print("connected", flush=True)
try:
    for app in APPS:
        for variant, sub in (("native", "baseline"), ("ported", "from_mspm0_shim")):
            img = glob.glob(str(bdir / sub / app / "freertos/ticlang/*.out"))[0]
            t0 = time.time()
            s.memory.loadProgram(img)          # sets the PC to the program's entry; "System Reset" is refused on this chip
            t_run = time.time()
            s.target.run(False)
            time.sleep(secs)
            s.target.halt()
            rec = {"app": app, "variant": variant, "load_secs": round(t_run - t0, 1), "t_run": t_run, "t_halt": time.time()}
            for name, addr in (("dout", DOUT), ("doe", DOE)):
                try:
                    rec[name] = "%08x" % s.memory.readOne(addr)
                except Exception as e:   # GPIO registers can be unreadable while the peripheral domain is off
                    rec[name] = "unreadable: " + str(e)[-60:]
            open(out, "a").write(json.dumps(rec) + "\n")
            print(json.dumps(rec), flush=True)
finally:
    s.memory.loadBinaryProgram(str(rdir / "main.bin"), 0x0)
    s.memory.loadBinaryProgram(str(rdir / "ccfg.bin"), 0x50000000)
    print("restored Fox Red 1354 image (starts at the next power-up)", flush=True)
    s.target.disconnect()
    ds.shutdown()
