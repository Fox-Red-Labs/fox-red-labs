#!/usr/bin/env python3
"""Build the LP-MSPM0L1306 no-code controls program (pin, timer, bus) and write it as a prebuilt image with a manifest.

  python3 tools/webflash/build_controls.py        writes prebuilt/mspm0l1306/controls/

Same route as build_player.py, from TI's i2c_controller_rw_multibyte_fifo_poll example (its SysConfig sets up I2C0 on
PA0 and PA1 at 400 kHz and LED2 red on PA26): the example's main source is replaced by tools/webflash/player/controls.c
and built with the validated tools. The address of the status block (struct ctl) is read from the symbol table before
the image is stripped and goes in the manifest, with the table the page writes.
"""
import datetime as dt
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolfind  # noqa: E402
import build_board  # noqa: E402

BOARD = "mspm0l1306"
EXAMPLE = "examples/nortos/LP_MSPM0L1306/driverlib/i2c_controller_rw_multibyte_fifo_poll"
MAIN = "i2c_controller_rw_multibyte_fifo_poll.c"
SRC = build_board.PLAYER / "controls.c"
OUT = ROOT / "prebuilt" / BOARD / "controls"
TABLE = {"addr": "0x0000FC00", "magic": "0x4C544346",
         "words": ["magic", "seq", "pin", "mode", "on_ms", "off_ms", "i2c_addr", "i2c_reg", "i2c_n", "pwm_hz", "pwm_duty", "flags",
                   "r1_addr", "r1_reg", "r1_n", "r2_addr", "r2_reg", "r2_n", "r3_addr", "r3_reg", "r3_n"],
         "modes": {"0": "read", "1": "low", "2": "high", "3": "toggle", "4": "pwm"}, "no_pin": 255, "i2c_scan": 256, "no_reg": 256,
         "min_ms": 5, "max_ms": 10000, "max_n": 8, "max_n_extra": 4, "pwm_min_hz": 100, "pwm_max_hz": 20000,
         "pwm_pins": [26, 27, 13, 12, 3, 6], "flags": {"scan_too": 1}}
STATUS_WORDS = ["magic", "seq", "level", "toggles", "i2c", "bytes0", "bytes1", "scan0", "scan1", "scan2", "scan3", "cpuclk",
                "pwm_period", "pwm_cc", "pwm_div", "r1_result", "r1_bytes", "r2_result", "r2_bytes", "r3_result", "r3_bytes"]
# pins a reader may pick, by chip name, with what the LaunchPad wires to them (TI's SLAU869E; the repo's board notes).
# Header positions are given only where this repository has recorded them from a run (mspm0l1306/bench/uart_loop).
PINS = [   # header positions from TI's SLAU869E Figure 2-9 (J1 pins 1 to 10, J2 11 to 20, J3 21 to 30, J4 31 to 40); roles from the same figure and the board notes
    {"pin": 26, "name": "PA26", "role": "LED2 red", "header": "J4 pin 38", "note": "the RGB LED's red channel (jumper J2 as shipped); TI's pinout marks it PWM"},
    {"pin": 13, "name": "PA13", "role": "LED2 green", "header": "J4 pin 39", "note": "jumper J3 as shipped; TI's pinout marks it PWM"},
    {"pin": 27, "name": "PA27", "role": "LED2 blue", "header": "J4 pin 37", "note": "jumper J12 as shipped; TI's pinout marks it PWM"},
    {"pin": 0, "name": "PA0", "role": "LED1 red, I2C0 SDA", "header": "J1 pin 10", "note": "J9 pulls it up to 3.3 V; the bus controls use it as SDA"},
    {"pin": 1, "name": "PA1", "role": "I2C0 SCL", "header": "J1 pin 9", "note": "J10 pulls it up to 3.3 V; the bus controls use it as SCL"},
    {"pin": 8, "name": "PA8", "role": "UART0 TX", "header": "J1 pin 4", "note": "to the USB serial port with J16 and J17 on XDS (as shipped)"},
    {"pin": 9, "name": "PA9", "role": "UART0 RX", "header": "J1 pin 3", "note": "to the USB serial port with J16 and J17 on XDS (as shipped)"},
    {"pin": 22, "name": "PA22", "role": "light sensor amplifier output", "header": "J1 pin 8", "note": "jumper J6 as shipped: the photodiode circuit"},
    {"pin": 15, "name": "PA15", "role": "thermistor (ADC0.9)", "header": "J3 pin 23", "note": "J1 1-2 as shipped: the thermistor divider, about 1.6 V at room temperature"},
    {"pin": 6, "name": "PA6", "role": "free pin (SPI_CLK on TI's pinout)", "header": "J1 pin 7", "note": "TI's examples use it as a test output"},
    {"pin": 24, "name": "PA24", "role": "free pin", "header": "J1 pin 5", "note": "jumper J5 as shipped joins it to the light sensor circuit"},
    {"pin": 12, "name": "PA12", "role": "free pin (PWM on TI's pinout)", "header": "J4 pin 40", "note": ""},
    {"pin": 10, "name": "PA10", "role": "free pin (timer capture on TI's pinout)", "header": "J4 pin 36", "note": ""},
    {"pin": 3, "name": "PA3", "role": "free pin (PWM on TI's pinout)", "header": "J2 pin 19", "note": ""},
    {"pin": 7, "name": "PA7", "role": "free pin", "header": "J2 pin 18", "note": "the Sensors BoosterPack's pin 28 (TMP117 power) goes here by wire, as our arrival plan for that board has it"},
    {"pin": 23, "name": "PA23", "role": "free pin", "header": "J2 pin 12", "note": "the Sensors BoosterPack's pin 27 (OPT3001 power) goes here by wire, as our arrival plan for that board has it"},
]
for _p in PINS:   # pins whose timer output the program can drive (device header IOMUX_PINCMx_PF_TIMGy_CCPz)
    _p["pwm"] = _p["pin"] in TABLE["pwm_pins"]


