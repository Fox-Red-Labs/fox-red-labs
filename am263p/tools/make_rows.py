#!/usr/bin/env python3
"""Turn the saved logs in am263p/data/logs/ into rows of data/results/am263p.csv (replaces the rows it makes; keeps the rest).
   python3 am263p/tools/make_rows.py          (then python3 tools/results/check.py)"""
import csv, hashlib, re, statistics, sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[2]; LOGS = REPO / "am263p" / "data" / "logs"; CSV = REPO / "data" / "results" / "am263p.csv"
COLS = ["board", "test", "variant", "status", "value", "value_hi", "runs", "clock_mhz", "conditions", "date", "source", "checked", "note"]
DATE = "2026-10-09"
BASE = "compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;route=result printed on the UART0 console (XDS110 port 1, 115200), port opened before the load; not read back from RAM"
CLK = "R5F at 400 MHz (the SDK's SOC_getSelfCpuClk and TI's SysConfig clock), checked against the Mac clock: a line fit over 31 one-second UART lines gives 399.9 MHz"
rows = []

def sha(files):
    h = hashlib.sha256()
    for f in files: h.update(Path(f).read_bytes())
    return h.hexdigest()[:16]
def logs(prefix): return sorted(LOGS.glob(f"{prefix}-r[0-9].txt"))
def num(txt, pat, g=1): return float(re.search(pat, txt).group(g))
def add(test, variant, status, value="", value_hi="", runs="", clock="", cond="", src="", checked="", note=""):
    rows.append({"board": "am263p", "test": test, "variant": variant, "status": status, "value": value, "value_hi": value_hi, "runs": runs,
                 "clock_mhz": clock, "conditions": cond, "date": DATE, "source": src, "checked": checked, "note": note})
def fmt(x, n=3): return f"{x:.{n}f}".rstrip("0").rstrip(".") if isinstance(x, float) else str(x)

# ---- CoreMark
MEM = {"ocsram": "code, constants and the 16 kB main stack in OCSRAM (the SDK calls it OCRAM) with both L1 caches on, .data in TCMA (TI's linker placement)",
       "tcm": "CoreMark's own code and constants in TCMA, the main stack in TCMB, SDK and library code in OCSRAM with caches on"}
OPTN = {"o3": "-O3", "o2": "-O2", "os": "-Os", "oz": "-Oz"}
cm_scores = {}
for mem in ("ocsram", "tcm"):
    for o in ("o3", "o2", "os", "oz"):
        label = f"ticlang-bare-{o}-{mem}"; fl = logs(label)
        vals = [num(f.read_text(), r"Iterations/Sec\s*:\s*([0-9.]+)") for f in fl]
        iters = int(num(fl[0].read_text(), r"Iterations\s*:\s*(\d+)")); secs = num(fl[0].read_text(), r"Total time \(secs\):\s*([0-9.]+)")
        assert all("Correct operation validated" in f.read_text() for f in fl) and len(fl) == 3
        med = statistics.median(vals); cm_scores[label] = med
        add("coremark", label, "measured", fmt(med), fmt(max(vals)), 3, 400,
            f"{BASE};log_sha256={sha(fl)};{OPTN[o]}; {MEM[mem]}; PMU cycle counter in 64-cycle ticks; iterations picked by CoreMark's own 10 s rule ({iters:,} iterations, {secs:.1f} s); {CLK}; Mac load 4 to 6 (cycles counted on the chip)",
            f"am263p/data/logs/{label}-r1.txt", "crc", f"Source am263p/bench/coremark/; EEMBC files unmodified from third-party/coremark; the three runs differ by under 0.002%; value is the median, value_hi the best")
