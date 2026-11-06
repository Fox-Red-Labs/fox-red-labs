#!/usr/bin/env python3
"""Decode a 72-byte frl_result block read from a board.   python3 tools/decode_result.py result.bin
Read it with the probe, for example (address from `arm-none-eabi-nm wasm3.elf | grep frl_result`):
  DSLite memory -c <board>.ccxml -r 0x<addr>,72 -s 8 -o result.bin"""
import struct, sys
n = ["magic", "version", "status", "cpu_hz", "blinks", "load_us", "run_overhead_us", "fib_n", "fib_val", "fib_wasm_us", "fib_native_us",
     "bench_n", "bench_val", "bench_wasm_us", "bench_native_us", "heap_used", "heap_size", "wasm_bytes"]
v = struct.unpack("<18I", open(sys.argv[1], "rb").read()[:72])
for k, x in zip(n, v): print(f"{k:16s}{x}" + ("  (0x%x)" % x if k == "magic" else ""))
ok = v[0] == 0x4D335746 and v[2] == 0 and v[8] == 6765 and v[12] == 3486216529
print("PASS" if ok else "FAIL (expect magic 0x4d335746, status 0, fib_val 6765, bench_val 3486216529 for fib 20 / bench 20000)")
