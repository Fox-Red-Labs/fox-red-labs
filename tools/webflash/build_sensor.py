#!/usr/bin/env python3
"""Build the browser bench's thermometer for MSPM0 LaunchPad kits.

  python3 tools/webflash/build_sensor.py [board ...]        default: mspm0l1306 mspm0g3519 mspm0g5187

Same route as build_board.py (TI's gpio_toggle_output example for the board, its main source replaced, built with
the validated tools), with tools/webflash/player/thermometer.c and the board's temperature-sensor channel. Writes
prebuilt/<board>/thermometer/ with a manifest that carries what the page reads: ADC0's MEMRES0 register and the
factory trim. Channels from SysConfig's ADC12_internalConnections.js, addresses from the device headers
(SDK 2.11.00.07); the conversion to degrees C is TI's, from adc12_internal_temp_sensor_mathacl.
"""
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_board  # noqa: E402
import params  # noqa: E402
from boards_mspm0 import BOARDS, GPIOA  # noqa: E402

ROOT = build_board.ROOT
SRC = build_board.PLAYER / "thermometer.c"

# The L1306 is built by build_player.py for its other recipes; its facts for this build only.
BOARDS.setdefault("mspm0l1306", {
    "kit": "LP-MSPM0L1306", "serial": "ML130001", "example": "examples/nortos/LP_MSPM0L1306/driverlib/gpio_toggle_output",
    "deviceid": 0x0BB8202F, "flash_end": 0x10000, "table": 0xFC00,
    "channels": {"red": ("GPIOA", 26, GPIOA, "LED2 red (PA26)"), "green": None, "blue": None}, "active_high": True,
})

# ADC0 base (mspm0l130x.h, mspm0g351x.h, mspm0g518x.h); MEMRES0 is at +0x1280 (hw_adc12.h, ULLMEM).
# Temperature-sensor channel from ADC12_internalConnections.js (MSPM0L11XX_L13XX, MSPM0GX51X, MSPM0G518X).
SENSE = {
    "mspm0l1306": {"adc0": 0x40004000, "chan": 11, "mv_per_c": -1.75},
    "mspm0g3519": {"adc0": 0x40000000, "chan": 11, "mv_per_c": -1.8},
    "mspm0g5187": {"adc0": 0x40004000, "chan": 29, "mv_per_c": -1.8},
}
# Slope: -1.75 mV/C in TI's L-series examples (-571.42 C/V), -1.8 mV/C in its G-series ones (-555.55 C/V). The factory
# trim (TEMP_SENSE0, at 30 C) is an ADC code whose reference TI's documents disagree on: 1.4 V in the TRMs, VDDA in the
# examples; read on our boards it fits 1.4 V on the G3519 and neither on the L1306, whose early samples TI says carry a
# wrong trim (E2E 1311550). The page uses the reference that puts the trim voltage in 0.55 to 0.75 V, else the typical
# 634.5 mV at 30 C from the TRM's worked example, and says which it used.
TEMP_SENSE0 = 0x41C4003C      # hw_factoryregion.h, the same on all three


def main():
    for board in sys.argv[1:] or list(SENSE):
        s = SENSE[board]
        src = Path(build_board.tempfile.mkdtemp(prefix="frl-th-")) / "thermometer.c"
        src.write_text(f"#define TH_CHAN DL_ADC12_INPUT_CHAN_{s['chan']}\n" + SRC.read_text())
        img, t, notice, tmp = build_board.build(board, src, params_kind="thermometer")
        try:
            th = {"memres": f"0x{s['adc0'] + 0x1280:08X}", "trim": f"0x{TEMP_SENSE0:08X}", "channel": s["chan"],
                  "vref": 3.3, "bits": 12, "mv_per_c": s["mv_per_c"], "trim_c": 30, "trim_refs": [1.4, 3.3],
                  "trim_window_v": [0.55, 0.75], "typical_vtrim": 0.6345,
                  "formula": "T = (Vsample - Vtrim) / slope + 30 C, V = (code - 0.5) * ref / 4096 (TI's MSPM0 examples and TRM)"}
            build_board.write(board, "thermometer", img, t, notice, tmp, "thermometer",
                              {"thermometer": th, "params": params.manifest_block("thermometer", BOARDS[board]["table"]), "expect": "LED2 red flashes once a second; ADC0 MEMRES0 holds the chip's temperature reading, updated ten times a second"},
                              SRC, f"the example's main source replaced by the thermometer; TH_CHAN = channel {s['chan']}",
                              f"python3 tools/webflash/build_sensor.py {board}")
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
            shutil.rmtree(src.parent, ignore_errors=True)


if __name__ == "__main__":
    main()