fl = logs("ti-port-o3-ocsram"); vals = [num(f.read_text(), r"Iterations/Sec\s*:\s*([0-9.]+)") for f in fl]
assert all("All tests have passed" in f.read_text() for f in fl)
add("coremark", "ti-port-o3-ocsram", "measured", fmt(statistics.median(vals)), "", 3, 400,
    f"{BASE};log_sha256={sha(fl)};-O3 (TI's makefile line is -Os -Oz -O3, last one wins); TI's benchmarks/coremark_benchmark for am263px-lp r5fss0-0_nortos, built by the SDK makefile, unchanged; 15,000 iterations, 10.24 s timed with ClockP_getTimeUsec; code and stack in OCSRAM with caches on, .data in TCMA; {CLK}",
    "am263p/data/logs/ti-port-o3-ocsram-r1.txt", "crc", "TI's own printed score (it divides cycles by 400 MHz); TI's page shows 1,508 per second (3.769 per MHz) on the controlCARD with the same chip; ours with CoreMark's code in TCM reads 1,498")
for lab in ("ticlang-bare-o3-ocsram", "ticlang-bare-o3-tcm", "ti-port-o3-ocsram"):
    v = cm_scores.get(lab) or statistics.median(vals)
    add("coremark_per_mhz", lab, "measured", fmt(v / 400, 3), "", 3, 400, f"{BASE};derived from the coremark row of the same variant", f"am263p/data/logs/{lab}-r1.txt", "crc", f"{fmt(v,3)} / 400; TI's page lists 3.769 on the LaunchPad")
add("coremark", "ticlang-bare-o3-xip", "waiting", cond=f"{BASE};code in OSPI flash through the cache needs the image written into flash", src="am263p/README.md", checked="",
    note="Not run: it needs a flash write and tonight's rule is RAM loads only (the flash backup lost block 0x80000)")
add("coremark", "ticlang-bare-o3-xip-sp", "waiting", cond=f"{BASE};XIP with TI's smart placement", src="am263p/README.md", checked="", note="Not run: needs a flash write, same as the XIP row")

# ---- Dhrystone
def dh(prefix):
    fl = logs(prefix); r = [re.search(r"runs=(\d+) cycles=(\d+) dps=(\d+) checks_bad=(\d+)", f.read_text()) for f in fl]
    assert all(m and m.group(4) == "0" for m in r) and len(fl) == 3
    dps = [int(m.group(3)) for m in r]; return fl, statistics.median(dps), int(r[0].group(1))
for pre, label, opt, extra, note in (("dhry-noinline-o3", "ticlang-bare-o3-noinline", "-O3 -fno-inline", "", "rules-compliant (no inlining across the two source files)"),
                                     ("dhry-noinline-os", "ticlang-bare-os-noinline", "-Os -fno-inline", "", "rules-compliant"),
                                     ("dhry-inline-o3", "ticlang-bare-o3-inline", "-O3 with inlining", "", "NOT rules-compliant (inlining); experiment")):
    fl, dps, runs = dh(pre)
    add("dhrystone", label, "measured", fmt(dps / 1757 / 400, 4), "", 3, 400,
        f"{BASE};log_sha256={sha(fl)};{opt}; code, data and stack in OCSRAM with caches on; PMU cycle counter, one tick per CPU cycle; Dhrystone 2.1 sources from mspm0l1306/bench/dhrystone/src unmodified; {runs:,} runs; {CLK}",
        "am263p/bench/dhrystone/", "serial", f"{int(dps):,} Dhrystones per second / 1,757 / 400 MHz; every final-value check passed; {note}")
t = (LOGS / "ti-dhrystone-r1.txt").read_text()
add("dhrystone", "ti-example-o3", "measured", fmt(num(t, r"Normalized MIPS/MHz:\s*([0-9.]+)"), 4), "", 1, 400,
    f"{BASE};log_sha256={sha([LOGS/'ti-dhrystone-r1.txt'])};TI's benchmarks/dhrystone_benchmark for am263px-lp, SDK makefile -O3, unchanged; TI's cycle counter; {CLK}", "am263p/data/logs/ti-dhrystone-r1.txt", "serial",
    "TI's page shows 1.9610; TI's build allows inlining, ours with inlining reads 1.88")

# ---- math and TI's mathlib benchmarks
def mm(prefix, key):
    fl = logs(prefix); v = [int(re.search(key + r"=(\d+)", f.read_text()).group(1)) / 100 for f in fl]; assert len(fl) == 3; return fl, statistics.median(v)
