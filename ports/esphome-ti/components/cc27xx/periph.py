"""What the YAML asks of TI's drivers, turned into SysConfig text and a C header for the shim.

TI's drivers (UART2, I2C, ADC, PWM, NVS) are configured at build time: each instance, and the pins it uses, is a
SysConfig entry. ESPHome decides which buses and pins exist from the YAML, so run_compile calls collect() on the
validated config, and ti_build appends syscfg() to TI's Zigbee SysConfig file and writes header() as frl_periph.h
for shim code. The pin tables come from pindata.json (gen_pins.py makes it from TI's board and device data).
"""

from __future__ import annotations

import json
import re
from pathlib import Path

HERE = Path(__file__).parent
PINDATA = json.loads((HERE / "pindata.json").read_text())

# NVS region for ESPHome's saved settings, taken from the top of the application flash (the linker file's FLASH
# region is shortened by the same amount). Sector size on these chips is 2 kB.
NVS_BYTES = 0x2000


def board_pins(board: str) -> dict[int, dict]:
    return {int(k): v for k, v in PINDATA[board]["pins"].items()}


def _signals(board: str, dio: int) -> list[str]:
    return board_pins(board)[dio]["signals"]


def _adc_name(board: str, dio: int) -> str | None:
    for s in _signals(board, dio):
        if re.fullmatch(r"ADC\d+", s):
            return s
    return None


def _lgpt_channels(board: str, dio: int) -> list[tuple[int, int]]:
    """(timer, channel) pairs that can drive this DIO as a plain PWM output (not the inverted N output)."""
    out = []
    for s in _signals(board, dio):
        m = re.fullmatch(r"LGPT(\d)C(\d)", s)
        if m:
            out.append((int(m.group(1)), int(m.group(2))))
    return out


def collect(config: dict, board: str) -> dict:
    """Read the validated config for everything that needs a TI driver instance."""
    per = {"uart_log": None, "i2c": [], "adc": [], "pwm": [], "nvs": True}
    if "logger" in config:
        per["uart_log"] = config["logger"].get("baud_rate", 115200)
    for bus in config.get("i2c", []):
        per["i2c"].append(dict(sda=int(bus["sda"]), scl=int(bus["scl"]), frequency=int(bus.get("frequency", 100000))))
    for s in config.get("sensor", []):
        if s.get("platform") == "ti_adc":
            per["adc"].append(int(s["pin"]))
    used_timers: dict[int, int] = {}
    for o in config.get("output", []):
        if o.get("platform") == "ti_pwm":
            dio = int(o["pin"])
            choices = _lgpt_channels(board, dio)
            if not choices:
                raise ValueError(f"DIO{dio} has no PWM timer output on {board}")
            # one timer instance drives one pin in this platform: pick a timer no other PWM pin took
            pick = next((c for c in choices if c[0] not in used_timers), None)
            if pick is None:
                raise ValueError(f"No free PWM timer left for DIO{dio} (each PWM output takes one of the chip's LGPT timers)")
            used_timers[pick[0]] = dio
            per["pwm"].append(dict(dio=dio, timer=pick[0], channel=pick[1]))
    return per


def _pin(board: str, dio: int) -> str:
    """SysConfig's name for a DIO: the CC2755 lists analog-capable pins with their suffix (DIO21_A4); the CC2340 takes DIOn."""
    return board_pins(board)[dio]["name"] if board == "LP_EM_CC2755P10" else f"DIO{dio}"


def syscfg(per: dict, board: str) -> str:
    t = ["", "/* ======== ESPHome peripherals (written by components/cc27xx/periph.py) ======== */"]
    if per["uart_log"]:
        t += [
            'const FRL_UART2 = scripting.addModule("/ti/drivers/UART2");',
            "const frlUart = FRL_UART2.addInstance();",
            'frlUart.$name = "CONFIG_FRL_UART2_LOG";',
            "frlUart.$hardware = system.deviceData.board.components.XDS110UART;",
            "frlUart.rxRingBufferSize = 16;  // the driver's minimum; the logger only writes",
            "frlUart.enableNonblocking = false;",
        ]
    if per["i2c"]:
        t.append('const FRL_I2C = scripting.addModule("/ti/drivers/I2C");')
        for i, b in enumerate(per["i2c"]):
            t += [
                f"const frlI2c{i} = FRL_I2C.addInstance();",
                f'frlI2c{i}.$name = "CONFIG_FRL_I2C_{i}";',
                f'frlI2c{i}.i2c.sdaPin.$assign = "{_pin(board, b["sda"])}";',
                f'frlI2c{i}.i2c.sclPin.$assign = "{_pin(board, b["scl"])}";',
            ]
    if per["adc"]:
        t.append('const FRL_ADC = scripting.addModule("/ti/drivers/ADC");')
        for i, d in enumerate(per["adc"]):
            t += [
                f"const frlAdc{i} = FRL_ADC.addInstance();",
                f'frlAdc{i}.$name = "CONFIG_FRL_ADC_{i}";',
                f'frlAdc{i}.adcChannel.adc.adcPin.$assign = "{_pin(board, d)}";',
            ]
    if per["pwm"]:
        t.append('const FRL_PWM = scripting.addModule("/ti/drivers/PWM");')
        for i, p in enumerate(per["pwm"]):
            t += [
                f"const frlPwm{i} = FRL_PWM.addInstance();",
                f'frlPwm{i}.$name = "CONFIG_FRL_PWM_{i}";',
                f'frlPwm{i}.timerObject.channel{p["channel"]}Connection = "Pin";',
                f'frlPwm{i}.timerObject.timer.$assign = "LGPT{p["timer"]}";',
                f'frlPwm{i}.timerObject.timer.ch{p["channel"]}Pin.$assign = "{_pin(board, p["dio"])}";',
                f'scripting.suppress("@@@.+?@@@", frlPwm{i}.timerObject.timer, "ch{p["channel"]}Pin");',
            ]
    return "\n".join(t) + "\n"


def header(per: dict) -> str:
    h = ["/* generated by components/cc27xx/periph.py: which TI driver instance serves which pin */", "#pragma once", ""]
    if per["uart_log"]:
        h += ["#define FRL_HAVE_LOG_UART 1", f"#define FRL_LOG_BAUD {per['uart_log']}"]
    h.append(f"#define FRL_I2C_COUNT {len(per['i2c'])}")
    h.append(f"#define FRL_ADC_COUNT {len(per['adc'])}")
    h.append(f"#define FRL_PWM_COUNT {len(per['pwm'])}")
    h.append("#define FRL_I2C_BUSES { " + ", ".join(f"{{ CONFIG_FRL_I2C_{i}, {b['frequency']} }}" for i, b in enumerate(per["i2c"])) + " }" if per["i2c"] else "#define FRL_I2C_BUSES { }")
    h.append("#define FRL_ADC_PINS { " + ", ".join(f"{{ {d}, CONFIG_FRL_ADC_{i} }}" for i, d in enumerate(per["adc"])) + " }" if per["adc"] else "#define FRL_ADC_PINS { }")
    h.append("#define FRL_PWM_PINS { " + ", ".join(f"{{ {p['dio']}, CONFIG_FRL_PWM_{i} }}" for i, p in enumerate(per["pwm"])) + " }" if per["pwm"] else "#define FRL_PWM_PINS { }")
    return "\n".join(h) + "\n"
