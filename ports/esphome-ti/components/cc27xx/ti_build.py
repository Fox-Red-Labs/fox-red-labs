"""Build and flash driver for the TI SimpleLink Low Power F3 platforms (cc27xx, cc23xx).

ESPHome generates the C++ (src/ in its build folder). This module does the part PlatformIO does for other
platforms: it runs TI's SysConfig, compiles ESPHome's sources, TI's FreeRTOS/Zigbee glue and our shim with TI Arm
Clang, links as TI's makefile does, patches the image CRC and writes an Intel hex. It is a Python port of
ports/esphome-ti/build.sh, which stays as the stand-alone version of the same steps.
"""

from __future__ import annotations

import filecmp
import glob
import hashlib
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor

from . import periph

_LOGGER = logging.getLogger(__name__)

HERE = Path(__file__).parent
SDK_VERSION = "9.21.00.36"
SDK_GLOB = "simplelink_lowpower_f3_sdk_9_21_00_36*"
CGT_GLOB = "ti-cgt-armllvm_5.1.1*"
SYSCONFIG_GLOB = "sysconfig_1.2[67]*"  # 1.26.3 and 1.27.0 were built with; the newest one found is used

# Plain names ESPHome gives its platform-specific sources; none of them is built here.
OTHER_PLATFORM = re.compile(r"_(esp32|esp8266|rp2|bk72xx|rtl87xx|ln882x|host|libretiny|arduino|esp_idf|idf)\b")
SKIP_DIRS = ("components/zephyr", "components/nrf52")


class BuildError(Exception):
    pass


def _roots() -> list[Path]:
    roots = []
    for env in ("ESPHOME_TI_ROOT",):
        if os.environ.get(env):
            roots.append(Path(os.environ[env]).expanduser())
    roots += [Path("/Applications/ti"), Path.home() / "ti", Path("/opt/ti")]
    return [r for r in roots if r.is_dir()]


def _find(explicit: str | None, env: str, patterns: list[str], what: str, need: str) -> Path:
    if explicit:
        p = Path(explicit).expanduser()
        if p.exists():
            return p
        raise BuildError(f"{what}: configured path {p} does not exist")
    if os.environ.get(env):
        p = Path(os.environ[env]).expanduser()
        if p.exists():
            return p
        raise BuildError(f"{what}: {env}={p} does not exist")
    for root in _roots():
        for pat in patterns:
            hits = sorted(glob.glob(str(root / pat)))
            if hits:
                return Path(hits[-1])
    raise BuildError(
        f"{what} not found. Needed: {need}. Set the key in the YAML, the environment variable {env}, "
        f"or install it under /Applications/ti, ~/ti or /opt/ti."
    )


class Tools:
    def __init__(self, conf: dict) -> None:
        self.sdk = _find(conf.get("sdk_path"), "ESPHOME_TI_F3_SDK", [SDK_GLOB], "TI F3 SDK", f"SimpleLink Low Power F3 SDK {SDK_VERSION}")
        if SDK_VERSION.replace(".", "_") not in self.sdk.name:
            _LOGGER.warning("SDK %s is not version %s; this platform was only built against %s", self.sdk, SDK_VERSION, SDK_VERSION)
        self.cgt = _find(
            conf.get("compiler_path"), "ESPHOME_TI_CGT",
            [CGT_GLOB, "ccs*/ccs/tools/compiler/" + CGT_GLOB], "TI Arm Clang", "TI Arm Clang (ti-cgt-armllvm) 5.1.1",
        )
        self.sysconfig = _find(conf.get("sysconfig_path"), "ESPHOME_TI_SYSCONFIG", [SYSCONFIG_GLOB], "SysConfig", "SysConfig 1.26.3 or 1.27.0")
        self.cc = self.cgt / "bin" / "tiarmclang"
        self.sysconfig_cli = self.sysconfig / "sysconfig_cli.sh"
        for p in (self.cc, self.sysconfig_cli):
            if not p.exists():
                raise BuildError(f"expected tool missing: {p}")



def find_dslite(conf: dict) -> Path:
    return _find(conf.get("dslite_path"), "ESPHOME_TI_DSLITE", ["uniflash_*/dslite.sh"], "DSLite (UniFlash command line)", "UniFlash 9.6 (dslite.sh)")


# --- per-board facts -------------------------------------------------------------------------------------------

