#!/usr/bin/env python3
"""Make LP-AM263P copies of TI's AI examples in three memory layouts, with cycle timing, under bench/xip/ai/<layout>/<example>/.
  ocram   : TI's layout unchanged (code and constants in on-chip RAM)
  xip     : .text and .rodata in flash at 0x60100000 (blank in our 2026-10-09 backup), MPU region 0x60000000 cached, no L2
  xip64k  : as xip, plus TI's RL2 (OptiFlash L2) cache of 64 kB over the flash range, cache bank at 0x70080000
The example's own C is TI's, with two lines swapped for the timed versions in bench/xip/ai/frl_ai_timing.h.
ai_xip_gen.py [example ...]   (default: the twelve examples with test vectors and no sensor)"""
import os, re, shutil, sys
SDK = os.path.expanduser("~/ti/mcu_plus_sdk_am263px_26_01_00_08")
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(HERE, "bench", "xip", "ai")
EXAMPLES = ["arc_fault", "blower_imbalance", "fan_blade_anomalydetection", "forecasting_pmsm_rotor_temp",
            "generic_timeseries_anomalydetection", "generic_timeseries_classification", "generic_timeseries_forecasting",
            "generic_timeseries_regression", "hvac_indoor_temp_forecast", "motor_fault", "torque_measurement",
            "washing_machine_load_weighing"]

MPU_FLASH = """
mpu_armv7frl.$name             = "CONFIG_MPU_REGION_FRL_FLASH";
mpu_armv7frl.baseAddr          = 0x60000000;
mpu_armv7frl.size              = 27;
mpu_armv7frl.accessPermissions = "Supervisor RD, User RD";
"""
RL2 = """
RL21.$name                                  = "RL20";
RL21.enable                                 = true;
RL21.memoryRegion                           = "L2_CACHE_BANK";
RL21.rangeStart                             = 0x60100000;
RL21.rangeEnd_2                             = 0x60180000;
RL21.cacheSize                              = "RL2_CACHESIZE_64K";
RL21.memory_section.group                   = scripting.forceWrite(true);
RL21.memory_section.load_to_memory          = scripting.forceWrite("Memory");
RL21.memory_section.select_multiple_regions = scripting.forceWrite(false);
RL21.memory_section.split_across_memories   = scripting.forceWrite(false);
"""

def patch_syscfg(t, layout):
    if layout == "ocram":
        return t
    # code and read-only data to flash
    sec = re.search(r'(section\d+)\.\$name\s*=\s*"Code and Read-Only Data";', t)
    assert sec, "Code and Read-Only Data section not found"
    t, n = re.subn(re.escape(sec.group(1)) + r'(\.load_memory\s*=\s*)"OCRAM";', sec.group(1) + r'\1"FLASH";', t)
    assert n == 1, "its load_memory line not found"
    # one more MPU region: the flash window, cached and executable, read only
    # after the last existing instance: on the R5F the highest-numbered MPU region wins, and region 0 is a 4 GB
    # device, no-execute region, so the flash region must come after it
    last = list(re.finditer(r"const mpu_armv7\d+\s*=\s*mpu_armv7\.addInstance\(\);\n", t))[-1]
    t = t[:last.end()] + "const mpu_armv7frl = mpu_armv7.addInstance();\n" + t[last.end():]
    t = t.replace("\ndefault_linker1.$name", MPU_FLASH + "\ndefault_linker1.$name", 1)
    if layout == "xip64k":
        m = re.search(r"region1\.memory_region\.create\((\d+)\);", t); assert m; n = int(m.group(1))
        t = t.replace(m.group(0), f"region1.memory_region.create({n + 1});", 1)
        t = t.replace("\nsection1.", f"""
region1.memory_region[{n}].$name              = "L2_CACHE_BANK";
region1.memory_region[{n}].auto               = false;
region1.memory_region[{n}].manualStartAddress = 0x70080000;
region1.memory_region[{n}].size               = 0x10000;
""" + "\nsection1.", 1)
        t = t.replace("const region ", 'const RL2             = scripting.addModule("/optiflash/RL2/RL2", {}, false);\nconst RL21            = RL2.addInstance();\nconst region ', 1)
        t = t.rstrip() + "\n" + RL2
    return t

