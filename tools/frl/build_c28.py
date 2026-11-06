"""Build TI's C2000Ware LED example for frl: the pattern edit, then SysConfig and the C2000 compiler, for flash.

The same steps as c2000-launchpad/tools/c2kbuild.sh FLASH, with paths from toolfind: SysConfig generates the board
files from led_ex1_blinky.syscfg, cl2000 compiles the example, the generated files and the code-start branch, and
the link uses the generated device_cmd.cmd (or the generic flash linker file) and C2000Ware's driverlib. Device
facts come from the route (c28: device folder, the define for the LaunchPad, the define for the family).
"""
import re
import subprocess
from pathlib import Path

from core import stop

MARK = "// Fox Red Labs pattern (frl prepare --pattern): LED on, off, on, off ... in ms"
LOOP = re.compile(r"    for\(;;\)\n    \{\n.*?GPIO_writePin\(myBoardLED0_GPIO, 0\);.*?DEVICE_DELAY_US\(500000\);.*?"
                  r"GPIO_writePin\(myBoardLED0_GPIO, 1\);.*?DEVICE_DELAY_US\(500000\);\n    \}\n", re.S)
PATTERN_LOOP = """    //
    // The pattern drives myBoardLED0, active low (written 0 to light it); it starts off
    //
    unsigned i;                                   // declared first: the C2000 compiler builds this example as C89
    GPIO_writePin(myBoardLED0_GPIO, 1);
    for(;;)
    {
        for(i = 0; i < sizeof(pattern_ms) / sizeof(pattern_ms[0]); i++)
        {
            GPIO_writePin(myBoardLED0_GPIO, (i % 2 == 0) ? 0 : 1);
            DEVICE_DELAY_US(pattern_ms[i] * 1000UL);
        }
    }
"""


def with_pattern(text, ms, src):
    table = "static const uint32_t pattern_ms[] = {" + ", ".join(map(str, ms)) + "};"
    if MARK in text:
        return re.sub(r"static const uint32_t pattern_ms\[\] = \{[^}]*\};", table, text, count=1)
    if not LOOP.search(text) or "void main(void)" not in text:
        stop("refused", "FRL-E070", f"{src} has been changed by hand and has no LED loop to replace",
             "set the times in it yourself, or move it aside and run prepare again to start from TI's example")
    text = text.replace("void main(void)", MARK + "\n" + table + "\n\nvoid main(void)", 1)
    return LOOP.sub(lambda m: PATTERN_LOOP, text, count=1)


def build(proj, t, r, log):
    """Compile and link proj for flash with the tools t (sdk = C2000Ware, compiler = cl2000, sysconfig). Returns the .out."""
    c2k, cgt = Path(t["sdk"]["path"]), Path(t["compiler"]["path"])
    c28 = r["c28"]
    dev = c28["device"]                                   # e.g. f280013x
    out, gen, obj = proj / "frl-build", proj / "frl-build" / "syscfg", proj / "frl-build" / "obj"
    for d in (gen, obj):
        d.mkdir(parents=True, exist_ok=True)
    lines = []

    def step(args):
        p = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=600, cwd=obj)
        lines.append(" ".join(map(str, args)) + "\n" + p.stdout + p.stderr)
        log.write_text("\n".join(lines), encoding="utf-8")
        if p.returncode:
            tail = (p.stdout + p.stderr).strip().splitlines()[-12:]
            stop("failed", "FRL-E030", "the build failed: " + (tail[-1] if tail else "no output"),
                 f"read the first error in {log}; look it up in the board's AGENTS.md and FAQ.md", log=str(log), tail=tail)

    defs = ["--define=_FLASH", "--define=generic_flash_lnk"]
    cflags = ["--opt_level=off", "-v28", "-ml", "-mt", "--float_support=fpu32", "--abi=eabi", "--gen_func_subsections=on",
              "--define=DEBUG", f"--define={c28['board_define']}", f"--define={c28['family_define']}=1", "--define=CPU1", *defs,
              "--diag_warning=225", "--diag_suppress=10063", f"--include_path={cgt / 'include'}", f"--include_path={c2k / 'device_support' / dev / 'common' / 'include'}",
              f"--include_path={c2k / 'driverlib' / dev / 'driverlib'}", f"--include_path={c2k}", f"--include_path={gen}", f"--include_path={proj}"]
    syscfg = next(proj.glob("*.syscfg"))
    step([t["sysconfig"]["path"], "--product", c2k / ".metadata" / "sdk.json", "-o", gen, syscfg])
    cflags += [f"--cmd_file={o}" for o in sorted(gen.glob("*.opt"))]   # SysConfig's own options (device_cmd.opt sets CMDTOOL: copy tables, not Ramfuncs symbols)
    lnk = gen / "device_cmd.cmd"
    if not lnk.is_file():
        lnk = c2k / "device_support" / dev / "common" / "cmd" / f"{dev[1:]}_generic_flash_lnk.cmd"
    srcs = [*sorted(proj.glob("*.c")), *sorted(gen.glob("*.c")), c2k / "device_support" / dev / "common" / "source" / f"{dev}_codestartbranch.asm"]
    for f in srcs:
        step([cgt / "bin" / "cl2000", *cflags, "-c", f, f"--output_file={Path(f).stem}.obj"])
    image = out / "blink.out"
    step([cgt / "bin" / "cl2000", *cflags, "-z", "--entry_point", "code_start", "--stack_size=0x200", "--heap_size=0x100", *defs,
          f"-i{cgt / 'lib'}", f"-i{c2k}", f"-i{gen}", "--reread_libs", "--rom_model", f"-m{out / 'blink.map'}", f"-o{image}",
          *sorted(str(p) for p in obj.glob("*.obj")), lnk, f"-l{c2k / 'driverlib' / dev / 'driverlib' / 'ccs' / 'Release' / 'driverlib.lib'}", "-llibc.a"])
    return image
