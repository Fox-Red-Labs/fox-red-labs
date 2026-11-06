"""Build and flash driver for the TI CC35xx platform (cc35xx).

ESPHome generates the C++ (src/ in its build folder). This module does what PlatformIO does for other platforms:
it takes TI's mqtt_client example from the Wi-Fi SDK as the board support (FreeRTOS, TI's Wi-Fi driver glue, lwIP
interface, UART console, SysConfig file), runs SysConfig, compiles TI's C and ESPHome's C++ with TI Arm Clang, links
as TI's makefile does, and signs the image with the SimpleLink Wi-Fi Toolbox. Flashing uses the Toolbox programmer.

Changes to TI's files, all made on the build copy: frl_wifi_append.c is appended to wifi_if.c (entry points for the
Wi-Fi backend), and mqtt_client_app.c and mqtt_if.c are not built. Changes to ESPHome's generated sources, on the build
copy: the socket and IP-address files take their lwIP branch instead of their Zephyr one (USE_ZEPHYR is renamed there).
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

_LOGGER = logging.getLogger(__name__)

HERE = Path(__file__).parent
SDK_VERSION = "10.20.00.39"
SDK_GLOB = "simplelink_wifi_sdk_10_20_00_39*"
CGT_GLOB = "ti-cgt-armllvm_5.1.1*"
SYSCONFIG_GLOB = "sysconfig_1.28.*"
TOOLBOX_GLOB = "simplelink_wifi_toolbox_*4_3_23*"
EXAMPLE = "examples/rtos/LP_EM_CC35X1/demos/mqtt_client"
TI_C = ["wifi_if.c", "tcpip_if.c", "dns_if.c", "led_if.c", "button_if.c", "crc.c", "nvocmp_cc35xx.c",
        "adaptation/osi_dpl.c", "adaptation/osi_filesystem.c", "adaptation/uart_term.c", "adaptation/syslog.c",
        "freertos/main_freertos.c"]
OTHER_PLATFORM = re.compile(r"_(esp32|esp8266|rp2|bk72xx|rtl87xx|ln882x|host|libretiny|arduino|esp_idf|idf|nrf52)\b")
SKIP_DIRS = ("components/zephyr", "components/nrf52")
# ESPHome headers whose Zephyr branch would pull Zephyr's network headers; in the build copy they take the lwIP branch.
UNZEPHYR = ("components/socket/headers.h", "components/socket/socket.cpp", "components/network/ip_address.h", "components/logger/")


class BuildError(Exception):
    pass


def _roots() -> list[Path]:
    roots = [Path(os.environ["ESPHOME_TI_ROOT"]).expanduser()] if os.environ.get("ESPHOME_TI_ROOT") else []
    roots += [Path("/Applications/ti"), Path.home() / "ti", Path("/opt/ti")]
    return [r for r in roots if r.is_dir()]


def _find(explicit, env, patterns, what, need) -> Path:
    for p in (explicit, os.environ.get(env)):
        if p:
            p = Path(p).expanduser()
            if p.exists():
                return p
            raise BuildError(f"{what}: {p} does not exist")
    for root in _roots():
        for pat in patterns:
            hits = sorted(glob.glob(str(root / pat)))
            if hits:
                return Path(hits[-1])
    raise BuildError(f"{what} not found. Needed: {need}. Set the key in the YAML, {env}, or install it under /Applications/ti, ~/ti or /opt/ti.")


class Tools:
    def __init__(self, conf: dict) -> None:
        self.sdk = _find(conf.get("sdk_path"), "ESPHOME_TI_WIFI_SDK", [SDK_GLOB], "TI Wi-Fi SDK", f"SimpleLink Wi-Fi SDK {SDK_VERSION}")
        self.cgt = _find(conf.get("compiler_path"), "ESPHOME_TI_CGT", [CGT_GLOB, "ccs*/ccs/tools/compiler/" + CGT_GLOB],
                         "TI Arm Clang", "TI Arm Clang (ti-cgt-armllvm) 5.1.1")
        self.sysconfig = _find(conf.get("sysconfig_path"), "ESPHOME_TI_SYSCONFIG_WIFI", [SYSCONFIG_GLOB], "SysConfig", "SysConfig 1.28")
        self.toolbox = _find(conf.get("toolbox_path"), "ESPHOME_TI_TOOLBOX", [TOOLBOX_GLOB], "SimpleLink Wi-Fi Toolbox", "SimpleLink Wi-Fi Toolbox 4.3.23")
        self.cc = self.cgt / "bin" / "tiarmclang"
        self.sysconfig_cli = self.sysconfig / "sysconfig_cli.sh"
        for p in (self.cc, self.sysconfig_cli):
            if not p.exists():
                raise BuildError(f"expected tool missing: {p}")


def _write_if_changed(path: Path, text: str) -> bool:
    if path.exists() and path.read_text() == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return True


def _run(cmd, cwd=None, log=None, env=None) -> subprocess.CompletedProcess:
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, env=env)
    if log is not None:
        Path(log).write_text((r.stdout or "") + (r.stderr or ""))
    return r


def _sync_tree(src: Path, dst: Path) -> None:
    for root, _dirs, files in os.walk(src):
        rel = Path(root).relative_to(src)
        (dst / rel).mkdir(parents=True, exist_ok=True)
        for f in files:
            s, d = Path(root) / f, dst / rel / f
            text_rel = (rel / f).as_posix()
            if any(text_rel.startswith("esphome/" + u) for u in UNZEPHYR):
                new = s.read_text().replace("USE_ZEPHYR", "FRL_ZEPHYR_BRANCH_OFF")
                # ip_address.h includes lwIP's address types only for ESP32 and LibreTiny; CC35xx uses the same lwIP branch.
                new = new.replace("#if defined(USE_ESP32) || defined(USE_LIBRETINY) ||", "#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_CC35XX) ||")
                # The logger's write_msg_ is out of line on cc35xx (shim/logger_cc35xx.cpp), as on host and Zephyr.
                new = new.replace("#if defined(USE_HOST) || defined(FRL_ZEPHYR_BRANCH_OFF)\n  void write_msg_", "#if defined(USE_HOST) || defined(USE_CC35XX)\n  void write_msg_")
                _write_if_changed(d, new)
            elif not d.exists() or not filecmp.cmp(s, d, shallow=False):
                shutil.copy2(s, d)


def _needs_build(obj: Path, src: Path, flags_hash: str) -> bool:
    dep, stamp = obj.with_suffix(".d"), obj.with_suffix(".flags")
    if not (obj.exists() and dep.exists() and stamp.exists() and stamp.read_text() == flags_hash):
        return True
    t = obj.stat().st_mtime
    if src.stat().st_mtime > t:
        return True
    text = dep.read_text().replace("\\\n", " ")
    for d in (text.split(":", 1)[1].split() if ":" in text else []):
        try:
            if os.stat(d).st_mtime > t:
                return True
        except OSError:
            return True
    return False


def _prepare_ti(tools: Tools, work: Path) -> tuple[Path, Path]:
    """Copy TI's example once, append our Wi-Fi entry points, run SysConfig. Returns (ti_dir, gen_dir)."""
    ex = tools.sdk / EXAMPLE
    if not ex.is_dir():
        raise BuildError(f"TI example not found: {ex}")
    ti, gen = work / "ti", work / "gen"
    stamp = work / "ti.stamp"
    want = hashlib.sha1((str(ex) + (HERE / "ti" / "frl_wifi_append.c").read_text()).encode()).hexdigest()
    if not (stamp.exists() and stamp.read_text() == want and (gen / "ti_drivers_config.c").exists()):
        shutil.rmtree(ti, ignore_errors=True)
        shutil.rmtree(gen, ignore_errors=True)
        shutil.copytree(ex, ti, ignore=shutil.ignore_patterns("*.obj", "*.out", "*.map", "gcc", "README*"))
        w = ti / "wifi_if.c"
        t = w.read_text()
        # The station role cannot report its channel through Wlan_Get (TI's driver answers only for the AP role), so the
        # channel is kept from the connect event: one line in TI's event handler, on the build copy.
        anchor = "            SET_STATUS_BIT(m_wifiCtx.Status, STATUS_BIT_STA_CONNECTION);\n"
        if t.count(anchor) != 1:
            raise BuildError("wifi_if.c changed: the connect-event anchor is not unique")
        t = "int g_frl_channel = 0;\nvolatile int g_frl_scan_done = 0;\n" + t.replace(anchor, anchor + "            g_frl_channel = pWlanEvent->Data.Connect.Channel;\n")
        # ... and the end of a scan, so the Wi-Fi backend can hand ESPHome its scan results.
        scan_anchor = "        PrintScanResults(m_wifiCtx.netEntriesSize);\n"
        if t.count(scan_anchor) != 1:
            raise BuildError("wifi_if.c changed: the scan-result anchor is not unique")
        t = t.replace(scan_anchor, scan_anchor + "        g_frl_scan_done = 1;\n")
        w.write_text(t + (HERE / "ti" / "frl_wifi_append.c").read_text())
        gen.mkdir(parents=True)
        cmd = [str(tools.sysconfig_cli), "--compiler", "ticlang", "--product", str(tools.sdk / ".metadata/product.json"),
               "--product", str(tools.toolbox / ".metadata/product.json"), "--output", str(gen), str(ti / "freertos/mqtt_client.syscfg")]
        _LOGGER.info("Running SysConfig")
        r = _run(cmd, log=work / "sysconfig.log")
        if r.returncode != 0:
            raise BuildError("SysConfig failed:\n" + (r.stdout + r.stderr)[-1500:])
        shutil.copy2(ti / "freertos/ticlang/linker.cmd", gen / "linker.cmd")
        stamp.write_text(want)
    return ti, gen


