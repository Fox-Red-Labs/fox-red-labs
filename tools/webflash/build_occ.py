#!/usr/bin/env python3
"""Build the browser bench's room-occupancy images for MSPM0 LaunchPad kits: one of the lab's four CNN models per image.

  python3 tools/webflash/build_occ.py [--model occ_cnn_500,occ_cnn_2k,...] [board ...]
      default: every model that fits, mspm0l1306 mspm0g3519 mspm0g5187

Same route as build_digit.py (TI's gpio_toggle_output example, its main source replaced by player/occ.c, the model's
prebuilt O3 archive linked in). The mailbox address (struct oc in occ.c) comes from the symbol table before the image
is stripped and goes in the manifest; so do the 20 test windows' true head counts (the page draws the windows from
site/flash/bench/occ-windows.json, written by this script from the model's own test_vectors.h). prebuilt/<board>/occ-<model>/.
The L1306 (4 kB RAM) holds up to occ_cnn_8k; occ_cnn_30k needs 4224 bytes of scratch and does not link there (measured: results rows ai_occ_fit).
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_board  # noqa: E402
import build_sensor  # noqa: E402,F401  (adds the L1306 to BOARDS)
import params  # noqa: E402
from boards_mspm0 import BOARDS  # noqa: E402

ROOT = build_board.ROOT
SRC = build_board.PLAYER / "occ.c"
MODELS_DIR = ROOT / "mspm0g3507" / "mcu-bench" / "edgeai" / "uci-room-occupancy"
MODELS = ["occ_cnn_500", "occ_cnn_2k", "occ_cnn_8k", "occ_cnn_30k"]
FITS = {"mspm0l1306": MODELS[:3], "mspm0g3519": MODELS, "mspm0g5187": MODELS}
STACK = 1024


def windows(model):
    """The 20 test windows of a model: channel-major 4 x 32 floats and the true head count of each."""
    t = (MODELS_DIR / model / "test_vectors.h").read_text()
    x = re.search(rf"{model}_test_x\[20\]\[128\] = \{{(.*?)\n\}};", t, re.S).group(1)
    rows = [[float(v) for v in re.findall(r"-?\d+(?:\.\d+)?(?:e[-+]?\d+)?f?", r.replace("f", ""))] for r in re.findall(r"\{([^{}]*)\}", x)]
    label = [int(v) for v in re.search(rf"{model}_test_label\[20\] = \{{([^}}]*)\}}", t).group(1).split(",")]
    exp = [int(v) for v in re.search(rf"{model}_test_expected_class\[20\] = \{{([^}}]*)\}}", t).group(1).split(",")]
    assert len(rows) == 20 and all(len(r) == 128 for r in rows), (model, len(rows))
    return rows, label, exp


def build(board, model):
    b = BOARDS[board]
    t = {k: build_board.toolfind.find(k, v) for k, v in build_board.VALID.items()}
    tmp = Path(build_board.tempfile.mkdtemp(prefix=f"frl-occ-{board}-"))
    proj = tmp / "p"
    shutil.copytree(Path(t["mspm0_sdk"]["path"]) / b["example"], proj)
    for junk in ("iar", "keil", "gcc"):
        shutil.rmtree(proj / junk, ignore_errors=True)
    notice = (proj / "gpio_toggle_output.c").read_text().split("*/", 1)[0] + "*/\n"
    (proj / "gpio_toggle_output.c").write_text(f"#define M {model}\n" + SRC.read_text())
    (proj / "board_leds.h").write_text(build_board.header(b))
    shutil.copy2(build_board.PLAYER / "frl_params.h", proj / "frl_params.h")
    (proj / "frl_params_gen.h").write_text(params.gen_header("occ"))
    md = MODELS_DIR / model
    for f in (f"{model}.h", "test_vectors.h"):
        shutil.copy2(md / f, proj / f)
    shutil.copy2(md / "model_O3.a", proj / "model_O3.a")
    mk = proj / "ticlang" / "makefile"
    text = mk.read_text().replace("../ticlang/device_linker.cmd \\", f"../ticlang/device_linker.cmd -Wl,--stack_size={STACK} ../model_O3.a \\", 1)
    mk.write_text(text)
    args = [t["make"]["exe"], "-C", str(proj / "ticlang"), f"MSPM0_SDK_INSTALL_DIR={t['mspm0_sdk']['path']}",
            f"SYSCONFIG_TOOL={t['sysconfig']['path']}", f"TICLANG_ARMCOMPILER={t['ticlang']['path']}", "GCC_ARMCOMPILER="]
    p = subprocess.run(args, capture_output=True, text=True, timeout=900)
    img = proj / "ticlang" / "gpio_toggle_output.out"
    if p.returncode or not img.is_file():
        print((p.stdout + p.stderr)[-2500:])
        raise SystemExit(f"build failed ({board} {model})")
    m = (proj / "ticlang" / "gpio_toggle_output.map").read_text()
    memory = build_board.map_memory(m)
    print(f"   {board} {model}:", " ".join(l.strip() for l in m.splitlines() if re.match(r"\s+(FLASH|SRAM)\s", l)))
    nm = Path(t["ticlang"]["path"]) / "bin" / "tiarmnm"
    sym = subprocess.run([str(nm), str(img)], capture_output=True, text=True).stdout
    addr = next(int(l.split()[0], 16) for l in sym.splitlines() if l.split()[-1:] == ["oc"])
    subprocess.run([str(Path(t["ticlang"]["path"]) / "bin" / "tiarmstrip"), "--strip-all", str(img)], check=True)
    return img, t, notice, tmp, addr, memory


def main():
    args = sys.argv[1:]
    models = None
    if args[:1] == ["--model"]:
        models, args = args[1].split(","), args[2:]
    ref = None
    for board in args or list(FITS):
        for model in models or FITS[board]:
            if model not in FITS[board]:
                print(f"skip {board} {model}: does not fit")
                continue
            img, t, notice, tmp, addr, memory = build(board, model)
            try:
                w, label, exp = windows(model)
                if ref is None:
                    ref = (w, label)
                assert (w, label) == ref, f"{model}'s test windows differ from the first model's"
                oc = {"addr": f"0x{addr:08X}", "magic": "0x0CC00002", "model": model, "n_test": 20, "labels": label,
                      "layout": ["magic", "cpuclk_hz", "n_test", "same_ref", "right", "cyc_min", "cyc_max", "seq_in", "seq_out", "idx", "pred", "cycles", "logits[4]"]}
                build_board.write(board, "occ-" + model.replace("occ_cnn_", ""), img, t, notice, tmp, "occupancy",
                                  {"occ": oc, "memory": memory, "params": params.manifest_block("occ", BOARDS[board]["table"]),
                                   "expect": "self-test: the chip runs its 20 test windows and matches the reference answers; then one answer per window the page asks for; LED2 red flashes once per answer"},
                                  SRC, f"the example's main source replaced by the room-occupancy program with {model}; stack {STACK} bytes",
                                  f"python3 tools/webflash/build_occ.py --model {model} {board}")
            finally:
                shutil.rmtree(tmp, ignore_errors=True)
    bench = build_board.ROOT / "site" / "flash" / "bench"
    if ref and bench.is_dir():                        # the bench page's data; a reader clone has no bench page
        (bench / "occ-windows.json").write_text(json.dumps(
            {"note": "Written by tools/webflash/build_occ.py from the models' test_vectors.h. 20 held-out windows of the UCI Room Occupancy Estimation data (CC BY 4.0, Singh and Chaudhari 2018); x is channel-major 4 x 32.",
             "channels": ["light (lux)", "sound (V)", "CO2 (ppm)", "motion (0 or 1)"], "label": ref[1], "x": [[round(v, 4) for v in r] for r in ref[0]]}, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    main()