BOARDS = {
    "LP_EM_CC2755P10": dict(
        arch=["-mcpu=cortex-m33", "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"],
        port="ARM_CM33_NTZ/non_secure",
        syscfg="esphome_fit.syscfg",
        ccxml="CC2755P10.ccxml.in",
        pin_to_led={16: "red LED"},
        template_gpios={16: ("LED_R", "CONFIG_GPIO_RLED", "LED"), 12: ("LED_G", "CONFIG_GPIO_GLED", "LED"), 20: ("BUTTON_L", "CONFIG_GPIO_BTN1", "GPIO"), 0: ("BUTTON_R", "CONFIG_GPIO_BTN2", "GPIO")},
        nvs_base=0xE4000,  # the 8 kB below ZBOSS's own NVRAM region (0xE6000), inside the application flash
    ),
    "LP_EM_CC2340R5": dict(
        arch=["-mcpu=cortex-m0plus", "-march=thumbv6m", "-mfloat-abi=soft", "-mthumb"],
        port="ARM_CM0",
        syscfg="esphome_fit_cc2340r5.syscfg",
        ccxml="CC2340R5.ccxml.in",
        pin_to_led={},
        template_gpios={14: ("LED_R", "CONFIG_GPIO_RLED", "LED"), 15: ("LED_G", "CONFIG_GPIO_GLED", "LED"), 10: ("BUTTON_L", "CONFIG_GPIO_BTN1", "GPIO"), 9: ("BUTTON_R", "CONFIG_GPIO_BTN2", "GPIO")},
        nvs_base=0x7D000,  # the 8 kB below ZBOSS's own NVRAM region
    ),
}


def _write_if_changed(path: Path, text: str) -> bool:
    if path.exists() and path.read_text() == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return True


def _run(cmd: list[str], cwd: Path | None = None, log: Path | None = None) -> subprocess.CompletedProcess:
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if log is not None:
        log.write_text((r.stdout or "") + (r.stderr or ""))
    return r


def _sync_tree(src: Path, dst: Path) -> None:
    """Copy src over dst, leaving a file's mtime alone when its content did not change (so a rebuild stays small).

    ESPHome's logger sources are copied with their Zephyr branch switched off (the token USE_ZEPHYR renamed): that branch
    needs Zephyr's kernel and UART API. What is left is the generic logger, and shim/logger_f3.cpp supplies pre_setup()
    and write_msg_() on TI's UART2 driver."""
    for root, _dirs, files in os.walk(src):
        rel = Path(root).relative_to(src)
        (dst / rel).mkdir(parents=True, exist_ok=True)
        for f in files:
            s, d = Path(root) / f, dst / rel / f
            if (rel / f).as_posix().startswith("esphome/components/logger/") and f.endswith((".h", ".cpp")):
                new = s.read_text().replace("USE_ZEPHYR", "FRL_ZEPHYR_BRANCH_OFF")
                new = new.replace("#if defined(USE_HOST) || defined(FRL_ZEPHYR_BRANCH_OFF)\n  void write_msg_",
                                  "#if defined(USE_HOST) || defined(FRL_TI_F3)\n  void write_msg_")
                _write_if_changed(d, new)
            elif (rel / f).as_posix() == "esphome/components/light/light_color_values.h":
                # TI Arm Clang's C++ library reports numeric_limits<float>::is_iec559 as false, though the FPU (M33) and the
                # soft-float library (M0+) do single-precision IEEE 754; the assertion only guards that assumption.
                new = s.read_text().replace('static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 float required");',
                                            "// (is_iec559 assertion removed for TI Arm Clang)")
                _write_if_changed(d, new)
            elif not d.exists() or not filecmp.cmp(s, d, shallow=False):
                shutil.copy2(s, d)


def _needs_build(obj: Path, src: Path, flags_hash: str) -> bool:
    dep = obj.with_suffix(".d")
    stamp = obj.with_suffix(".flags")
    if not (obj.exists() and dep.exists() and stamp.exists() and stamp.read_text() == flags_hash):
        return True
    t = obj.stat().st_mtime
    if src.stat().st_mtime > t:
        return True
    text = dep.read_text().replace("\\\n", " ")
    deps = text.split(":", 1)[1].split() if ":" in text else []
    for d in deps:
        try:
            if os.stat(d).st_mtime > t:
                return True
        except OSError:
            return True
    return False


