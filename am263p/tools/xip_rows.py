#!/usr/bin/env python3
"""Turn the execute-in-place logs into results rows and a summary table.
xip_rows.py [--copy] [--rows] [--table]
  --copy : copy the run logs from out/am263p/xip into am263p/data/logs/xip/ (the repo copy rows cite)
  --rows : print data/results/am263p.csv rows for xip_memcpy_tasks (TI's OptiFlash benchmark, 0 B row) and
           ai_vendor_example (TI's AI examples: median of 101 warm inferences), three runs each, value = median of runs
  --table: print markdown tables for the experiment page"""
import glob, hashlib, os, re, shutil, statistics, sys
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); REPO = os.path.dirname(HERE)
SRC = os.path.abspath("out/am263p/xip"); DST = os.path.join(HERE, "data", "logs", "xip")
MHZ = 400
COND = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
        "loaded into RAM over the debugger (development boot, no boot switch change); flash sections written first by "
        "am263p/bench/xip/flasher at offset {off} and read back with zero mismatches; flash IS25LX256 8D-8D-8D with "
        "PHY tuning failing, so 133.33 MHz / 8 = 16.7 MHz; TI's pages run with PHY on; R5F at 400 MHz")
OPTI = [("ocram", "ocram"), ("flash_xip", "xip"), ("flash_xip_l2_cache_8k", "xip-l2-8k"), ("flash_xip_l2_cache_16k", "xip-l2-16k"),
        ("flash_xip_l2_cache_32k", "xip-l2-32k"), ("flash_xip_l2_cache_64k", "xip-l2-64k"), ("flash_xip_smart_placement", "xip-sp")]
TI_PAGE = {"ocram": 12450, "xip": 46566, "xip-l2-8k": 42429, "xip-l2-16k": 39194, "xip-l2-32k": 31881, "xip-l2-64k": 20446, "xip-sp": 11014}
AI = ["arc_fault", "blower_imbalance", "fan_blade_anomalydetection", "forecasting_pmsm_rotor_temp", "generic_timeseries_anomalydetection",
      "generic_timeseries_classification", "generic_timeseries_forecasting", "generic_timeseries_regression", "hvac_indoor_temp_forecast",
      "motor_fault", "torque_measurement", "washing_machine_load_weighing"]
GOLDEN_BROKEN = {"fan_blade_anomalydetection"}
LAYOUT = {"ocram": "", "xip": "-xip", "xip64k": "-xip-l2-64k"}

def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]
def rel(p): return os.path.relpath(p, REPO)

def opti(v):
    out = []
    for i in (1, 2, 3):
        p = os.path.join(DST, f"{v}-r{i}.txt")
        if not os.path.exists(p): continue
        t = open(p).read()
        m = re.search(r"\s0B\s+(\d+)us\s+(\d+)", t)
        if m and "All tests have passed" in t: out.append((int(m.group(1)), int(m.group(2)), p))
    return out

def ai(layout, ex):
    out = []
    for i in (1, 2, 3):
        p = os.path.join(DST, f"ai_{layout}_{ex}-r{i}.txt")
        if not os.path.exists(p): continue
        t = open(p).read()
        m = re.search(r"FRL_AI fe_cycles (\d+) infer_cold (\d+) infer_min (\d+) infer_median (\d+) infer_max (\d+)", t)
        g = re.search(r"Golden vectors matched: (\d+) not matched: (\d+)", t)
        # fan_blade: TI's own unmodified example matches 0 of 192 golden values on this board; keep its timing
        if m and g and (g.group(2) == "0" or ex in GOLDEN_BROKEN):
            out.append(tuple(int(x) for x in m.groups()) + (int(g.group(1)), int(g.group(1)) + int(g.group(2)), p))
    return out

def offset(prefix):
    p = os.path.join(DST, prefix + "-flasher.txt")
    if not os.path.exists(p): return "none (RAM layout)"
    m = re.search(r"XIPFLASH OK offset (0x[0-9a-f]+) len (\d+)", open(p).read())
    return f"{m.group(1)} ({m.group(2)} bytes)" if m else "unknown"

def q(s): return '"' + s.replace('"', "'") + '"'

