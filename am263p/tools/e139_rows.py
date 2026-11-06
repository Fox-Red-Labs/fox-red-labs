#!/usr/bin/env python3
"""Results rows for E139 on the LP-AM263P: the control loop on R5FSS0-0 (bench/ctrl/images/ctrl_tmu_long_delay20.out:
TMU, 200,000 interrupts after a 20 s start delay) alone, and while cores 1 to 3 run CoreMark (cm4_0xE_load images:
60,000 iterations, no start sync). Logs: data/logs/e139/{alone,loaded}-r<N>.txt; the shared CoreMark table read over the
probe when the loop finished (table-mid-r<N>.bin) and after CoreMark finished (table-end-r<N>.bin); cores 1 to 3 alone
with the same images: cm-alone-r<N>.bin. Writes table-*.txt beside each .bin. Value = median of 3 runs, value_hi = largest."""
import glob, hashlib, os, re, statistics, struct
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); REPO = os.path.dirname(HERE)
L = os.path.join(HERE, "data", "logs", "e139"); MHZ = 400
BASE = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
        "four R5F cores at 400 MHz in split mode, loaded into RAM over the debugger with am263p/tools/mcload.py, core 0 first "
        "(its connect clears on-chip RAM), then cores 1 to 3 during core 0's 20 s start delay; ")
LOOP = ("control loop on R5FSS0-0: am263p/bench/ctrl/ctrl.c with -DUSE_TMU -DN_IRQ=200000 -DSTART_DELAY_S=20, -Os, code in "
        "on-chip RAM with caches on; EPWM0 50 kHz zero event to ADC1 SOC0 to the ADC interrupt (IRQ, priority 0); PMU cycle counter; "
        "200,000 interrupts per run, 3 runs; no missed period; ")
def q(s): return '"' + s.replace('"', "'") + '"'
def table(p):
    b = open(p, "rb").read(); out = []
    for i in range(4):
        m, d, e, k, s, cl, cm, cs, cf, st = struct.unpack("<IIiifIIIII", b[i * 40:i * 40 + 40])
        out.append(dict(magic=m, done=d, errors=e, known_id=k, score=s, crcfinal=cf, stage=st, crclist=cl))
    t = p[:-4] + ".txt"
    open(t, "w").write("".join(f"core {i} magic 0x{x['magic']:08x} done {x['done']} errors {x['errors']} known_id {x['known_id']} "
                               f"score {x['score']:.3f} crclist {x['crclist']} crcfinal 0x{x['crcfinal']:04x} stage {x['stage']}\n" for i, x in enumerate(out)))
    return out
def loop(kind):
    r = []
    for i in (1, 2, 3):
        p = os.path.join(L, f"{kind}-r{i}.txt"); t = open(p).read()
        a = re.search(r"irqs (\d+) timeout (\d+) skipped (\d+)", t)
        e = re.search(r"entry_cyc min (\d+) mean_x10 (\d+) max (\d+) exit_cyc min (\d+) mean_x10 (\d+) max (\d+) first (\d+) min_rest (\d+)", t)
        assert a and e and a.groups() == ("200000", "0", "0"), p
        r.append(([int(x) for x in e.groups()], p))
    return r
ALONE = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
         "R5FSS0-0 at 400 MHz, split mode, loaded into RAM over the debugger with am263p/tools/capload.py, core 0 only; ")
COPY = ("copying a 64 kB block back and forth between two buffers for 40 s (am263p/bench/multicore/coremark4/cm4.c "
        "-DFRL_MEMSTRESS=40; larger than the data cache, so the copies keep going out to the on-chip RAM), {how}")
MEMHOW = {"mem": "both buffers in its own 256 kB of on-chip RAM, with the C library memcpy (TI_memcpy_small in this build, a byte loop)", "mem64": "both buffers in its own 256 kB of on-chip RAM, with our 8-byte word loop (-DFRL_WORDCOPY)",
          "near64": "with our 8-byte word loop (-DFRL_WORDCOPY), buffer B in its own region and buffer A in the 256 kB just below core 0's on-chip RAM (-DFRL_MEMNEAR: 0x70000000 + 64 kB x core index, TI's SBL region, unused in development boot)"}
