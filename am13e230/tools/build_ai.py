#!/usr/bin/env python3
"""Build one of TI's AM13E2x SDK examples/ai/<name> by hand (TI Arm Clang, the projectspec's Release flags: -O3 -ffast-math -finline-functions),
with one patch: the example's own FE and inference cycle counts (Arch_Counter, the CPU cycle counter) are copied into a RAM struct `ai`
before the example prints them, so they can be read over the probe. The model, input and golden-output check are TI's.
   build_ai.py <example>      output <run>/ai-<name>/ai.out   (gen-ai-<name>/ holds the SysConfig output)"""
import os, re, shutil, subprocess, sys, glob
SDK = "/Applications/ti/am13e230x_sdk_26_01_00_03"; CC = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
R = os.environ.get("RUNDIR", os.path.abspath("out/am13e230")); name = sys.argv[1]
ex = f"{SDK}/examples/ai/{name}"; dst = f"{R}/ai-{name}"; gen = f"{R}/gen-ai-{name}"
shutil.rmtree(dst, ignore_errors=True); os.makedirs(dst + "/artifacts")
shutil.rmtree(gen, ignore_errors=True); os.makedirs(gen)
for f in ("main.c", "test_vector.c", "user_input_config.h"):
    if os.path.exists(f"{ex}/{f}"): shutil.copy(f"{ex}/{f}", dst)
for f in glob.glob(f"{ex}/artifacts/*"): shutil.copy(f, dst + "/artifacts")
s = open(f"{dst}/main.c").read()
s, n = re.subn(r"(\n\s*)totalCycles = inferenceCycles \+ feCycles;", r"\1ai.fe = feCycles; ai.inf = inferenceCycles; ai.err = (uint32_t)error; ai.iters = APP_AI_NUM_ITERATIONS; ai.magic = 0xA1C0DE01u;\1totalCycles = inferenceCycles + feCycles;", s, count=1)
if n != 1: print("patch did not apply", name); sys.exit(2)
i = s.index('#include "ti_sdk_dl_config.h"') + len('#include "ti_sdk_dl_config.h"')
s = s[:i] + "\nvolatile struct { uint32_t magic, fe, inf, err, iters; } ai;" + s[i:]
open(f"{dst}/main.c", "w").write(s)
node = "/Applications/ti/sysconfig_1.28.0/nodejs/node"
r = subprocess.run([node, "/Applications/ti/sysconfig_1.28.0/dist/cli.js", "--product", f"{SDK}/.metadata/product.json", "--quiet", "--output", gen,
                    f"{ex}/am13e230x_lp/m33_nortos/example.syscfg"], capture_output=True, text=True)
if r.returncode: print(r.stdout[-500:], r.stderr[-500:]); sys.exit(3)
ARCH = "-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard".split()
inc = [f"{SDK}/source/compiler/m33_ti_arm_clang", f"{SDK}/source/device/am13e230x/include", f"{SDK}/source/device/am13e230x/include/hw",
       f"{SDK}/ti_sdk_config/am13e230x/default/device_support/include", f"{SDK}/source/arch/include", f"{SDK}/source/arch/m33/include",
       f"{SDK}/ti_sdk_config/am13e230x/default/arch_cfg", f"{SDK}/ti_sdk_config/am13e230x/default/arch_cfg/m33", f"{SDK}/source/cmsis/Core/Include",
       f"{SDK}/source/cmsis/DSP/Include", f"{SDK}/source/ai/dap", f"{SDK}/source/ai/hann", f"{SDK}/source/ai/feature_extract",
       f"{SDK}/source/driverlib/am13e230x", f"{SDK}/source/utils/log/include", f"{SDK}/ti_sdk_config/am13e230x/default/utils_cfg", f"{SDK}/source", gen, dst, dst + "/artifacts"]
CF = ["-O3", "-ffast-math", "-finline-functions"] + ARCH + ["-gdwarf-3", "-w"] + ["-I" + i for i in inc]
srcs = [f"{dst}/main.c", f"{dst}/test_vector.c", f"{SDK}/source/ai/feature_extract/feature_extract.c"] + glob.glob(gen + "/*.c")
srcs = [x for x in srcs if os.path.exists(x)]
objs = []
for x in srcs:
    o = f"{dst}/{os.path.basename(x)[:-2]}.o"
    r = subprocess.run([f"{CC}/bin/tiarmclang"] + CF + ["-c", x, "-o", o], capture_output=True, text=True)
    if r.returncode: print(r.stderr[-800:]); sys.exit(4)
    objs.append(o)
L = f"-Wl,-i{SDK}/build/am13e230x/lib/Release"
libs = [L] + ["-llib%s.a" % n for n in ("ti_sdk_cfg_default_m33_ti_arm_clang", "ai_ti_sdk_cfg_default_m33_ti_arm_clang", "arch_ti_sdk_cfg_default_m33_ti_arm_clang",
       "driverlib_ti_sdk_cfg_default_m33_ti_arm_clang", "CMSISDSP_m33_ti_arm_clang", "utils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang", "am13e230x_m33_ti_arm_clang")]
cmd = [f"{CC}/bin/tiarmclang"] + ARCH + objs + libs + [f"{dst}/artifacts/mod_m33_ti_arm_clang.a", f"{SDK}/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd",
       "-Wl,--rom_model", "-Wl,--reread_libs", "-Wl,--diag_suppress=10063", f"-Wl,-m,{dst}/ai.map", f"-L{CC}/lib", "-llibc.a", "-o", f"{dst}/ai.out"]
r = subprocess.run(cmd, capture_output=True, text=True)
if r.returncode: print(r.stderr[-800:]); sys.exit(5)
bad = [l for l in open(f"{dst}/ai.map") if re.match(r"^ +(BCR|BSL)_CONFIG ", l) and l.split()[3] != "00000000"]
for o in objs: os.remove(o)
print(name, "built", "NONMAIN USED" if bad else "", os.path.getsize(f"{dst}/ai.out"))