for var, pre, what in (("hw-tmu", "math-tmu", "ti_tmu_sincos on the memory-mapped TMU (a store, four no-ops, two loads)"), ("sdk", "math-sdk", "TI's software mathlib ti_arm_sincos"),
                       ("sw", "math-sw", "clang C library sinf and cosf (float)")):
    fl, v = mm(pre, "sincos_x100")
    add("math_sincos", var, "measured", fmt(v, 2), "", 3, 400,
        f"{BASE};log_sha256={sha(fl)};-O2; code in OCSRAM with caches on; PMU cycle counter; mean of 1,000 calls over one turn on the second pass (caches warm), empty loop (5.6 cycles) removed; one sin plus cos pair through {what}; R5FSS0-0 only (TMU erratum i2485 is for core 1 of a cluster)",
        "am263p/bench/math/main.c", "serial", {"hw-tmu": "plan expected 15 to 40; TI's tmu_operations shows 35.0 average for its SINCOS test", "sdk": "TI's mathlib benchmark shows 79.0 for sincos, plan expected about 71",
                                                "sw": "TI's mathlib benchmark shows 276 plus 278 for the double-precision sin and cos; this row is float sinf and cosf, so lower than the plan's 590"}[var])
fl, v = mm("math-sw", "divide_x100")
add("math_divide", "sw", "measured", fmt(v, 2), "", 3, 400, f"{BASE};log_sha256={sha(fl)};-O2; code in OCSRAM, caches on; PMU cycle counter; mean of 1,000 divides, empty loop removed; float divide on the FPU's VDIV", "am263p/bench/math/main.c", "serial", "AM13E230 reads 15.07 on its M33; same VFP divide here")
add("math_divide", "hw-tmu", "not_supported", cond=f"{BASE};TMU functions in the SDK (ti_tmu_trig.h)", src="am263p/README.md", checked="", note="The SDK's TMU driver has sin, cos, atan, log, exp and atan2 only; no divide or square root")
# foc
for var, pre, what in (("hw-tmu-ocsram", "foc-tmu", "ti_tmu_sin and ti_tmu_cos"), ("sdk-ocsram", "foc-sdk", "TI's software mathlib ti_arm_sin and ti_arm_cos"), ("sw-ocsram", "foc-sw", "clang sinf and cosf")):
    fl = logs(pre); m = [re.search(r"best=(\d+) worst=(\d+) mean_x100=(\d+)", f.read_text()) for f in fl]; assert len(fl) == 3
    mean = statistics.median(int(x.group(3)) for x in m) / 100; best = min(int(x.group(1)) for x in m); worst = max(int(x.group(2)) for x in m)
    add("foc_step", var, "measured", fmt(mean, 1), worst, 3, 400,
        f"{BASE};log_sha256={sha(fl)};-O2; code in OCSRAM, caches on; PMU cycle counter; the same foc.c as c2000-launchpad/bench/ctrl unchanged; mean of 1,000 steps over one electrical turn on the second pass; sin and cos through {what}",
        "am263p/bench/foc/main.c", "serial", f"value mean, value_hi worst, best {best} cycles; the duty outputs match across all three variants to 1 part in a million")

# ---- timing
fl = logs("timing"); t = fl[0].read_text()
g = lambda k: int(re.search(k + r"=(\d+)", t).group(1))
for var, k, what in (("register", "gpio_reg_x100", "raw stores to the GPIO0 block's SET_DATA and CLR_DATA"), ("driver", "gpio_drv_x100", "the SDK's GPIO_pinWriteHigh and GPIO_pinWriteLow")):
    add("gpio_toggle", var, "measured", fmt(g(k) / 100, 2), "", 3, 400, f"{BASE};log_sha256={sha(fl)};-O3; code in OCSRAM, caches on; PMU cycle counter; best of 5 loops of 2,000 set and clear pairs, per pin write; pin 138, not muxed to a pad: the cost is the store into the GPIO block ({what})",
        "am263p/bench/timing/main.c", "serial", "empty loop 1.05 cycles per iteration and a counter read 7.0 cycles are inside; the three runs read the same")