def build(*, build_dir: Path, src_dir: Path, out_dir: Path, name: str, conf: dict, esphome_flags: list[str] | None = None, jobs: int | None = None) -> dict:
    t0 = time.time()
    tools = Tools(conf)
    sdk, cgt = tools.sdk, tools.cgt
    work = build_dir / "ti35_build"
    obj_dir, src = work / "obj", work / "src"
    for d in (obj_dir, out_dir):
        d.mkdir(parents=True, exist_ok=True)
    ti, gen = _prepare_ti(tools, work)

    _sync_tree(src_dir, src)
    shim = HERE / "shim"
    for h in shim.rglob("*.h"):
        d = src / h.relative_to(shim)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or not filecmp.cmp(h, d, shallow=False):
            shutil.copy2(h, d)

    s = str(sdk)
    inc = [f"-I{ti}", f"-I{gen}", f"-I{s}/source", f"-I{s}/source/third_party/mbedtls/include",
           f"-I{s}/source/third_party/mbedtls/ti/configs", f"-I{s}/source/third_party/mbedtls/ti/port", f"-I{s}/kernel/freertos",
           f"-I{s}/source/third_party/freertos/include", f"-I{s}/source/third_party/freertos/portable/GCC/ARM_CM33_NTZ/non_secure",
           f"-I{s}/source/ti/net/ble_interface/inc_adapt", f"-I{s}/source/ti/net/ble_interface/inc_common",
           f"-I{s}/source/ti/drivers/net/wifi/wifi_platform/cc35xx/inc_common", f"-I{s}/source/ti/net/wifi_stack/inc_common",
           f"-I{s}/source/ti/drivers/net/wifi/wifi_host_driver/inc_common", f"-I{s}/source/ti/drivers/net/wifi/wifi_host_driver/inc_adapt",
           f"-I{s}/tools/TI_Containers", f"-I{s}/source/third_party/lwip/lwip-stack/src/include",
           f"-I{s}/source/third_party/lwip/ti_config/lwip-port/osi/include", f"-I{s}/source/third_party/lwip/lwip-contrib"]
    arch = ["-mcpu=cortex-m33", "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"]
    ti_flags = arch + [f"@{gen}/ti_utils_build_compiler.opt"] if (gen / "ti_utils_build_compiler.opt").exists() else list(arch)
    ti_flags += ['-DMBEDTLS_CONFIG_FILE="config-hsm.h"', "-DNVOCMP_SPS_USE_CBC", '-DMBEDTLS_PSA_CRYPTO_CONFIG_FILE="config-psa-crypto-hsm.h"',
                 "-DCC35XX", "-DDeviceFamily_CC35XX", "-DNVOCMP_POSIX_MUTEX", "-gdwarf-3"]
    c_flags = ti_flags + ["-Ofast", "-Wall", "-Wno-unused-variable", f"-I{s}/source/ti/posix/ticlang", "-DTHREADSTACKSIZE=16384"] + inc
    cxx_flags = ti_flags + [
        "-Oz", "-std=gnu++20", "-fno-exceptions", "-fno-rtti", "-U_INLINE", "-Wno-deprecated-declarations",
        "-Wno-unused-variable", "-Wno-unused-function", "-Wno-sign-compare", "-Wno-unaligned-access",
        *(esphome_flags or []), "-DUSE_ZEPHYR", "-DUSE_CC35XX", "-include", "cerrno", f"-I{src}", *inc, "-idirafter", f"{s}/source/ti/posix/ticlang",
    ]

    jobs_list: list[tuple[Path, list[str]]] = [(ti / f, c_flags) for f in TI_C]
    jobs_list += [(f, c_flags) for f in sorted(gen.glob("*.c"))]
    cpp: list[Path] = [src / "main.cpp"]
    for f in sorted((src / "esphome").rglob("*.cpp")):
        rel = f.relative_to(src).as_posix()
        if any(x in rel for x in SKIP_DIRS) or OTHER_PLATFORM.search(f.stem):
            continue
        if f.stem.endswith("_zephyr"):
            continue  # the shim answers for core/wake_zephyr, logger_zephyr and mdns_zephyr
        cpp.append(f)
    cpp += sorted(shim.glob("*.cpp"))
    jobs_list += [(f, cxx_flags) for f in cpp]

    objs, todo = [], []
    for f, flags in jobs_list:
        key = re.sub(r"[/.]", "_", str(f.relative_to(work) if f.is_relative_to(work) else f.relative_to(HERE)))
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
    with ThreadPoolExecutor(max_workers=jobs or os.cpu_count() or 4) as pool:
        for f, err in pool.map(one, todo):
            if err:
                errors.append((f, err))
    if errors:
        msg = "\n".join(f"FAILED {f}\n{e[:2500]}" for f, e in errors[:4])
        raise BuildError(f"{len(errors)} file(s) failed to compile:\n{msg}")
    t_compile = time.time()

    out_elf, out_map = out_dir / "firmware.out", out_dir / "firmware.map"
    libs = ["third_party/lwip/lib/ticlang/lwip.a", "third_party/hostap/lib/ticlang/hostap.a", "third_party/nimble/lib/ticlang/nimble.a",
            "ti/net/wifi_stack/lib/ticlang/wifi_stack.a", "ti/net/ble_interface/lib/ticlang/ble_interface.a",
            "ti/drivers/net/wifi/wifi_host_driver/lib/ticlang/wifi_host_driver.a",
            "ti/drivers/net/wifi/wifi_platform/cc35xx/lib/ticlang/wifi_platform_cc35xx.a", "ti/utils/FWU/lib/ticlang/FWU.a"]
    link = [str(tools.cc), "-Wl,-u,_c_int00", *map(str, objs), *arch, f"-L{s}/source", "-Wl,--reread_libs",
            *[f"-Wl,-l{s}/source/{l}" for l in libs], "ti_utils_build_linker.cmd.genlibs", "linker.cmd",
            f"-Wl,-m,{out_map}", "-Wl,--rom_model", "-Wl,--warn_sections", f"-L{cgt}/lib", "-llibc.a", "-o", str(out_elf)]
    _LOGGER.info("Linking")
    r = _run(link, cwd=gen, log=work / "link.log")
    if r.returncode != 0:
        raise BuildError("Link failed:\n" + (r.stdout + r.stderr)[-4000:])
    t_link = time.time()

    # Sign and lay out the flash images with the Toolbox, as TI's projects do after the link.
    tb = tools.toolbox
    r = _run(["make", "-s", "-f", str(tb / "scripts/makefile"), "all", f"SDK_DIR={s}", f"SYSCONFIG_ARTIFACT={gen}",
              f"BUILD_DIR={gen}", f"BUILD_ARTIFACT={out_elf}", f"TOOLBOX_DIR={tb}"], cwd=gen, log=work / "sign.log")
    settings = gen / "toolbox" / "tool_settings.json"
    if r.returncode != 0 or not settings.exists():
        raise BuildError("Toolbox signing failed:\n" + (work / "sign.log").read_text()[-2500:])
    _write_if_changed(out_dir / "tool_settings.path", str(settings))
    mem = memory_table(out_map)
    t1 = time.time()
    return dict(bundle=settings, elf=out_elf, map=out_map, memory=mem, compiled=len(todo), files=len(jobs_list),
                seconds=dict(total=t1 - t0, compile=t_compile - t0, link=t_link - t_compile, sign=t1 - t_link))