def build(*, build_dir: Path, src_dir: Path, out_dir: Path, name: str, board: str, router: bool, conf: dict, sleepy: bool = False, heap_size: int | None = None, jobs: int | None = None, per: dict | None = None, esphome_flags: list[str] | None = None) -> dict:
    """Return {'hex':, 'elf':, 'map':, 'memory': {...}, 'seconds': {...}}."""
    t0 = time.time()
    tools = Tools(conf)
    b = BOARDS[board]
    sdk, cgt = tools.sdk, tools.cgt
    ex = sdk / "examples" / "rtos" / board / "zigbee" / "onoff_light"
    if not ex.is_dir():
        raise BuildError(f"TI example not found: {ex}")
    work = build_dir / "ti_build"
    gen, ti, obj_dir, src = work / "gen", work / "ti", work / "obj", work / "src"
    for d in (gen, ti, obj_dir, out_dir):
        d.mkdir(parents=True, exist_ok=True)
    role = "ROUTER" if router else "END_DEVICE"

    # 1. SysConfig: TI's Zigbee configuration, with the role this YAML asks for
    syscfg_text = (HERE / "ti" / b["syscfg"]).read_text()
    if not router:
        syscfg_text = syscfg_text.replace('["zr"]', '["zed"]')
    if heap_size is not None:
        syscfg_text = re.sub(r"FreeRTOS\.heapSize = 0x[0-9a-fA-F]+;", f"FreeRTOS.heapSize = 0x{heap_size:08X};", syscfg_text)
    per = per or {"uart_log": None, "i2c": [], "adc": [], "pwm": [], "nvs": True}
    # TI's example claims the LED and button pins as GPIO instances. A pin the YAML gives to a TI driver (PWM, I2C, ADC)
    # is taken from that instance; the instance's macro stays defined as the DIO number (the GPIO driver indexes by DIO),
    # so TI's LED and button code still compiles.
    driver_dios = {p["dio"] for p in per["pwm"]} | set(per["adc"]) | {x for bus in per["i2c"] for x in (bus["sda"], bus["scl"])}
    dropped = []
    for dio, (var, macro, mod) in b["template_gpios"].items():
        if dio in driver_dios:
            syscfg_text = syscfg_text.replace(f"const {var} = {mod}.addInstance();", f"const {var} = {{}};  // DIO{dio} is given to a TI driver by the YAML")
            dropped.append(f"-D{macro}={dio}")
    syscfg_text += periph.syscfg(per, board)
    if per.get("nvs"):
        syscfg_text += (
            'const FRL_NVS = scripting.addModule("/ti/drivers/NVS");\n'
            "const frlNvs = FRL_NVS.addInstance();\n"
            'frlNvs.$name = "CONFIG_FRL_NVS";\n'
            f"frlNvs.internalFlash.regionBase = 0x{b['nvs_base']:X};\n"
            f"frlNvs.internalFlash.regionSize = 0x{periph.NVS_BYTES:X};\n"
        )
    syscfg_file = work / "esphome_fit.syscfg"
    changed = _write_if_changed(syscfg_file, syscfg_text)
    if changed or not list(gen.glob("*.c")):
        _LOGGER.info("Running SysConfig (%s)", role.lower())
        for sub in ("osif", "dmm"):
            shutil.rmtree(ti / sub, ignore_errors=True)
            shutil.copytree(ex / sub, ti / sub)
        r = _run(
            [str(tools.sysconfig_cli), "--compiler", "ticlang", "--product", str(sdk / ".metadata" / "product.json"), "--output", str(gen), str(syscfg_file)],
            log=work / "sysconfig.log",
        )
        if r.returncode != 0:
            raise BuildError("SysConfig failed:\n" + (r.stdout + r.stderr)[-1500:])
        cmd_lines = (ex / "freertos" / "ticlang" / "lpf3_zigbee_freertos.cmd").read_text().split("\n")
        if per.get("nvs"):
            # the application flash ends below the ESPHome settings region (CONFIG_FRL_NVS), which sits under ZBOSS's NVRAM
            extra = f" - 0x{periph.NVS_BYTES:X}"
            for i, line in enumerate(cmd_lines):
                if line.startswith("#define FLASH_SIZE") and "NVSINTERNAL_ZB_SIZE" in line:
                    cmd_lines[i] = line[:-1] + extra + ")" if line.rstrip().endswith(")") else line + extra
        (gen / "lpf3_zigbee_freertos.cmd").write_text("\n".join(cmd_lines))

    _write_if_changed(gen / "frl_periph.h", periph.header(per))

    # 2. the C++ ESPHome generated, with the shim's headers laid over it (quote includes look beside the includer first)
    _sync_tree(src_dir, src)
    shim = HERE / "shim"
    for h in shim.rglob("*.h"):
        d = src / h.relative_to(shim)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or not filecmp.cmp(h, d, shallow=False):
            shutil.copy2(h, d)

    # 3. compile
    common = list(b["arch"]) + [
        "-Oz", "-flto", "-gdwarf-3", "-Wall",
        "-Wno-gnu-folding-constant", "-Wno-unaligned-access", "-Wno-unused-variable", "-Wno-unused-function", "-Wno-sign-compare",
        f"@{gen}/ti_utils_build_compiler.opt", "-DSYNC_WRITE", "-DZB_TI_F3_ZBOSS", "-DZB_DEFAULT_VENDOR",
        f"-I{gen}", f"-I{ti}", f"-I{sdk}/source", f"-I{sdk}/source/ti/zigbee/osif/include", f"-I{sdk}/source/ti/zigbee/include",
        f"-I{sdk}/source/ti/common/cc26xx/flash_interface/", f"-I{sdk}/kernel/freertos",
        f"-I{sdk}/source/third_party/freertos/include", f"-I{sdk}/source/third_party/freertos/portable/GCC/{b['port']}",
    ]
    common += dropped
    if not router and not sleepy:
        # TI's example is a sleepy end device that polls its parent every 3 s, so a command from the coordinator waits for
        # the next poll. ESPHome's default (zigbee: sleepy: false) is an end device that keeps its receiver on.
        common.append("-DED_RX_ALWAYS_ON=1")
    posix = [f"-I{sdk}/source/ti/posix/ticlang"]
    cxx = [
        "-std=gnu++20", "-fno-exceptions", "-fno-rtti", "-U_INLINE", "-Wno-deprecated-declarations",
        "-DUSE_ZEPHYR", "-DUSE_NRF52", "-DFRL_TI_F3", "-DCONFIG_SETTINGS", *(esphome_flags or []), f"-DCONFIG_ZIGBEE_ROLE_{role}", f"-DFRL_BOARD_{board.split('_')[-1]}", f"-I{src}", "-idirafter", f"{sdk}/source/ti/posix/ticlang",
    ]
    jobs_list: list[tuple[Path, list[str]]] = []
    for f in [ti / "osif/ti_f3_main.c", ti / "osif/ti_f3_led_buttons.c", ti / "dmm/rcl_stub_dmm.c", shim / "ti_glue.c", *sorted(gen.glob("*.c"))]:
        jobs_list.append((f, common + posix))
    cpp: list[Path] = [src / "main.cpp"]
    for f in sorted((src / "esphome").rglob("*.cpp")):
        rel = f.relative_to(src).as_posix()
        if any(s in rel for s in SKIP_DIRS) or OTHER_PLATFORM.search(f.stem):
            continue
        if f.stem.endswith("_zephyr") and "zigbee" not in rel:
            continue  # other components' Zephyr code needs Zephyr; the shim answers for the few that matter
        cpp.append(f)
    cpp += sorted(shim.glob("*.cpp"))
    for f in cpp:
        jobs_list.append((f, common + cxx))

    objs: list[Path] = []
    todo = []
    for f, flags in jobs_list:
        key = str(f.relative_to(work) if f.is_relative_to(work) else f.relative_to(HERE)).replace("/", "_").replace(".", "_")
        o = obj_dir / (key + ".o")
        objs.append(o)
        fh = hashlib.sha1(" ".join(flags).encode()).hexdigest()
        if _needs_build(o, f, fh):
            todo.append((f, o, flags, fh))
    _LOGGER.info("Compiling %d of %d files (TI Arm Clang)", len(todo), len(jobs_list))

    def one(item):
        f, o, flags, fh = item
        r = subprocess.run([str(tools.cc), *flags, "-MD", "-MF", str(o.with_suffix(".d")), "-c", str(f), "-o", str(o)], capture_output=True, text=True)
        if r.returncode != 0:
            return f, r.stderr
        o.with_suffix(".flags").write_text(fh)
        return f, None

    errors = []
    with ThreadPoolExecutor(max_workers=jobs or os.cpu_count() or 4) as ex_:
        for f, err in ex_.map(one, todo):
            if err:
                errors.append((f, err))
    if errors:
        msg = "\n".join(f"FAILED {f}\n{e[:1500]}" for f, e in errors[:3])
        raise BuildError(f"{len(errors)} file(s) failed to compile:\n{msg}")
    t_compile = time.time()

    # 4. link as TI's makefile does (their linker has no --wrap)
    out_elf, out_map = out_dir / "firmware.elf", out_dir / "firmware.map"
    link = [str(tools.cc), "-Wl,-u,_c_int00", *map(str, objs), *common, "-Wl,--reread_libs", f"-L{sdk}/source",
            "ti_utils_build_linker.cmd.genlibs", "lpf3_zigbee_freertos.cmd", f"-Wl,-m,{out_map}", "-Wl,--rom_model", "-Wl,--warn_sections",
            f"-L{cgt}/lib", "-llibc.a", "-o", str(out_elf)]
    hex_out = out_dir / "firmware.hex"
    _LOGGER.info("Linking")
    r = _run(link, cwd=gen, log=work / "link.log")
    if r.returncode != 0:
        raise BuildError("Link failed:\n" + (r.stdout + r.stderr)[-2500:])
    crc = _run([str(sdk / "tools/common/crc_tool/crc_tool"), "patch-image", "--elf", str(out_elf), "--symbol-prefix",
                "ti_utils_build_GenMap_sym_CRC", "--output", str(out_dir / "firmware.crc.elf")], log=work / "crc.log")
    if crc.returncode != 0:
        raise BuildError("CRC patch failed:\n" + (work / "crc.log").read_text()[-800:])
    r = _run([str(cgt / "bin/tiarmobjcopy"), "-O", "ihex", str(out_dir / "firmware.crc.elf"), str(hex_out)])
    if r.returncode != 0:
        raise BuildError("objcopy failed:\n" + r.stderr[-800:])
    mem = memory_table(out_map)
    t1 = time.time()
    return dict(hex=out_dir / "firmware.hex", elf=out_dir / "firmware.crc.elf", map=out_map, memory=mem,
                seconds=dict(total=t1 - t0, compile=t_compile - t0, link=t1 - t_compile), compiled=len(todo), files=len(jobs_list))