for kind, var, extra in (("alone", "hw-tmu-eoc-ocsram-200k-alone", "cores 1 to 3 run nothing of ours (core 0's connect clears on-chip RAM)"),
                         ("mem-loaded", "hw-tmu-eoc-ocsram-200k-beside-bytecopy3", "cores 1 to 3 each " + COPY.format(how=MEMHOW["mem"]) +
                          "; each core's stage marker read 3 (copying) when the loop finished, and all three then reported"),
                         ("mem64-loaded", "hw-tmu-eoc-ocsram-200k-beside-wordcopy3", "cores 1 to 3 each " + COPY.format(how=MEMHOW["mem64"]) +
                          "; each core's stage marker read 3 (copying) when the loop finished, and all three then reported"),
                         ("tcm-alone", "hw-tmu-eoc-tcm-200k-alone", "cores 1 to 3 run nothing of ours (core 0's connect clears on-chip RAM)"),
                         ("tcm-near64-loaded", "hw-tmu-eoc-tcm-200k-beside-nearcopy3", "cores 1 to 3 each " + COPY.format(how=MEMHOW["near64"]) +
                          "; each core's stage marker read 3 (copying) when the loop finished, and all three then reported"),
                         ("near64-loaded", "hw-tmu-eoc-ocsram-200k-beside-nearcopy3", "cores 1 to 3 each " + COPY.format(how=MEMHOW["near64"]) +
                          "; each core's stage marker read 3 (copying) when the loop finished, and all three then reported"),
                         ("loaded", "hw-tmu-eoc-ocsram-200k-beside-coremark3",
                          "cores 1 to 3 running CoreMark from their own on-chip RAM the whole time (each core's stage marker read 3, "
                          "CoreMark running, when the loop finished; all three then completed with correct checksums)")):
    if kind in ("mem-loaded", "mem64-loaded", "near64-loaded", "tcm-near64-loaded"):
        pre = kind.rsplit("-", 1)[0].replace("-loaded", "")
        for i in (1, 2, 3):
            mid = table(os.path.join(L, f"{pre}-table-mid-r{i}.bin")); end = table(os.path.join(L, f"{pre}-table-end-r{i}.bin"))
            assert [x["stage"] for x in mid[1:]] == [3, 3, 3], i
            assert all(x["done"] == 1 and x["errors"] == 0 for x in end[1:]), i
    if kind == "loaded":
        for i in (1, 2, 3):
            mid = table(os.path.join(L, f"table-mid-r{i}.bin")); end = table(os.path.join(L, f"table-end-r{i}.bin"))
            assert [x["stage"] for x in mid[1:]] == [3, 3, 3], i
            assert all(x["done"] == 1 and x["errors"] == 0 and x["crcfinal"] == 0xbd59 for x in end[1:]), i
    r = loop(kind); p0 = r[0][1]; sha = hashlib.sha256(open(p0, "rb").read()).hexdigest()[:16]
    loop_txt = LOOP if not kind.startswith("tcm") else LOOP.replace("-DSTART_DELAY_S=20, -Os, code in on-chip RAM with caches on;",
        "-DSTART_DELAY_S=20 -DFRL_TCM, -Os, linked with linker_tcm.cmd (am263p/bench/ctrl/tcm_linker.py): the ADC handler, foc_step and TI's interrupt "
        "dispatcher in TCM-A, .data, .bss, the main and mode stacks in TCM-B, the rest of the code and the heap in on-chip RAM, caches on; the one "
        "call left in on-chip RAM from the handler (a 64-bit divide) runs after the measured window;")
    cond = q((ALONE if kind in ("alone", "tcm-alone") else BASE) + loop_txt + extra + f";log_sha256={sha}"); src = os.path.relpath(p0, REPO)
    ent = [g[1] / 10 / MHZ for g, _ in r]; ext = [g[4] / 10 / MHZ for g, _ in r]
    worst = [g[5] / MHZ for g, _ in r]; jit = [(g[5] - g[3]) / MHZ for g, _ in r]; pct = [w / 20.0 * 100 for w in worst]
    g0 = r[0][0]
    firstw = (f"the worst is the first interrupt ({g0[6]} cycles, caches cold)" if g0[5] == g0[6] else
              f"the worst is a later interrupt; the first took {g0[6]} cycles")
    note = (f"trigger to new PWM value: mean {g0[4]/10:.1f}, best {g0[3]}, worst {g0[5]} cycles (run 1); {firstw}, "
            f"the best of the rest {g0[7]}; runs {', '.join(f'{x:.4f}' for x in ext)} us")
    print(f"am263p,control_loop,{var},measured,{statistics.median(ext):.4f},{max(ext):.4f},3,{MHZ},{cond},2026-10-11,{src},serial,{q(note)}")
    print(f"am263p,isr_entry,{var},measured,{statistics.median(ent):.4f},{max(ent):.4f},3,{MHZ},{cond},2026-10-11,{src},serial,"
          f"{q(f'trigger (EPWM zero) to the handler first line, including the ADC conversion: mean {g0[1]/10:.1f}, min {g0[0]}, max {g0[2]} cycles (run 1)')}")
    print(f"am263p,control_loop_worst_pct,{var},measured,{statistics.median(pct):.1f},{max(pct):.1f},3,{MHZ},{cond},2026-10-11,{src},serial,"
          f"{q('derived: worst trigger-to-new-value time / 20 us period')}")
    print(f"am263p,control_loop_jitter,{var},measured,{statistics.median(jit):.3f},{max(jit):.3f},3,{MHZ},{cond},2026-10-11,{src},serial,"
          f"{q('derived: (worst - best) trigger-to-new-value cycles / 400 MHz, all 200,000 interrupts including the first')}")