def memory_table(map_file: Path) -> dict:
    rows = {}
    lines = map_file.read_text(errors="replace").splitlines()
    start = next((i for i, l in enumerate(lines) if "MEMORY CONFIGURATION" in l), None)
    if start is None:
        return rows
    for line in lines[start + 1 : start + 16]:
        p = line.split()
        if len(p) >= 5 and all(re.fullmatch(r"[0-9a-fA-F]{8}", x) for x in p[1:5]):
            rows[p[0]] = dict(origin=int(p[1], 16), length=int(p[2], 16), used=int(p[3], 16), unused=int(p[4], 16))
    return rows


def flash(*, out_dir: Path, probe_serial: str, conf: dict) -> int:
    """Erase the whole flash (an image laid out by another program can leave the new one silent), then program."""
    tools = Tools(conf)
    settings = Path((out_dir / "tool_settings.path").read_text().strip())
    prog = str(tools.toolbox / "simplelink-wifi-toolbox")
    base = [prog, "programmer", "-i", "XDS110", "-param1", probe_serial]
    _LOGGER.info("Erasing the flash (probe %s)", probe_serial)
    rc = subprocess.call(base + ["full_flash_erase", "--flash_type", "AUTO_DETECT", "--activation_type", "sdk_example_key"], cwd=tools.toolbox)
    if rc != 0:
        return rc
    _LOGGER.info("Programming %s", settings)
    return subprocess.call(base + ["programming", "--tool_settings", str(settings)], cwd=tools.toolbox)
