#!/usr/bin/env python3
"""Results rows for the LP-AM263P control loop (am263p/bench/ctrl/ctrl.c), from am263p/data/logs/ctrl/ctrl-<variant>-r<N>.txt.
Tests as on the AM13E230: control_loop (us, trigger to new PWM value, mean), isr_entry (us, trigger to the handler's first
line, mean), control_loop_worst_pct (worst trigger-to-new-value as a percent of the 20 us period), control_loop_jitter
(us, worst minus best). Value = median of 3 runs, value_hi = the largest.
LOCKSTEP=1: the same images run with both R5F clusters in lockstep (tools/lockstep_run.py), logs data/logs/lockstep/ls-ctrl-*."""
import hashlib, os, re, statistics
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); REPO = os.path.dirname(HERE)
LS = bool(os.environ.get("LOCKSTEP"))
L = os.path.join(HERE, "data", "logs", "lockstep" if LS else "ctrl"); MHZ = 400
COND = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
        "-Os (TI's adc_soc_epwm makefile); F28-style single-precision FOC step (foc.c and foc.h from c2000-launchpad/bench/ctrl, unchanged); "
        "sine and cosine: {trig}; code in on-chip RAM (OCSRAM) with caches on, loaded into RAM over the debugger; "
        "EPWM0 50 kHz (4,000 counts at 200 MHz, checked: 50,000 EPWM clocks per 100,000 CPU cycles) zero event (SOCA) to ADC1 SOC0 "
        "(AIN0, nothing driven); ADC1 interrupt 1 through TI's interrupt crossbar and HwiP dispatcher (IRQ, priority 0); "
        "PMU cycle counter; EPWM counter read at the handler's first line and after the compare write; 1,000 interrupts per run, "
        "3 runs; no missed period; R5FSS0-0 at 400 MHz;log_sha256={sha}")
if LS:
    COND = COND.replace("loaded into RAM over the debugger;", "loaded into RAM over the debugger with both R5F clusters in lockstep (MSS_CTRL R5SS0 and R5SS1 "
                        "STATUS_REG lock-step bit 0x100 read before and after each run; Cortex_R5_1 refuses a debug connection), reached by a "
                        "debugger System Reset then TI's GEL init steps without the dual-core switch (am263p/tools/lockstep.js);")
TRIG = {"hw-tmu": "TMU (ti_tmu_sin, ti_tmu_cos)", "sdk": "TI's software mathlib (ti_arm_sin, ti_arm_cos)", "sw": "C library sinf, cosf"}

def runs(v):
    out = []
    for i in (1, 2, 3):
        p = os.path.join(L, ("ls-ctrl-{}-r{}.txt" if LS else "ctrl-{}-r{}.txt").format('tmu' if v == 'hw-tmu' else v, i))
        t = open(p).read()
        a = re.search(r"irqs (\d+) timeout (\d+) skipped (\d+)", t)
        e = re.search(r"entry_cyc min (\d+) mean_x10 (\d+) max (\d+) exit_cyc min (\d+) mean_x10 (\d+) max (\d+) first (\d+) min_rest (\d+)", t)
        assert a and e and a.group(1) == "1000" and a.group(2) == "0" and a.group(3) == "0", p
        out.append(([int(x) for x in e.groups()], p))
    return out

def q(s): return '"' + s.replace('"', "'") + '"'

for v in ("hw-tmu", "sdk", "sw"):
    r = runs(v); p0 = r[0][1]; sha = hashlib.sha256(open(p0, "rb").read()).hexdigest()[:16]
    cond = q(COND.format(trig=TRIG[v], sha=sha)); src = os.path.relpath(p0, REPO); var = f"{v}-eoc-ocsram" + ("-lockstep" if LS else "")
    ent = [g[1] / 10 / MHZ for g, _ in r]; ext = [g[4] / 10 / MHZ for g, _ in r]
    worst = [g[5] / MHZ for g, _ in r]; best = [g[3] / MHZ for g, _ in r]
    jit = [(g[5] - g[3]) / MHZ for g, _ in r]; pct = [w / 20.0 * 100 for w in worst]
    g0 = r[0][0]
    note = (f"trigger to new PWM value: mean {g0[4]/10:.1f}, best {g0[3]}, worst {g0[5]} cycles (run 1); the worst is the first "
            f"interrupt ({g0[6]} cycles, caches cold), the best of interrupts 2 to 1,000 is {g0[7]}; runs {', '.join(f'{x:.4f}' for x in ext)} us")
    print(f"am263p,control_loop,{var},measured,{statistics.median(ext):.4f},{max(ext):.4f},3,{MHZ},{cond},2026-10-11,{src},serial,{q(note)}")
    print(f"am263p,isr_entry,{var},measured,{statistics.median(ent):.4f},{max(ent):.4f},3,{MHZ},{cond},2026-10-11,{src},serial,"
          f"{q(f'trigger (EPWM zero) to the handler first line, including the ADC conversion: mean {g0[1]/10:.1f}, min {g0[0]}, max {g0[2]} cycles (run 1)')}")
    print(f"am263p,control_loop_worst_pct,{var},measured,{statistics.median(pct):.1f},{max(pct):.1f},3,{MHZ},{cond},2026-10-11,{src},serial,"
          f"{q('derived: worst trigger-to-new-value time / 20 us period; the worst is the first interrupt after load')}")
    print(f"am263p,control_loop_jitter,{var},measured,{statistics.median(jit):.3f},{max(jit):.3f},3,{MHZ},{cond},2026-10-11,{src},serial,"
          f"{q('derived: (worst - best) trigger-to-new-value cycles / 400 MHz, all 1,000 interrupts including the first')}")