def make(ex, layout):
    src = os.path.join(SDK, "examples", "ai", ex); dst = os.path.join(OUT, layout, ex)
    shutil.rmtree(dst, ignore_errors=True)
    os.makedirs(os.path.join(dst, "am263px-lp", "r5fss0-0_nortos", "ti-arm-clang"))
    for f in ("test_vector.c", "user_input_config.h"):
        shutil.copy(os.path.join(src, f), dst)
    shutil.copytree(os.path.join(src, "artifacts"), os.path.join(dst, "artifacts"))
    c = open(os.path.join(src, ex + ".c")).read()
    c = c.replace('#include "tvmgen_default.h"', '#include "tvmgen_default.h"\n#include "../../frl_ai_timing.h"', 1)
    for a, b in (("    FE_runFeatureExtract(fe_handle);", "    FRL_FE_RUN(fe_handle);"),
                 ("    tvmgen_default_run(&inputs, &outputs);", "    FRL_INFER(&inputs, &outputs);")):
        assert c.count(a) == 1, (ex, a); c = c.replace(a, b)
    open(os.path.join(dst, ex + ".c"), "w").write(c)
    p = os.path.join(src, "am263px-lp", "r5fss0-0_nortos"); q = os.path.join(dst, "am263px-lp", "r5fss0-0_nortos")
    shutil.copy(os.path.join(p, "main.c"), q)
    open(os.path.join(q, "example.syscfg"), "w").write(patch_syscfg(open(os.path.join(p, "example.syscfg")).read(), layout))
    for f in ("makefile", "syscfg_c.rov.xs"):
        t = open(os.path.join(p, "ti-arm-clang", f)).read()
        if f == "makefile":
            t = re.sub(r"^export MCU_PLUS_SDK_PATH\?=.*$", "export MCU_PLUS_SDK_PATH?=$(HOME)/ti/mcu_plus_sdk_am263px_26_01_00_08", t, flags=re.M)
            t = t.replace("../../../../../../source", "$(MCU_PLUS_SDK_PATH)/source")
        open(os.path.join(q, "ti-arm-clang", f), "w").write(t)
    return q

def make_coremark(layout):
    """TI's coremark_benchmark (am263px-lp, r5fss0-0 nortos) in the same three layouts, under bench/xip/cm/<layout>/."""
    src = os.path.join(SDK, "examples", "benchmarks", "coremark_benchmark")
    dst = os.path.join(HERE, "bench", "xip", "cm", layout, "coremark_benchmark")
    shutil.rmtree(dst, ignore_errors=True)
    q = os.path.join(dst, "am263px-lp", "r5fss0-0_nortos"); os.makedirs(os.path.join(q, "ti-arm-clang"))
    for f in os.listdir(src):
        if f.endswith((".c", ".h")): shutil.copy(os.path.join(src, f), dst)
    p = os.path.join(src, "am263px-lp", "r5fss0-0_nortos")
    shutil.copy(os.path.join(p, "main.c"), q)
    open(os.path.join(q, "example.syscfg"), "w").write(patch_syscfg(open(os.path.join(p, "example.syscfg")).read(), layout))
    for f in ("makefile", "syscfg_c.rov.xs"):
        t = open(os.path.join(p, "ti-arm-clang", f)).read()
        if f == "makefile":
            t = re.sub(r"^export MCU_PLUS_SDK_PATH\?=.*$", "export MCU_PLUS_SDK_PATH?=$(HOME)/ti/mcu_plus_sdk_am263px_26_01_00_08", t, flags=re.M)
        open(os.path.join(q, "ti-arm-clang", f), "w").write(t)
    return q

if __name__ == "__main__":
    if sys.argv[1:] == ["coremark"]:
        for layout in ("ocram", "xip", "xip64k"):
            print(make_coremark(layout))
        sys.exit(0)
    for ex in sys.argv[1:] or EXAMPLES:
        for layout in ("ocram", "xip", "xip64k"):
            print(make(ex, layout))
