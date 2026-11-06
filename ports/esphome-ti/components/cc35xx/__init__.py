"""ESPHome platform `cc35xx`: TI CC35xx Wi-Fi 6 and Bluetooth LE chips (LP-EM-CC35X1, CC3551E). See ../../README.md.

Same shape as the cc27xx platform: ESPHome's generated C++ is built with TI's compiler against TI's Wi-Fi SDK by
ti35_build.py, and the parts of ESPHome that are platform-specific (HAL, GPIO, preferences, the Wi-Fi backend) come
from shim/. ESPHome's own files are not edited.
"""

from __future__ import annotations

import logging
import re

from esphome import pins
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_set_core_data, zephyr_to_code, zephyr_setup_preferences
from esphome.components.zephyr.const import KEY_BOOTLOADER, zephyr_ns
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, EsphomeError
from esphome.coroutine import CoroPriority, coroutine_with_priority

_LOGGER = logging.getLogger(__name__)

PLATFORM = "cc35xx"
BOARDS = {"LP_EM_CC35X1": "CC3551E"}
CONF_SDK_PATH = "sdk_path"
CONF_COMPILER_PATH = "compiler_path"
CONF_SYSCONFIG_PATH = "sysconfig_path"
CONF_TOOLBOX_PATH = "toolbox_path"
CONF_PROBE_SERIAL = "probe_serial"
CONF_API_SELF_TEST = "api_self_test"

ZephyrGPIOPin = zephyr_ns.class_("ZephyrGPIOPin", cg.InternalGPIOPin)


def _logger_uart_selection_for_cc35xx():
    """ESPHome's logger asks the platform which UARTs it offers and has no answer for an out-of-tree platform (it raises
    NotImplementedError). On cc35xx the log goes to TI's console UART (the XDS110 application port, 115200 8N1), so
    there is nothing to select: answer "invalid" for every choice, which the logger already handles."""
    from esphome.components import logger as _logger

    original = _logger.uart_selection

    def uart_selection(value):
        if CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) == PLATFORM:
            raise cv.Invalid("cc35xx logs to TI's console UART; hardware_uart is not used")
        return original(value)

    _logger.uart_selection = uart_selection


_logger_uart_selection_for_cc35xx()


def _pin_number(value):
    if isinstance(value, dict) or value is None:
        raise cv.Invalid("This variable only supports pin numbers, not full pin schemas (with inverted and mode).")
    if isinstance(value, int):
        return value
    s = str(value).strip().upper()
    for p in ("GPIO", "DIO"):
        if s.startswith(p):
            s = s[len(p):]
    try:
        return int(s)
    except ValueError:
        raise cv.Invalid(f"Invalid pin: {value} (write the GPIO number, for example GPIO23)") from None


def _validate_pin(value):
    n = _pin_number(value)
    if n < 0 or n > 63:
        raise cv.Invalid(f"Invalid GPIO number: {n}")
    return n


def _set_core_data(config):
    zephyr_set_core_data({CONF_BOARD: config[CONF_BOARD], KEY_BOOTLOADER: ""})
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "zephyr"
    return config


def _board(value):
    key = cv.string_strict(value).upper().replace("-", "_")
    if key not in BOARDS:
        raise cv.Invalid(f"{PLATFORM} supports {', '.join(BOARDS)}; got {value}")
    return key


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_BOARD, default="LP_EM_CC35X1"): _board,
            cv.Optional(CONF_SDK_PATH): cv.string,
            cv.Optional(CONF_COMPILER_PATH): cv.string,
            cv.Optional(CONF_SYSCONFIG_PATH): cv.string,
            cv.Optional(CONF_TOOLBOX_PATH): cv.string,
            cv.Optional(CONF_PROBE_SERIAL): cv.string,
            cv.Optional(CONF_API_SELF_TEST, default=False): cv.boolean,
        }
    ),
    _set_core_data,
)

IS_TARGET_PLATFORM = True


def _final_validate(config):
    import esphome.final_validate as fv

    api = fv.full_config.get().get("api") or {}
    if "encryption" in api:
        raise cv.Invalid(f"{PLATFORM} has no hardware random number source wired in yet, so api: encryption is refused")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate

pin_schema = pins.gpio_base_schema(ZephyrGPIOPin, _validate_pin, modes=pins.GPIO_STANDARD_MODES)


@pins.PIN_SCHEMA_REGISTRY.register(PLATFORM, pin_schema)
async def pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression("DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0))"), 64, "GPIO")
    cg.add(var.set_pin(config[CONF_NUMBER]))
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config):
    cg.add_build_flag(f"-DUSE_{PLATFORM.upper()}")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", PLATFORM.upper())
    cg.add_define("ESPHOME_MANUFACTURER", "Texas Instruments")
    if config[CONF_API_SELF_TEST]:
        cg.add_build_flag("-DFRL_API_SELF_TEST")
    from esphome.const import ThreadModel

    cg.add_define(ThreadModel.SINGLE)
    cg.add_global(cg.RawStatement('#include "esphome/components/zephyr/gpio.h"'))
    cg.add_global(cg.RawStatement('#include "esphome/components/zephyr/preferences.h"'))
    zephyr_setup_preferences()
    zephyr_to_code(config)


def copy_files():
    return None


def _drop_config_cache():
    try:
        from esphome.compiled_config import compiled_config_path

        compiled_config_path(CORE.config_path.name).unlink(missing_ok=True)
    except Exception:
        _LOGGER.debug("could not remove the validated-config cache", exc_info=True)


def run_compile(args, config):
    from . import ti35_build

    _drop_config_cache()
    conf = config[PLATFORM]
    try:
        # ESPHome hands some options (log level, feature switches) to the compiler as build flags, which other platforms
        # pass through PlatformIO; here they go straight to TI Arm Clang. Only defines and undefines are taken.
        flags = sorted(f for f in CORE.build_flags if f.startswith(("-D", "-U")))
        res = ti35_build.build(build_dir=CORE.build_path, src_dir=CORE.relative_src_path(),
                               out_dir=CORE.relative_pioenvs_path(CORE.name), name=CORE.name, conf=conf, esphome_flags=flags)
    except ti35_build.BuildError as err:
        raise EsphomeError(str(err)) from err
    s = res["seconds"]
    _LOGGER.info("Built %s: compiled %d of %d files, total %.1f s", res["bundle"], res["compiled"], res["files"], s["total"])
    for region, m in res["memory"].items():
        _LOGGER.info("  %-22s used %7d B  free %7d B  of %7d B", region, m["used"], m["unused"], m["length"])
    return True


def upload_program(config, args, host):
    from . import ti35_build

    _drop_config_cache()
    conf = config[PLATFORM]
    m = re.search(r"usbmodem([A-Za-z0-9]+?)\d$", host or "")
    serial = conf.get(CONF_PROBE_SERIAL) or (m.group(1) if m else (host if host and "/" not in host else None))
    if not serial:
        raise EsphomeError("Say which debug probe to use: probe_serial: in the cc35xx: block, or --device <serial>.")
    rc = ti35_build.flash(out_dir=CORE.relative_pioenvs_path(CORE.name), probe_serial=serial, conf=conf)
    if rc != 0:
        raise EsphomeError(f"Toolbox programming failed with exit code {rc}")
    return True
