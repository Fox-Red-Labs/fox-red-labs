#!/usr/bin/env python3
"""Build one of TI's edgeAI examples for the LP-MSPM33C321A with the cycle counter added around its feature
extraction and its model run (the DWT counter, results in the RAM struct `ai`), printf turned off.
   build_ai.py <example name> [O2|O3]     output <run>/ai-<name>/ticlang/<name>.out
TI's example source is copied, never edited in place. The patch only adds timing; the model, its input and
its golden-output check are TI's."""
import os, re, shutil, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SDK = "/Applications/ti/mspm33_sdk_1_04_00_00"
CC = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
R = os.environ.get("RUNDIR", os.path.abspath("out/mspm33c321a"))
name = sys.argv[1]; opt = sys.argv[2] if len(sys.argv) > 2 else "O2"
src = f"{SDK}/examples/nortos/LP_MSPM33C321A/edgeAI/{name}"; dst = f"{R}/ai-{name}"
shutil.rmtree(dst, ignore_errors=True); shutil.copytree(src, dst)
shutil.copy(os.path.join(HERE, "..", "bench", "m33_timer.h"), dst)
p = f"{dst}/{name}.c"; s = open(p).read()
hdr = '''#include "m33_timer.h"
#define printf(...) ((void)0)
volatile struct { uint32_t magic, cpu_hz, fe_runs, fe_cycles, runs, run_cycles, run_first, errors; } ai;
#define AI_FE(x)  do { uint32_t t0_ = DWT->CYCCNT; x; ai.fe_cycles = DWT->CYCCNT - t0_; ai.fe_runs++; } while (0)
#define AI_RUN(x) do { uint32_t t0_ = DWT->CYCCNT; x; ai.run_cycles = DWT->CYCCNT - t0_; if (!ai.runs) ai.run_first = ai.run_cycles; ai.runs++; } while (0)
'''
i = s.index('#include "ti_msp_dl_config.h"') + len('#include "ti_msp_dl_config.h"')
s = s[:i] + "\n#include <stdint.h>\n" + hdr + s[i:]
s, n1 = re.subn(r'SYSCFG_DL_init\(\);', 'SYSCFG_DL_init(); bench_timer_start(); ai.cpu_hz = CPUCLK_FREQ;', s, count=1)
s, n2 = re.subn(r'\b(tvmgen_default_run\([^;]*\));', r'AI_RUN(\1);', s)
s, n3 = re.subn(r'\b(FE_runFeatureExtract\([^;]*\));', r'AI_FE(\1);', s)
# the end: mark done before the final spin; golden-output errors, when the example computes them, go to ai.errors
s, n4 = re.subn(r'(\n\s*)while\s*\(1\)\s*\n?\s*\{', r'\1ai.magic = 0xA1C0DE01u;\1while (1)\1{', s, count=1)
if n1 != 1 or n2 < 1 or n4 != 1:
    print("patch did not apply", name, n1, n2, n3, n4); sys.exit(2)
open(p, "w").write(s)
mk = f"{dst}/ticlang/makefile"; t = open(mk).read(); t = t.replace("-O2", f"-{opt}"); open(mk, "w").write(t)
r = subprocess.run(["make", "-C", f"{dst}/ticlang", f"MSPM33_SDK_INSTALL_DIR={SDK}", "SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh",
                    f"TICLANG_ARMCOMPILER={CC}", "GCC_ARMCOMPILER="], capture_output=True, text=True)
open(f"{dst}/build.log", "w").write(r.stdout + r.stderr)
if r.returncode:
    print((r.stdout + r.stderr)[-800:]); sys.exit(1)
mp = [f for f in os.listdir(f"{dst}/ticlang") if f.endswith(".map")][0]
bad = [l for l in open(f"{dst}/ticlang/{mp}") if re.match(r"^ +(BCR|BSL)_CONFIG ", l) and l.split()[3] != "00000000"]
print(name, "built", "NONMAIN USED" if bad else "", f"(AI_RUN x{n2}, AI_FE x{n3})")
