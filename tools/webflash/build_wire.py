#!/usr/bin/env python3
"""Build the browser bench's first-wire loop (tools/webflash/player/wire.c) for the LP-MSPM0G3519.

  python3 tools/webflash/build_wire.py

Same route as build_digit.py without BitNetMCU: TI's gpio_toggle_output example, its main source replaced, the
example's makefile and the validated tools. The mailbox address (struct lp) is read from the symbol table before
the image is stripped. The wire: J3 pin 30 (PA15, DAC) to J1 pin 6 (PA25, ADC0.2).
"""
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_board  # noqa: E402
from boards_mspm0 import BOARDS  # noqa: E402

SRC = build_board.PLAYER / "wire.c"
WIRE = {"mspm0g3519": {"from": "J3 pin 30 (PA15, DAC)", "to": "J1 pin 6 (PA25, ADC0.2)"}}


def main():
    for board in sys.argv[1:] or list(WIRE):
        b = BOARDS[board]
        t = {k: build_board.toolfind.find(k, v) for k, v in build_board.VALID.items()}
        tmp = Path(build_board.tempfile.mkdtemp(prefix=f"frl-wire-{board}-"))
        try:
            proj = tmp / "p"
            shutil.copytree(Path(t["mspm0_sdk"]["path"]) / b["example"], proj)
            for junk in ("iar", "keil", "gcc"):
                shutil.rmtree(proj / junk, ignore_errors=True)
            notice = (proj / "gpio_toggle_output.c").read_text().split("*/", 1)[0] + "*/\n"
            shutil.copy2(SRC, proj / "gpio_toggle_output.c")
            (proj / "board_leds.h").write_text(build_board.header(b))
            p = subprocess.run([t["make"]["exe"], "-C", str(proj / "ticlang"), f"MSPM0_SDK_INSTALL_DIR={t['mspm0_sdk']['path']}",
                                f"SYSCONFIG_TOOL={t['sysconfig']['path']}", f"TICLANG_ARMCOMPILER={t['ticlang']['path']}", "GCC_ARMCOMPILER="],
                               capture_output=True, text=True, timeout=900)
            img = proj / "ticlang" / "gpio_toggle_output.out"
            if p.returncode or not img.is_file():
                print((p.stdout + p.stderr)[-2500:])
                raise SystemExit(f"build failed ({board})")
            sym = subprocess.run([str(Path(t["ticlang"]["path"]) / "bin" / "tiarmnm"), str(img)], capture_output=True, text=True).stdout
            addr = next(int(l.split()[0], 16) for l in sym.splitlines() if l.split()[-1:] == ["lp"])
            subprocess.run([str(Path(t["ticlang"]["path"]) / "bin" / "tiarmstrip"), "--strip-all", str(img)], check=True)
            lp = {"addr": f"0x{addr:08X}", "magic": "0xD1A10001", **WIRE[board]}
            build_board.write(board, "first-wire", img, t, notice, tmp, "first-wire",
                              {"wire": lp, "expect": "with the wire in, the ADC reads back about the DAC code it was sent"},
                              SRC, "the example's main source replaced by the first-wire loop", f"python3 tools/webflash/build_wire.py {board}")
            print(f"   mailbox lp at 0x{addr:08X}")
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
