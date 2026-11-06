"""ESPHome target platform for TI SimpleLink Low Power F3 chips.

One factory builds both `cc27xx` (CC2755, Cortex-M33) and `cc23xx` (CC2340, Cortex-M0+). It follows ESPHome's own
nrf52 platform: a CONFIG_SCHEMA that records the target platform and framework, a pin schema, a to_code, and the
two platform hooks `run_compile` and `upload_program`. Where nrf52 hands the build to west and Zephyr, this builds with
TI's compiler, FreeRTOS and ZBOSS through ti_build.py.
"""

from __future__ import annotations

import logging
import re
from pathlib import Path

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

from . import ti_build

_LOGGER = logging.getLogger(__name__)

CONF_SDK_PATH = "sdk_path"
CONF_COMPILER_PATH = "compiler_path"
CONF_SYSCONFIG_PATH = "sysconfig_path"
CONF_DSLITE_PATH = "dslite_path"
CONF_PROBE_SERIAL = "probe_serial"
CONF_HEAP_SIZE = "heap_size"

ZephyrGPIOPin = zephyr_ns.class_("ZephyrGPIOPin", cg.InternalGPIOPin)


def _pin_number(value):
    """DIO16, 16, "16" or the nRF-style "P0.16" (a YAML written for nrf52 keeps working)."""
    if isinstance(value, dict) or value is None:
        raise cv.Invalid("This variable only supports pin numbers, not full pin schemas (with inverted and mode).")
    if isinstance(value, int):
        return value
    s = str(value).strip().upper()
    if s.startswith("DIO"):
        s = s[3:]
    elif s.startswith("P0."):
        s = s[3:]
    try:
        return int(s)
    except ValueError:
        raise cv.Invalid(f"Invalid pin: {value} (write the DIO number, for example DIO16)") from None


def _validate_pin(value):
    n = _pin_number(value)
    if n < 0 or n > 31:
        raise cv.Invalid(f"Invalid DIO number: {n}")
    return n


