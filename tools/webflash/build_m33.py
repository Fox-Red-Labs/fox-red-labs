#!/usr/bin/env python3
"""Build the browser bench's blink for the Cortex-M33 LaunchPad kits: LP-MSPM33C321A and LP-AM13E230.

  python3 tools/webflash/build_m33.py [ms]        default 250: the LEDs toggle every 250 ms

The build steps of mspm33c321a/blink.sh (TI's gpio_toggle_output and its own makefile) and am13e230/blink.sh
(SysConfig, then TI Arm Clang with the SDK's prebuilt libraries), with no flash step: the page writes the image.
Writes prebuilt/<board>/blink-<ms>-<ms>/ with a manifest shaped like the MSPM0 ones, and refuses an image with
any loadable byte outside main flash (the boot configuration and factory regions are never written).
"""
import datetime as dt
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_board import segments  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CC = Path("/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS")
SYSCFG = Path("/Applications/ti/sysconfig_1.28.0")
BOARDS = {
    "mspm33c321a": {"kit": "LP-MSPM33C321A", "sdk": Path("/Applications/ti/mspm33_sdk_1_04_00_00"), "flash_end": 0x100000, "alias": 0x10000000,
                    "led": {"label": "LED2 blue (PA2)", "gpio": "0x400F0000", "bit": 2}, "active_high": True},   # port C reads 0 from the debugger
    "am13e230": {"kit": "LP-AM13E230", "sdk": Path("/Applications/ti/am13e230x_sdk_26_01_00_03"), "flash_end": 0x80000,
                 "led": {"label": "LED4 red (PA5)", "gpio": "0x400F0000", "bit": 5}, "active_high": True},
}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def run(args, **kw):
    p = subprocess.run(args, capture_output=True, text=True, timeout=900, **kw)
    if p.returncode:
        print((p.stdout + p.stderr)[-2500:])
        raise SystemExit("build step failed: " + " ".join(map(str, args))[:200])
    return p


def build_mspm33(b, ms, tmp):
    ex = b["sdk"] / "examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output"
    out = tmp / "p"
    shutil.copytree(ex, out)
    src = out / "gpio_toggle_output.c"
    text = src.read_text()
    if "#define DELAY (16000000)" not in text:
        raise SystemExit("the example's DELAY line changed; read it before building")
    src.write_text(text.replace("#define DELAY (16000000)", f"#define DELAY ({ms * 32000})"))   # 32 MHz, as blink.sh
    run(["make", "-C", str(out / "ticlang"), f"MSPM33_SDK_INSTALL_DIR={b['sdk']}", f"SYSCONFIG_TOOL={SYSCFG / 'sysconfig_cli.sh'}",
         f"TICLANG_ARMCOMPILER={CC}", "GCC_ARMCOMPILER="])
    return out / "ticlang" / "gpio_toggle_output.out", ex, "gpio_toggle_output.c", f"#define DELAY ({ms * 32000})"


