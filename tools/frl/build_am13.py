"""Build TI's AM13E230x gpio_toggle_output example for frl: the LED pattern edit and the compile and link.

The AM13E230x SDK ships this example with a CCStudio project and CMake but no makefile, so frl does here what
am13e230/blink.sh does with the same tools: SysConfig generates the board files from the example's
example.syscfg, TI Arm Clang compiles main.c and those files, and the link uses the SDK's prebuilt libraries and
linker command file. Paths come from toolfind, so the build runs wherever frl found the tools.
"""
import re
import subprocess
from pathlib import Path

from core import stop

SYSCFG_FILE = "am13e230x_lp/m33_nortos/example.syscfg"
ARCH = ["-mcpu=cortex-m33", "-march=armv8.1-m.main+cdecp0", "-mthumb", "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard"]
INCLUDES = ["source/compiler/m33_ti_arm_clang", "source/device/am13e230x/include", "source/device/am13e230x/include/hw",
            "ti_sdk_config/am13e230x/default/device_support/include", "source/arch/include", "source/arch/m33/include",
            "ti_sdk_config/am13e230x/default/arch_cfg", "ti_sdk_config/am13e230x/default/arch_cfg/m33", "source/cmsis/Core/Include",
            "source/driverlib/am13e230x", "source/utils/log/include", "ti_sdk_config/am13e230x/default/utils_cfg", "source",
            "examples/empty/am13e230x_lp"]
LIBS = ["libti_sdk_cfg_default_m33_ti_arm_clang.a", "libarch_ti_sdk_cfg_default_m33_ti_arm_clang.a",
        "libdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a", "libutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a",
        "libam13e230x_m33_ti_arm_clang.a"]
MARK = "/* Fox Red Labs pattern (frl prepare --pattern): LED on, off, on, off ... in ms */"
LOOP = re.compile(r"    while \(1\)\n    \{\n        DL_GPIO_togglePins\(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN\);\n\n"
                  r"        /\* 500ms delay after toggle \*/\n        DEVICE_DELAY_MS\(500\);\n    \}\n")
PATTERN_LOOP = """    /* The pattern drives LED4 red (GPIO_GRP_0 PIN_0, PA5); it starts off */
    DL_GPIO_clearPins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN);
    while (1)
    {
        for (unsigned i = 0; i < sizeof(pattern_ms) / sizeof(pattern_ms[0]); i++)
        {
            if (i % 2 == 0)
            {
                DL_GPIO_setPins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN);
            }
            else
            {
                DL_GPIO_clearPins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN);
            }
            DEVICE_DELAY_MS(pattern_ms[i]);
        }
    }
"""


def with_pattern(text, ms, src):
    """main.c with the pattern table and loop; a second run only replaces the table."""
    table = "static const uint32_t pattern_ms[] = {" + ", ".join(map(str, ms)) + "};"
    if MARK in text:
        return re.sub(r"static const uint32_t pattern_ms\[\] = \{[^}]*\};", table, text, count=1)
    if not LOOP.search(text) or "int main()" not in text:
        stop("refused", "FRL-E070", f"{src} has been changed by hand and has no toggle loop to replace",
             "set the times in it yourself, or move it aside and run prepare again to start from TI's example")
    text = text.replace("int main()", MARK + "\n" + table + "\n\nint main()", 1)
    return LOOP.sub(lambda m: PATTERN_LOOP, text, count=1)


def build(proj, t, r, log):
    """Compile and link the project in proj with the tools t (from toolfind); r is the route recipe. Returns the image path."""
    sdk, cc = Path(t["sdk"]["path"]), Path(t["compiler"]["path"])
    out, gen, obj = proj / "frl-build", proj / "frl-build" / "gen", proj / "frl-build" / "obj"
    for d in (gen, obj):
        d.mkdir(parents=True, exist_ok=True)
    lines = []

    def step(args, cwd=None):
        p = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=600, cwd=cwd)
        lines.append(" ".join(map(str, args)) + "\n" + p.stdout + p.stderr)
        log.write_text("\n".join(lines), encoding="utf-8")
        if p.returncode:
            tail = (p.stdout + p.stderr).strip().splitlines()[-12:]
            stop("failed", "FRL-E030", "the build failed: " + (tail[-1] if tail else "no output"),
                 f"read the first error in {log}; look it up in the board's AGENTS.md and FAQ.md", log=str(log), tail=tail)

    step([t["sysconfig"]["path"], "--product", sdk / ".metadata" / "product.json", "--quiet", "--output", gen, proj / SYSCFG_FILE])
    inc = [f"-I{sdk / i}" for i in INCLUDES] + [f"-I{gen}"]
    for c in [proj / "main.c", *sorted(gen.glob("*.c"))]:
        step([cc / "bin" / "tiarmclang", "-O2", *ARCH, "-g", "-Wall", *inc, "-c", c, "-o", obj / (c.stem + ".o")])
    image = out / "blink.out"
    step([cc / "bin" / "tiarmclang", *ARCH, *sorted(obj.glob("*.o")), f"-Wl,-i{sdk / 'build/am13e230x/lib/Release'}",
          *[f"-l{x}" for x in LIBS], sdk / "ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd",
          "-Wl,--rom_model", "-Wl,--reread_libs", "-Wl,--diag_suppress=10063", f"-Wl,-m,{out / 'blink.map'}",
          f"-L{cc / 'lib'}", "-llibc.a", "-o", image])
    return image