m = re.search(r"irq best=(\d+) worst=(\d+) n=(\d+) fiq best=(\d+) worst=(\d+) n=(\d+)", t)
add("irq_latency", "bare", "measured", m.group(1), m.group(2), 3, 400, f"{BASE};log_sha256={sha(fl)};-O3; interrupt 20 pended with HwiP_post, timed to the first line of a C handler reached through the SDK's HwiP dispatcher; best of 200, value_hi the worst; includes one counter read (7 cycles) and the dispatcher's entry code",
    "am263p/bench/timing/main.c", "serial", f"{int(m.group(1))/0.4:.0f} ns best; plan expected 0.1 to 0.25 us for an external edge, this is a software-pended interrupt through TI's dispatcher")
add("irq_latency", "bare-fiq", "measured", m.group(4), m.group(5), 3, 400, f"{BASE};log_sha256={sha(fl)};-O3; as the bare row but the interrupt is mapped as FIQ", "am263p/bench/timing/main.c", "serial", "the FIQ path skips part of the IRQ dispatcher's save")
add("irq_latency", "bare-external-wire4", "waiting", cond=f"{BASE};external edge on a header pin", src="am263p/README.md", checked="", note="Needs wire 4 (bench setup 4b): not on tonight")

# ---- TI examples
t = (LOGS / "ti-task-switch-r1.txt").read_text()
sem = num(t, r"semaphore give/take\) = (\d+) ns") / 1000; ntf = num(t, r"notification give/take\) = (\d+) ns") / 1000
for var, v, what in (("freertos-ti-example-semaphore", sem, "semaphore give and take"), ("freertos-ti-example-notify", ntf, "direct-to-task notification")):
    add("rtos_task_switch", var, "measured", fmt(v, 3), "", 1, 400, f"{BASE};log_sha256={sha([LOGS/'ti-task-switch-r1.txt'])};TI's kernel/freertos/task_switch for am263px-lp (SDK makefile, release), 2,000,000 switches timed with ClockP_getTimeUsec; {what}; code in OCSRAM, caches on; {CLK}",
        "am263p/data/logs/ti-task-switch-r1.txt", "host-clock", f"TI's page shows 0.763 us (semaphore) and 0.558 us (notification) for the same example; ours is 8 to 9 times slower and unexplained. The Mac saw 12.1 s for the first loop, which matches the printed time")
add("rtos_max_tasks", "freertos", "not_run", cond=BASE, src="am263p/README.md", checked="", note="Not run: needs a FreeRTOS harness built with the SDK makefile; free disk was 1.7 GB, under the 2 GB floor. Note FreeRTOS here takes its heap from the C library, 32 kB by default")
# ---- setup
add("blink_build", "cli", "measured", "5.9", "", 1, "", "SDK i2c_led_blink for am263px-lp r5fss0-0_nortos, SDK makefile after make clean (SysConfig 1.28.0 run included), TI Arm Clang 5.1.1, the SDK's nortos libraries already built; Mac load about 4", "am263p/STOPWATCH.md", "host-clock",
    "wall time of one make; the SDK has no GPIO blink example for the LaunchPad, so the blink is the I2C LED example")
add("blink_flash", "dslite", "measured", "5", "", 4, "", "dslite load of a 0.9 MB .out into RAM on AM263Px_ZCZ_C_S26E0426.ccxml (GEL runs); loads of 4.4, 4.8, 5.1 and 7.6 s; the first load of i2c_led_blink took 78 s and an earlier try timed out at 90 s",
    "am263p/STOPWATCH.md", "host-clock", "no flash is written: RAM load only; this chip has no on-chip flash")
add("blink_flash_bytes", "bare", "not_applicable", cond="i2c_led_blink, TI Arm Clang 5.1.1 -O3 (TI's makefile)", src="am263p/STOPWATCH.md", checked="linker",
    note="No on-chip flash: the LaunchPad boots from an external 32 MB OSPI flash. The image is 37,600 B of code and constants, 872 B data, in RAM")