def build_am13(b, ms, tmp):
    sdk = b["sdk"]
    ex = sdk / "examples/driverlib/gpio/gpio_toggle_output"
    gen, obj = tmp / "gen", tmp / "obj"
    gen.mkdir(); obj.mkdir()
    run([str(SYSCFG / "nodejs/node"), str(SYSCFG / "dist/cli.js"), "--product", str(sdk / ".metadata/product.json"), "--quiet",
         "--treatWarningsAsErrors", "--output", str(gen), str(ex / "am13e230x_lp/m33_nortos/example.syscfg")])
    text = (ex / "main.c").read_text()
    if "DEVICE_DELAY_MS(500)" not in text:
        raise SystemExit("the example's delay line changed; read it before building")
    (tmp / "main.c").write_text(text.replace("DEVICE_DELAY_MS(500)", f"DEVICE_DELAY_MS({ms})"))
    arch = ["-mcpu=cortex-m33", "-march=armv8.1-m.main+cdecp0", "-mthumb", "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard"]
    cfg = sdk / "ti_sdk_config/am13e230x/default"
    inc = [f"-I{p}" for p in (sdk / "source/compiler/m33_ti_arm_clang", sdk / "source/device/am13e230x/include",
                              sdk / "source/device/am13e230x/include/hw", cfg / "device_support/include", sdk / "source/arch/include",
                              sdk / "source/arch/m33/include", cfg / "arch_cfg", cfg / "arch_cfg/m33", sdk / "source/cmsis/Core/Include",
                              sdk / "source/driverlib/am13e230x", sdk / "source/utils/log/include", cfg / "utils_cfg", sdk / "source",
                              gen, sdk / "examples/empty/am13e230x_lp")]
    for f in [tmp / "main.c", *sorted(gen.glob("*.c"))]:
        run([str(CC / "bin/tiarmclang"), "-O2", *arch, "-g", "-Wall", *inc, "-c", str(f), "-o", str(obj / (f.stem + ".o"))])
    lib = sdk / "build/am13e230x/lib/Release"
    img = tmp / "blink.out"
    run([str(CC / "bin/tiarmclang"), *arch, *sorted(str(o) for o in obj.glob("*.o")), f"-Wl,-i{lib}",
         "-llibti_sdk_cfg_default_m33_ti_arm_clang.a", "-llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a",
         "-llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a", "-llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a",
         "-llibam13e230x_m33_ti_arm_clang.a", str(cfg / "linker_m33_ti_arm_clang.cmd"), "-Wl,--rom_model", "-Wl,--reread_libs",
         "-Wl,--diag_suppress=10063", f"-L{CC / 'lib'}", "-llibc.a", "-o", str(img)])
    return img, ex, "main.c", f"DEVICE_DELAY_MS({ms})"


def main():
    ms = int(sys.argv[1]) if len(sys.argv) > 1 else 250
    for board, b in BOARDS.items():
        with tempfile.TemporaryDirectory(prefix=f"frl-{board}-") as t:
            tmp = Path(t)
            img, ex, srcfile, edited = (build_mspm33 if board == "mspm33c321a" else build_am13)(b, ms, tmp)
            run([str(CC / "bin/tiarmstrip"), "--strip-all", str(img)])
            segs = segments(img)
            al = b.get("alias", 0)   # TI links the MSPM33 at its secure alias of main flash, 0x10000000 (reads the same as 0)
            outside = [s for s in segs if not (int(s["addr"], 16) + s["bytes"] <= b["flash_end"]
                                               or al <= int(s["addr"], 16) and int(s["addr"], 16) + s["bytes"] <= al + b["flash_end"])]
            if outside:
                raise SystemExit(f"{board}: loadable data outside main flash, refused: {outside}")
            out = ROOT / "prebuilt" / board / f"blink-{ms}-{ms}"
            out.mkdir(parents=True, exist_ok=True)
            shutil.copy2(img, out / "firmware.out")
            notice = (ex / srcfile).read_text().split("*/", 1)[0] + "*/\n"
        commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        man = {"format": "frl-prebuilt-1", "board": board, "kit": b["kit"], "recipe": "blink-pattern",
               "revisions_tested": ["unknown: the board revision was not recorded for this build"],
               "pattern_ms": [ms, ms], "led": b["led"], "active_high": b["active_high"],
               "image": {"file": "firmware.out", "sha256": sha(out / "firmware.out"), "bytes": (out / "firmware.out").stat().st_size, "segments": segs},
               "source": {"repo_commit": commit, "example": str(ex.relative_to(b["sdk"])), "file": srcfile, "edited_line": edited,
                          "rebuild": f"python3 tools/webflash/build_m33.py {ms}"},
               "tools": {"sdk": b["sdk"].name, "compiler": CC.name, "sysconfig": SYSCFG.name},
               "made": dt.datetime.now().isoformat(timespec="seconds"), "release_cleared": False,
               "release_note": "TI's example (BSD-3-Clause, NOTICE.txt) with one line changed, linked with TI's libraries. Not cleared for public distribution."}
        (out / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
        (out / "NOTICE.txt").write_text(f"TI's {ex.name} example for the {b['kit']} from {b['sdk'].name}, one line changed. TI's notice:\n\n" + notice)
        print(f"OK {out.relative_to(ROOT)}: {segs}")


if __name__ == "__main__":
    main()