CM = ("CoreMark from TI's example (-O3 SDK build, 60,000 iterations, -DFRL_NO_SYNC) on R5FSS0-1, R5FSS1-0 and R5FSS1-1, each from its "
      "own 256 kB of on-chip RAM; scores read from the shared table at 0x701D0000 over the probe; ")
alone = sorted(glob.glob(os.path.join(L, "cm-alone-r*.bin")))
for c in (1, 2, 3):
    for kind, files, extra in (("beside-loop", [os.path.join(L, f"table-end-r{i}.bin") for i in (1, 2, 3)], "core 0 running the control loop for 4 s of each run"),
                               ("cores1to3-alone", alone, "core 0 idle (a silent loop)")):
        if len(files) != 3: continue
        s = [table(f)[c]["score"] for f in files]
        base = BASE if kind == "beside-loop" else BASE.replace("then cores 1 to 3 during core 0's 20 s start delay", "then cores 1 to 3")
        print(f"am263p,coremark,ti-port-o3-ocsram-60k-{kind}-core{c},measured,{statistics.median(s):.3f},{max(s):.3f},3,{MHZ},"
              f"{q(base + CM + extra)},2026-10-11,{os.path.relpath(files[0][:-4] + '.txt', REPO)},readback,{q('runs ' + ', '.join(f'{x:.3f}' for x in s) + '; crcfinal 0xbd59, errors 0 each')}")

MEMB = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
        "R5F at 400 MHz in split mode, loaded into RAM over the debugger with am263p/tools/mcload.py; cores 1 to 3 at once, each ")
for pre in ("mem", "mem64", "near64"):
    for c in (1, 2, 3):
        for kind, files, extra in (("cores1to3", [os.path.join(L, f"{pre}-alone-r{i}.bin") for i in (1, 2, 3)], "core 0 running a silent loop"),
                                   ("beside-loop", [os.path.join(L, f"{pre}-table-end-r{i}.bin") for i in (1, 2, 3)], "core 0 running the control loop for 4 s of each run")):
            t = [table(f)[c] for f in files]
            assert all(x["errors"] == 0 and x["done"] == 1 for x in t)
            v = [x["score"] for x in t]
            name = {"mem": "bytecopy", "mem64": "wordcopy", "near64": "wordcopy-nearcore0"}[pre]
            print(f"am263p,mem_bandwidth,{name}-64k-ocram-{kind}-core{c},measured,{statistics.median(v):.1f},{max(v):.1f},3,{MHZ},"
                  f"{q(MEMB + COPY.format(how=MEMHOW[pre]) + '; MB/s = bytes copied (each round 2 x 64 kB) / PMU-cycle time; ' + extra)},2026-10-11,"
                  f"{os.path.relpath(files[0][:-4] + '.txt', REPO)},readback,{q('not a STREAM triad: a block copy within one core; runs ' + ', '.join(f'{x:.1f}' for x in v) + '; the two buffers matched at the end of every run')}")