if __name__ == "__main__":
    a = sys.argv[1:]
    if "--copy" in a:
        os.makedirs(DST, exist_ok=True)
        for f in glob.glob(os.path.join(SRC, "*.txt")):
            shutil.copy(f, DST)
    if "--rows" in a:
        for v, var in OPTI:
            r = opti(v)
            if len(r) < 3: print(f"# {var}: {len(r)} runs", file=sys.stderr); continue
            ms = statistics.median(x[0] for x in r) / 1000
            cond = COND.format(off=offset(v)) + f";TI's optiflash_benchmark ({v}) for the controlCARD, ported to the LaunchPad (package ZCZ_C, SDK path); FreeRTOS; -Os;log_sha256={sha(r[0][2])}"
            note = (f"runs {', '.join(str(x[0]) for x in r)} us; instruction-cache misses {', '.join(str(x[1]) for x in r)}; "
                    f"TI's page (controlCARD, PHY on): {TI_PAGE[var]/1000:.3f} ms; all tests passed in each run")
            print(f"am263p,xip_memcpy_tasks,{var},measured,{ms:.3f},{max(x[0] for x in r)/1000:.3f},3,{MHZ},{q(cond)},2026-10-10,{rel(r[0][2])},serial,{q(note)}")
        for ex in AI:
            for lay, suf in LAYOUT.items():
                r = ai(lay, ex)
                if len(r) < 3: print(f"# {ex} {lay}: {len(r)} runs", file=sys.stderr); continue
                med = statistics.median(x[3] for x in r); ms = med / (MHZ * 1000)
                cond = (COND.format(off=offset(f"ai_{lay}_{ex}")) if lay != "ocram" else COND.split(";loaded")[0] + ";loaded into RAM over the debugger;R5F at 400 MHz") + \
                       (f";TI's AI example {ex} (am263px-lp, r5fss0-0 nortos, TI's prebuilt TVM model on the R5F, no NPU on this chip) with two lines swapped for timed versions (am263p/bench/xip/ai/frl_ai_timing.h); "
                        f"layout {lay}: " + {"ocram": "TI's, code and constants in on-chip RAM", "xip": ".text and .rodata in flash, cached by the R5F's L1 only",
                                              "xip64k": ".text and .rodata in flash plus TI's 64 kB RL2 cache"}[lay] +
                        f";PMU cycle counter;value is the median of 101 warm inferences, median of 3 loads;-Os (the example's makefile);log_sha256={sha(r[0][-1])}")
                note = (f"cycles median {int(med)}; cold first call {', '.join(str(x[1]) for x in r)}; min {min(x[2] for x in r)}, max {max(x[4] for x in r)}; "
                        f"feature extraction {r[0][0]} cycles once; golden vectors {r[0][5]} of {r[0][6]} matched in every run")
                chk = "vectors"
                if ex in GOLDEN_BROKEN:
                    chk = "serial"
                    note += "; TI's golden check fails: 0 of 192 values match, the same as TI's unmodified example on this board, so this times the model but does not confirm its output"
                print(f"am263p,ai_vendor_example,{ex}{suf},measured,{ms:.4f},,3,{MHZ},{q(cond)},2026-10-10,{rel(r[0][-1])},{chk},{q(note)}")
        # CoreMark: TI's coremark_benchmark in the flash layouts (its RAM row, ti-port-o3-ocsram, is already in the store)
        for lay, var in (("xip", "ti-port-o3-xip"), ("xip64k", "ti-port-o3-xip-l2-64k")):
            r = []
            for i in (1, 2, 3):
                p = os.path.join(DST, f"cm_{lay}_coremark_benchmark-r{i}.txt")
                if not os.path.exists(p): continue
                s = open(p).read(); m = re.search(r"CoreMark 1\.0 : ([0-9.]+)", s)
                ok = all(x in s for x in ("[0]crclist       : 0xe714", "[0]crcmatrix     : 0x1fd7", "[0]crcstate      : 0x8e3a"))
                if m and ok: r.append((float(m.group(1)), p))
            if len(r) < 3: print(f"# coremark {lay}: {len(r)} runs", file=sys.stderr); continue
            v = statistics.median(x[0] for x in r)
            cond = COND.format(off=offset(f"cm_{lay}_coremark_benchmark")) + (f";-O3 (TI's makefile line is -Os -Oz -O3, last one wins); TI's benchmarks/coremark_benchmark for am263px-lp r5fss0-0_nortos with "
                    + {"xip": ".text and .rodata in flash, R5F L1 caches only", "xip64k": ".text and .rodata in flash plus TI's 64 kB RL2 cache"}[lay]
                    + f" (am263p/tools/ai_xip_gen.py coremark); 15,000 iterations, about 10.3 s;log_sha256={sha(r[0][1])}")
            note = f"runs {', '.join(f'{x[0]:.3f}' for x in r)}; list, matrix and state CRCs are CoreMark's known-good values (0xe714, 0x1fd7, 0x8e3a) in every run; same build in RAM: 1464.527 (ti-port-o3-ocsram)"
            print(f"am263p,coremark,{var},measured,{v:.3f},{max(x[0] for x in r):.3f},3,{MHZ},{q(cond)},2026-10-10,{rel(r[0][1])},crc,{q(note)}")
    if "--table" in a:
        base = statistics.median(x[0] for x in opti("ocram")) if opti("ocram") else None
        print("| Layout | 0 B time, ms (median of 3) | Against our OCRAM | TI's page, ms | TI against its OCRAM |\n|---|---|---|---|---|")
        for v, var in OPTI:
            r = opti(v)
            if not r: continue
            m = statistics.median(x[0] for x in r)
            print(f"| {var} | {m/1000:.2f} | {m/base:.2f}x | {TI_PAGE[var]/1000:.2f} | {TI_PAGE[var]/TI_PAGE['ocram']:.2f}x |")
        print("\n| Model | RAM, ms | Flash, no L2, ms | Flash, 64 kB L2, ms | First call from flash, ms | Feature extraction, ms |\n|---|---|---|---|---|---|")
        for ex in AI:
            cells = []
            for lay in LAYOUT:
                r = ai(lay, ex); cells.append(f"{statistics.median(x[3] for x in r)/(MHZ*1000):.3f}" if len(r) == 3 else "n/a")
            r = ai("xip", ex); cold = f"{statistics.median(x[1] for x in r)/(MHZ*1000):.3f}" if r else "n/a"
            r0 = ai("ocram", ex); fe = f"{r0[0][0]/(MHZ*1000):.3f}" if r0 else "n/a"
            print(f"| {ex} | {' | '.join(cells)} | {cold} | {fe} |")
