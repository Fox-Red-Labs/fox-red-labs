#!/usr/bin/env python3
"""Decode a RAM struct printed by run_image.py (stdin: 'name @0xaddr: hex') into named words.
   decode_ram.py rtos|ctrl|io|ai: layouts match bench/rtos/main.c, bench/ctrl/main.c, bench/edumkii/inputs/main.c and tools/build_ai.py"""
import struct, sys
kind = sys.argv[1]
first = {"rtos": "fr_bench", "ctrl": "r", "io": "io", "ai": "ai"}[kind]
raw = sys.stdin.read().splitlines()
line = [l for l in raw if l.startswith(first + " @0x") and ": " in l][-1]
for l in raw:
    if l.startswith("test_result @"):
        print("test_result=%d" % struct.unpack("<I", bytes.fromhex(l.split(": ", 1)[1])[:4])[0])
b = bytes.fromhex(line.split(": ", 1)[1]); w = struct.unpack("<%dI" % (len(b) // 4), b)
if kind == "rtos":
    names = ["magic", "samples", "tim_counts", "systick_cycles", "timer_read"]
    for n in ["yield", "sem_wake", "notify_wake", "queue_rt", "irq_entry", "irq_sem_task", "irq_notify_task"]:
        names += [n + "_best", n + "_worst", n + "_worst_no_tick", n + "_sum"]
    names += ["errors", "stack0", "stack1", "stack2", "stack3", "hwm0", "hwm1", "hwm2", "hwm3", "ticks_end"]
elif kind == "ctrl":
    names = ["magic", "cpuclk_hz", "tim_per_100k_cyc", "period", "load", "entry_min", "entry_max", "exit_min", "exit_max", "entry_sum", "exit_sum", "irqs", "span_max", "skipped", "timeout", "last_adc", "last_da", "exit_first", "exit_min_rest", "adc_min", "adc_max"]
elif kind == "io":
    names = ["magic", "cpu_hz", "n", "seq_cycles"] + [f"{k}{c}" for k in ("mean_x16_", "min_", "max_", "sd_x100_") for c in range(6)]
    names += [f"btn_nopull{i}" for i in range(3)] + [f"btn_pulldown{i}" for i in range(3)] + [f"btn_pullup{i}" for i in range(3)] + [f"out_low{i}" for i in range(3)] + [f"out_high{i}" for i in range(3)]
    names += ["tone_high", "tone_low", "tone_half_periods"] + [f"raw_last{c}" for c in range(6)]
elif kind == "ai":
    names = ["magic", "cpu_hz", "fe_runs", "fe_cycles", "runs", "run_cycles", "run_first", "errors"]
for n, v in zip(names, w):
    if n == "last_da": v = struct.unpack("<f", struct.pack("<I", v))[0]
    print(f"{n}={v}")
