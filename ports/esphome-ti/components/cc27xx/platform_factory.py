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
import voluptuous as vol
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

from . import periph, ti_build

_LOGGER = logging.getLogger(__name__)

CONF_SDK_PATH = "sdk_path"
CONF_COMPILER_PATH = "compiler_path"
CONF_SYSCONFIG_PATH = "sysconfig_path"
CONF_DSLITE_PATH = "dslite_path"
CONF_PROBE_SERIAL = "probe_serial"
CONF_HEAP_SIZE = "heap_size"

ZephyrGPIOPin = zephyr_ns.class_("ZephyrGPIOPin", cg.InternalGPIOPin)

_TI_PLATFORMS = ("cc27xx", "cc23xx", "cc35xx")


def _let_ti_platforms_pass_nrf52_gates():
    """ESPHome gates i2c, spi, uart and other buses with cv.only_on([... "nrf52" ...]) and builds their Zephyr code
    for the "nrf52" target. This platform builds the same generated surface (see the shim), so the gate lets the TI
    platform names through wherever it lets nrf52 through. It has to run before those components are imported."""
    if getattr(cv, "_ti_only_on_patched", False):
        return
    original = cv.only_on
    original_default = cv.SplitDefault.default

    def only_on(platforms):
        plist = platforms if isinstance(platforms, list) else [platforms]
        if "nrf52" in plist:
            plist = plist + [p for p in _TI_PLATFORMS if p not in plist]
        return original(plist)

    cv.only_on = only_on

    def split_default(self):
        """Per-platform defaults (SplitDefault) have no entry for a TI platform name. Use the nrf52 entry, whose Zephyr
        build this surface follows, except that GPIO interrupts are not wired up here, so use_interrupt defaults to false
        (the binary_sensor polls the pin in ESPHome's loop)."""
        if CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) in _TI_PLATFORMS:
            if self.schema == "use_interrupt":
                return vol.default_factory(False)
            if self.schema == "hardware_uart":  # the logger's UART choice: nrf52's USB CDC does not exist here
                return vol.default_factory(vol.UNDEFINED)
            if "nrf52" in self._defaults:
                return self._defaults["nrf52"]
        return original_default.fget(self)

    cv.SplitDefault.default = property(split_default, original_default.fset)
    cv._ti_only_on_patched = True


_let_ti_platforms_pass_nrf52_gates()


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
    board = CORE.data.get("ti_f3_board")
    if board is None:
        return n
    table = periph.board_pins(board)
    if n not in table:
        raise cv.Invalid(f"DIO{n} does not exist on {board}; the chip has {', '.join('DIO%d' % d for d in sorted(table))}")
    info = table[n]
    for use in info["use"]:
        if use.startswith("SWD") or use.startswith("32 kHz"):
            raise cv.Invalid(f"DIO{n} is the {use} on {board} and cannot be used as a GPIO here")
    shared = [u for u in info["use"] if "LED" not in u and "button" not in u]
    if shared:
        _LOGGER.warning("DIO%d is also used on %s for: %s (BoosterPack header pin %s)", n, board, ", ".join(shared), info["header"])
    return n


def _logger_uart_selection_for_ti(platform):
    """ESPHome's logger asks the platform which UART it offers and has no answer for an out-of-tree platform (it raises
    NotImplementedError). Here the log always goes to the LaunchPad kit's backchannel UART (the XDS110 application port),
    so a hardware_uart: choice is refused and the logger's own default is used."""
    from esphome.components import logger as _logger

    if getattr(_logger, "_ti_uart_selection_patched", False):
        return
    original = _logger.uart_selection

    def uart_selection(value):
        if CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) in _TI_PLATFORMS:
            raise cv.Invalid(f"{platform} logs to the board's backchannel UART; hardware_uart is not used")
        return original(value)

    _logger.uart_selection = uart_selection
    _logger._ti_uart_selection_patched = True


def make_platform(platform: str, boards: dict[str, str], default_board: str) -> dict:
    """Return the module-level names ESPHome looks for in esphome.components.<platform>."""

    def _set_core_data(config):
        CORE.data["ti_f3_board"] = config[CONF_BOARD]
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

    _logger_uart_selection_for_ti(platform)

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
        try:
            per = periph.collect(config, board)
        except ValueError as err:
            raise EsphomeError(str(err)) from err
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
                per=per,
                # ESPHome hands some options (the log level, feature switches) to the compiler as build flags, which other
                # platforms pass through PlatformIO; here they go straight to TI Arm Clang. Only defines and undefines are taken.
                esphome_flags=sorted(f for f in CORE.build_flags if f.startswith(("-D", "-U"))),
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