def make_platform(platform: str, boards: dict[str, str], default_board: str) -> dict:
    """Return the module-level names ESPHome looks for in esphome.components.<platform>."""

    def _set_core_data(config):
        zephyr_set_core_data({CONF_BOARD: config[CONF_BOARD], KEY_BOOTLOADER: ""})
        CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = platform
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "zephyr"
        # ESPHome's Zigbee component turns its Zephyr options on (and offers the switch) when "nrf52" is among the
        # loaded integrations. Our platform stands in for it, so say so; nothing from nrf52 is built.
        CORE.loaded_integrations.add("nrf52")
        return config

    def _board(value):
        value = cv.string_strict(value)
        key = value.upper().replace("-", "_")
        if key not in boards:
            raise cv.Invalid(f"{platform} supports {', '.join(boards)}; got {value}")
        return key

    config_schema = cv.All(
        cv.Schema(
            {
                cv.Optional(CONF_BOARD, default=default_board): _board,
                cv.Optional(CONF_SDK_PATH): cv.string,
                cv.Optional(CONF_COMPILER_PATH): cv.string,
                cv.Optional(CONF_SYSCONFIG_PATH): cv.string,
                cv.Optional(CONF_DSLITE_PATH): cv.string,
                cv.Optional(CONF_PROBE_SERIAL): cv.string,
                cv.Optional(CONF_HEAP_SIZE): cv.int_range(min=0x1000, max=0x20000),
            }
        ),
        _set_core_data,
    )

    def _final_validate(config):
        if "zigbee" not in CORE.loaded_integrations:
            raise cv.Invalid(f"{platform} needs the zigbee: component (it is the only radio stack supported so far)")
        return config

    pin_schema = pins.gpio_base_schema(ZephyrGPIOPin, _validate_pin, modes=pins.GPIO_STANDARD_MODES)

    @pins.PIN_SCHEMA_REGISTRY.register(platform, pin_schema)
    async def pin_to_code(config):
        num = config[CONF_NUMBER]
        var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression("DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0))"), 32, "P0.")
        cg.add(var.set_pin(num))
        if config[CONF_INVERTED]:
            cg.add(var.set_inverted(True))
        cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
        return var

    @coroutine_with_priority(CoroPriority.PLATFORM)
    async def to_code(config):
        # The same generated surface as ESPHome's nrf52 platform, which the shim answers (see shim/).
        cg.add_build_flag("-DUSE_NRF52")
        cg.add_build_flag(f"-DUSE_{platform.upper()}")
        cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
        cg.add_define("ESPHOME_VARIANT", platform.upper())
        from esphome.const import ThreadModel

        cg.add_define(ThreadModel.SINGLE)
        cg.add_global(cg.RawStatement('#include "esphome/components/zephyr/gpio.h"'))
        cg.add_global(cg.RawStatement('#include "esphome/components/zephyr/preferences.h"'))
        zephyr_setup_preferences()
        zephyr_to_code(config)

    def copy_files():
        # Nothing to put in the build folder: the TI build reads ESPHome's generated src/ directly.
        return None

    def _drop_config_cache():
        """ESPHome 2026.9.1 lets `esphome upload` skip config validation when a validated-config cache exists. That path
        never installs the external_components import hook, so this platform's upload_program cannot be found and ESPHome
        falls back to Over-The-Air upload. Removing the cache makes `esphome upload` read the YAML in full (about a second)."""
        try:
            from esphome.compiled_config import compiled_config_path

            compiled_config_path(CORE.config_path.name).unlink(missing_ok=True)
        except Exception:  # best effort; the worst case is the OTA error message
            _LOGGER.debug("could not remove the validated-config cache", exc_info=True)

    def _tool_conf(config):
        return config[platform]

    def run_compile(args, config):
        conf = _tool_conf(config)
        _drop_config_cache()
        zb = config.get("zigbee")
        if zb is None:
            raise EsphomeError(f"{platform} needs the zigbee: component")
        router = bool(zb.get("router"))
        board = conf[CONF_BOARD]
        out_dir = CORE.relative_pioenvs_path(CORE.name)
        try:
            res = ti_build.build(
                build_dir=CORE.build_path,
                src_dir=CORE.relative_src_path(),
                out_dir=out_dir,
                name=CORE.name,
                board=board,
                router=router,
                sleepy=bool(zb.get("sleepy")),
                heap_size=conf.get(CONF_HEAP_SIZE),
                conf=conf,
            )
        except ti_build.BuildError as err:
            hint = ""
            if board == "LP_EM_CC2340R5" and router:
                hint = ("\nA Zigbee router needs about 1.4 kB more RAM than the CC2340R5 has at the default heap (0x2BBC). "
                        "Try heap_size: 0x25BC in the cc23xx: block, or use router: false (an end device).")
            raise EsphomeError(str(err) + hint) from err
        s = res["seconds"]
        _LOGGER.info(
            "Built %s: compiled %d of %d files in %.1f s, linked in %.1f s, total %.1f s",
            res["hex"], res["compiled"], res["files"], s["compile"], s["link"], s["total"],
        )
        for region, m in res["memory"].items():
            _LOGGER.info("  %-8s used %7d B  free %7d B  of %7d B", region, m["used"], m["unused"], m["length"])
        return True

    def upload_program(config, args, host):
        conf = _tool_conf(config)
        _drop_config_cache()
        board = conf[CONF_BOARD]
        hex_file = CORE.relative_pioenvs_path(CORE.name) / "firmware.hex"
        if not hex_file.exists():
            raise EsphomeError(f"{hex_file} not found; run esphome compile first")
        # The probe: from the YAML, else from the port ESPHome offered (macOS names an XDS110's ports
        # /dev/cu.usbmodem<serial><interface digit>), else the --device text taken as a serial number.
        m = re.search(r"usbmodem([A-Za-z0-9]+?)\d$", host or "")
        serial = conf.get(CONF_PROBE_SERIAL) or (m.group(1) if m else (host if host and "/" not in host else None))
        if not serial:
            raise EsphomeError(
                f"Say which debug probe to use: probe_serial: in the {platform}: block, --device <serial>, or pick the XDS110 port that carries the serial. "
                "`/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` lists the serials."
            )
        rc = ti_build.flash(board=board, hex_file=hex_file, probe_serial=serial, conf=conf, work=CORE.relative_build_path("ti_build"))
        if rc != 0:
            raise EsphomeError(f"DSLite flash failed with exit code {rc}")
        return True

    return dict(
        IS_TARGET_PLATFORM=True,
        CONFIG_SCHEMA=config_schema,
        FINAL_VALIDATE_SCHEMA=_final_validate,
        to_code=to_code,
        copy_files=copy_files,
        run_compile=run_compile,
        upload_program=upload_program,
    )