add("blink_ram_bytes", "bare", "measured", "96024", "", 1, "", "i2c_led_blink, TI Arm Clang 5.1.1 (TI's makefile line -Os -Oz -O3: -O3), tiarmsize text plus data plus bss; 37,600 + 872 + 57,552 B; includes the 32 kB heap and the 16 kB main stack the SDK linker file reserves",
    "am263p/STOPWATCH.md", "linker", "OCSRAM used 0x176d8 plus the 0x40 vector table in TCMA")
add("probe_included", "", "measured", "yes", "", 1, "", "", "am263p/data/identify.txt", "by-us", "Onboard XDS110, serial S26E0426, firmware 3.0.0.29, micro-B J34; its second port is the UART0 console")
add("zephyr_support", "upstream", "not_supported", cond="Zephyr tree on this Mac dated 2026-08-06", src="am263p/README.md", checked="by-us", note="boards/ti has no AM263 board; only am243x_evm, am62l_evm, sk_am62 and sk_am64 in the Sitara line. TI's downstream fork not checked")

for core in ("core1", "core2", "core3"):
    add("coremark", f"ticlang-bare-o3-tcm-{core}-alone", "not_run", cond=BASE, src="am263p/README.md", checked="",
        note="Not run: needs a build that prints the core id (MPIDR) so the run can be proved to be on that core, and free disk was 1.7 GB. One try: the existing core-0 TCM image loaded with DSLite -n 1 printed 1497.79, the core-0 figure, but nothing proved which core ran it")
add("coremark", "ticlang-bare-o3-tcm-lockstep", "not_run", cond=BASE, src="am263p/README.md", checked="", note="Not run: needs the Lock_Step CCXML and a lockstep SysConfig build; disk was under 2 GB")
# ---- waiting on wires, meter or space
W = "Needs wire"
for tst, var, why in (("adc_dac_error", "", "Needs wire 3 (DAC output to an ADC input), bench setup 4b"), ("uart_max_clean", "", "Needs wires 1 and 2 (UART loop), bench setup 4b"),
                      ("adc_max_rate", "", "No wire needed. Not run: needs a new ADC plus DMA program and a build, and the disk was under 2 GB when this was reached"),
                      ("examples_built", "mcu-plus-sdk-26.01-lp", "Not run: the sweep script (am263p/tools/sweep_build.sh) is written; it stopped at once because free disk was 1.4 GB, under the 2 GB floor"),
                      ("examples_ran_clean", "mcu-plus-sdk-26.01-lp", "Not run: needs the built sweep; RAM loads only and never any example with an OSPI erase or write"),
                      ("active_current_per_mhz", "", "Needs the USB-C power meter (checkpoint 4)"), ("energy_coremark", "", "Needs the USB-C power meter (checkpoint 4)"),
                      ("sleep_current", "", "Needs the USB-C power meter (checkpoint 4)")):
    add(tst, var, "waiting", cond=BASE, src="am263p/README.md", checked="", note=why)

# merge: keep the rows this script does not make (time_to_blink)
keep = []
if CSV.exists():
    made = {(r["test"], r["variant"]) for r in rows}
    for r in csv.DictReader(CSV.open()):
        if (r["test"], r["variant"]) not in made:
            if r["test"] == "time_to_blink":
                r["note"] = ("The eight green LEDs (TPIC2810 I2C expander) were seen blinking at 21:18. The load took 78 s this time and 5 s on an earlier try. "
                             "Serial proof came later (2026-10-09 21:46): with the UART port opened before the load, the app prints 'I2C LED Blink Test Started ...' on /dev/cu.usbmodemS26E04261 at 115200. Excludes the build.")
                r["conditions"] = r["conditions"].replace("; the app UART printed nothing on either XDS110 port", "")
            keep.append(r)
with CSV.open("w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=COLS); w.writeheader()
    for r in keep + rows: w.writerow(r)
print(len(keep), "kept,", len(rows), "written")