def memory_table(map_file: Path) -> dict:
    """The linker's own MEMORY CONFIGURATION table: {region: (origin, length, used, unused)}."""
    rows = {}
    text = map_file.read_text(errors="replace")
    lines = text.splitlines()
    start = next((i for i, l in enumerate(lines) if "MEMORY CONFIGURATION" in l), None)
    if start is None:
        return rows
    for line in lines[start + 1 : start + 14]:
        p = line.split()
        if len(p) >= 5 and all(re.fullmatch(r"[0-9a-fA-F]{8}", x) for x in p[1:5]):
            rows[p[0]] = dict(origin=int(p[1], 16), length=int(p[2], 16), used=int(p[3], 16), unused=int(p[4], 16))
    return rows
    for line in m.group(1).splitlines():
        p = line.split()
        if len(p) >= 5 and re.fullmatch(r"[0-9a-fA-F]{8}", p[1]) and re.fullmatch(r"[0-9a-fA-F]{8}", p[2]):
            try:
                rows[p[0]] = dict(origin=int(p[1], 16), length=int(p[2], 16), used=int(p[3], 16), unused=int(p[4], 16))
            except ValueError:
                pass
    return rows


# --- flashing --------------------------------------------------------------------------------------------------

def write_ccxml(board: str, probe_serial: str, dest: Path) -> Path:
    text = (HERE / "ccxml" / BOARDS[board]["ccxml"]).read_text().replace("@PROBE_SERIAL@", probe_serial)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text)
    return dest


def flash(*, board: str, hex_file: Path, probe_serial: str, conf: dict, work: Path) -> int:
    dslite = find_dslite(conf)
    ccxml = write_ccxml(board, probe_serial, work / f"{board}-{probe_serial}.ccxml")
    cmd = [str(dslite), "--mode", "flash", "-c", str(ccxml), "-e", "-f", "-u", str(hex_file)]
    _LOGGER.info("Flashing with %s", " ".join(cmd))
    return subprocess.call(cmd)
