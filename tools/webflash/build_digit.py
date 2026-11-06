#!/usr/bin/env python3
"""Build the browser bench's draw-a-digit program (BitNetMCU cnn_64) for MSPM0 LaunchPad kits.

  python3 tools/webflash/build_digit.py [--model cnn_64,12k,...] [board ...]   default: every model, mspm0l1306 mspm0g3519 mspm0g5187
  python3 tools/webflash/build_digit.py --notices [board]  only add BitNetMCU's GPL-3.0 notice to images already built

One image per model (BitNetMCU picks the model at build time): cnn_64 is prebuilt/<board>/digit, the others
prebuilt/<board>/digit-<model>. The bench's "Pick an AI model" stage lists them.

Same route as build_board.py (TI's gpio_toggle_output example, its main source replaced, the example's makefile
and the validated tools), plus BitNetMCU's inference code and model copied into the project and a bigger stack
(the CNN keeps about 2 kB of activations on it; ports/bitnetmcu/mspm0/build.sh uses 3072, and 3600 on the L1306).
The address of the mailbox (struct dg in tools/webflash/player/digit.c) is read from the symbol table before
the image is stripped and goes in the manifest. GPL-3.0 code: the image is for private use until cleared.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_board  # noqa: E402
import params  # noqa: E402
import build_sensor  # noqa: E402,F401  (adds the L1306 to BOARDS)
from boards_mspm0 import BOARDS  # noqa: E402

ROOT = build_board.ROOT
SRC = build_board.PLAYER / "digit.c"
BITNET = ROOT / "third-party" / "bitnetmcu"
RUN = ROOT / "ports" / "bitnetmcu" / "bitnet_run.c"
MODELS = ["cnn_64", "cnn_16small", "12k", "1k"]
# the CNNs keep about 2 kB of activations on the stack; the fully connected models (12k, 1k) keep two small arrays
STACK = {"cnn": {"mspm0l1306": 3600, "mspm0g3519": 3072, "mspm0g5187": 3072}, "fc": {"mspm0l1306": 1536, "mspm0g3519": 1536, "mspm0g5187": 1536}}


def stack_for(board, model):
    return STACK["cnn" if model.startswith("cnn") else "fc"][board]


def dirname(model):
    return "digit" if model == "cnn_64" else f"digit-{model}"


def build(board, model):
    b = BOARDS[board]
    t = {k: build_board.toolfind.find(k, v) for k, v in build_board.VALID.items()}
    tmp = Path(build_board.tempfile.mkdtemp(prefix=f"frl-digit-{board}-"))
    proj = tmp / "p"
    shutil.copytree(Path(t["mspm0_sdk"]["path"]) / b["example"], proj)
    for junk in ("iar", "keil", "gcc"):
        shutil.rmtree(proj / junk, ignore_errors=True)
    notice = (proj / "gpio_toggle_output.c").read_text().split("*/", 1)[0] + "*/\n"
    (proj / "gpio_toggle_output.c").write_text(f'#define MODEL_H "BitNetMCU_model_{model}.h"\n' + SRC.read_text())
    (proj / "board_leds.h").write_text(build_board.header(b))
    for f in ("BitNetMCU_inference.c", "BitNetMCU_inference.h", f"BitNetMCU_model_{model}.h", "BitNetMCU_MNIST_test_data.h"):
        shutil.copy2(BITNET / f, proj / f)
    shutil.copy2(RUN, proj / "bitnet_run.c")
    shutil.copy2(build_board.PLAYER / "frl_params.h", proj / "frl_params.h")
    (proj / "frl_params_gen.h").write_text(params.gen_header("digit"))
    # SysConfig rewrites device_linker.cmd on every build, so the stack goes on the link line after it (the last wins)
    mk = proj / "ticlang" / "makefile"
    mk.write_text(mk.read_text().replace("../ticlang/device_linker.cmd \\", f"../ticlang/device_linker.cmd -Wl,--stack_size={stack_for(board, model)} \\", 1))
    args = [t["make"]["exe"], "-C", str(proj / "ticlang"), f"MSPM0_SDK_INSTALL_DIR={t['mspm0_sdk']['path']}",
            f"SYSCONFIG_TOOL={t['sysconfig']['path']}", f"TICLANG_ARMCOMPILER={t['ticlang']['path']}", "GCC_ARMCOMPILER="]
    p = subprocess.run(args, capture_output=True, text=True, timeout=900)
    img = proj / "ticlang" / "gpio_toggle_output.out"
    if p.returncode or not img.is_file():
        print((p.stdout + p.stderr)[-2500:])
        raise SystemExit(f"build failed ({board})")
    m = (proj / "ticlang" / "gpio_toggle_output.map").read_text()
    memory = build_board.map_memory(m)
    stack = re.search(r"^\.stack\s+\S+\s+\S+\s+([0-9a-f]+)", m, re.M)
    print(f"   {board}: .stack {int(stack.group(1), 16) if stack else '?'} bytes;", " ".join(l.strip() for l in m.splitlines() if re.match(r"\s+(FLASH|SRAM)\s", l)))
    nm = Path(t["ticlang"]["path"]) / "bin" / "tiarmnm"
    sym = subprocess.run([str(nm), str(img)], capture_output=True, text=True).stdout
    addr = next(int(l.split()[0], 16) for l in sym.splitlines() if l.split()[-1:] == ["dg"])
    subprocess.run([str(Path(t["ticlang"]["path"]) / "bin" / "tiarmstrip"), "--strip-all", str(img)], check=True)
    return img, t, notice, tmp, addr, memory


BITNET = ROOT / "third-party" / "bitnetmcu"


def add_bitnet_notice(board, model="cnn_64"):
    """The image links BitNetMCU's GPL-3.0 inference code and model: its NOTICE.txt carries that license as well as
    TI's notice, and the manifest says so (found in the 2026-10-16 site review: the notice named only TI's)."""
    out = ROOT / "prebuilt" / board / dirname(model)
    n = out / "NOTICE.txt"
    text = n.read_text()
    if "BitNetMCU" not in text:
        text += ("\n\n---\n\nThis firmware also contains BitNetMCU by cpldcpu (https://github.com/cpldcpu/BitNetMCU, commit 0715bfc), "
                 f"its inference code (BitNetMCU_inference.c, .h) and its {model} model, unchanged, licensed under the GNU General "
                 "Public License version 3. The firmware as a whole is therefore under GPL-3.0; its source is "
                 "tools/webflash/player/digit.c and third-party/bitnetmcu/ in the Fox Red Labs repository. The license text:\n\n"
                 + (BITNET / "LICENSE").read_text())
        n.write_text(text)
    m = out / "manifest.json"
    man = json.loads(m.read_text())
    man["release_note"] = ("GPL-3.0 as a whole: BitNetMCU's inference code and model (GPL-3.0) on TI's example (BSD-3-Clause), "
                           "both notices in NOTICE.txt, linked with TI Arm Clang's runtime. Not cleared for public distribution.")
    m.write_text(json.dumps(man, indent=1) + "\n")


def main():
    args = sys.argv[1:]
    if args[:1] == ["--notices"]:       # add the notice to images already built, without rebuilding them
        for board in args[1:] or list(STACK["cnn"]):
            for model in MODELS:
                if (ROOT / "prebuilt" / board / dirname(model) / "NOTICE.txt").exists():
                    add_bitnet_notice(board, model)
            print(f"notice: prebuilt/{board}/digit*/NOTICE.txt carries BitNetMCU's GPL-3.0")
        return
    models = MODELS
    if args[:1] == ["--model"]:
        models, args = args[1].split(","), args[2:]
    for board in args or list(STACK["cnn"]):
        for model in models:
            img, t, notice, tmp, addr, memory = build(board, model)
            try:
                dg = {"addr": f"0x{addr:08X}", "magic": "0xD1617001", "model": model, "input": "16 x 16 int8, BitNetMCU's format",
                      "layout": ["magic", "cpuclk_hz", "selftest", "n_test", "seq_in", "seq_out", "pred", "cycles", "input[256]"]}
                build_board.write(board, dirname(model), img, t, notice, tmp, "digit",
                                  {"digit": dg, "memory": memory, "params": params.manifest_block("digit", BOARDS[board]["table"]),
                                   "expect": "self-test 10 of 10, then one answer per drawing the page writes; LED2 red flashes once per answer"},
                                  SRC, f"the example's main source replaced by the draw-a-digit program with BitNetMCU {model}; stack {stack_for(board, model)} bytes",
                                  f"python3 tools/webflash/build_digit.py --model {model} {board}")
                add_bitnet_notice(board, model)
                print(f"   {board} {model}: mailbox dg at 0x{addr:08X}")
            finally:
                shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