def main():
    t = {k: toolfind.find(k, v) for k, v in build_board.VALID.items()}
    tmp = Path(tempfile.mkdtemp(prefix="frl-controls-"))
    proj = tmp / "p"
    shutil.copytree(Path(t["mspm0_sdk"]["path"]) / EXAMPLE, proj)
    for junk in ("iar", "keil", "gcc"):
        shutil.rmtree(proj / junk, ignore_errors=True)
    notice = (proj / MAIN).read_text().split("*/", 1)[0] + "*/\n"
    (proj / MAIN).write_text(f"#define CTL_TABLE_ADDR ({TABLE['addr']}u)\n" + SRC.read_text())
    args = [t["make"]["exe"], "-C", str(proj / "ticlang"), f"MSPM0_SDK_INSTALL_DIR={t['mspm0_sdk']['path']}",
            f"SYSCONFIG_TOOL={t['sysconfig']['path']}", f"TICLANG_ARMCOMPILER={t['ticlang']['path']}", "GCC_ARMCOMPILER="]
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    img = proj / "ticlang" / (MAIN[:-2] + ".out")
    if p.returncode or not img.is_file():
        print((p.stdout + p.stderr)[-2500:])
        return 1
    nm = Path(t["ticlang"]["path"]) / "bin" / "tiarmnm"
    sym = subprocess.run([str(nm), str(img)], capture_output=True, text=True).stdout
    addr = next(int(l.split()[0], 16) for l in sym.splitlines() if l.split()[-1:] == ["ctl"])
    subprocess.run([str(Path(t["ticlang"]["path"]) / "bin" / "tiarmstrip"), "--strip-all", str(img)], check=True)
    OUT.mkdir(parents=True, exist_ok=True)
    shutil.copy2(img, OUT / "firmware.out")
    segs = build_board.segments(OUT / "firmware.out")
    end = max(int(s["addr"], 16) + s["bytes"] for s in segs)
    if end > int(TABLE["addr"], 16):
        print(f"the image reaches 0x{end:X}, into the table sector at {TABLE['addr']}")
        return 1
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    sha = lambda q: hashlib.sha256(Path(q).read_bytes()).hexdigest()  # noqa: E731
    man = {
        "format": "frl-prebuilt-1", "board": BOARD, "kit": "LP-MSPM0L1306",
        "revisions_tested": ["unknown: the board revision was not recorded for this build"],
        "chip": {"deviceid": "0x0BB8202F", "mask": "0x0FFFFFFF"},
        "recipe": "controls", "table": TABLE,
        "status": {"addr": f"0x{addr:08X}", "magic": "0x4C545354", "words": STATUS_WORDS},
        "pins": PINS, "gpio": "0x400A0000",
        "i2c": {"instance": "I2C0", "sda": "PA0", "scl": "PA1", "khz": 400, "pullups": "J9 and J10 (2.2 kΩ to 3.3 V, as shipped)"},
        "image": {"file": "firmware.out", "sha256": sha(OUT / "firmware.out"), "bytes": (OUT / "firmware.out").stat().st_size, "segments": segs},
        "source": {"repo_commit": commit, "example": EXAMPLE, "file": str(SRC.relative_to(ROOT)), "sha256": sha(SRC),
                   "edited_line": "the example's main source replaced by the controls program", "rebuild": "python3 tools/webflash/build_controls.py"},
        "tools": {k: {"version": t[k].get("version"), "path": t[k].get("path")} for k in ("mspm0_sdk", "sysconfig", "ticlang")},
        "expect": "The program idles until the page writes a table at 0xFC00; then it drives, reads or toggles the pin asked and answers the bus request, reporting in struct ctl.",
        "made": dt.datetime.now().isoformat(timespec="seconds"),
        "release_cleared": False,
        "release_note": "Built on TI's example (BSD-3-Clause, NOTICE.txt) with TI DriverLib, linked with TI Arm Clang's runtime. Not cleared for public distribution.",
    }
    (OUT / "manifest.json").write_text(json.dumps(man, indent=1, ensure_ascii=False) + "\n")
    (OUT / "NOTICE.txt").write_text("This firmware is the Fox Red Labs no-code controls program, built on Texas Instruments' MSPM0 SDK example "
                                    "i2c_controller_rw_multibyte_fifo_poll (its SysConfig setup and DriverLib). TI's notice from that example:\n\n" + notice)
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"OK: controls in {OUT.relative_to(ROOT)}: {segs[0]['bytes']} bytes at {segs[0]['addr']}, status ctl at 0x{addr:08X}, sha256 {man['image']['sha256'][:16]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
