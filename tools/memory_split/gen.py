#!/usr/bin/env python3
"""Build matrix/memory-breakdown.json and print per-build tables for memory-breakdown.md."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
import ti_map_split as T
import gnu_map_split as G

BASE = os.path.expanduser("<repo>/cc2340r5")
SDK_RTOS = "F3 SDK 9.21.00.36, FreeRTOS, TI Arm Clang 5.1.1"
SDK_NORTOS = "F3 SDK 9.21.00.36, no RTOS, TI Arm Clang 5.1.1"
TI_BUILDS = [
    ("empty_nortos", "Empty project, no RTOS (TI empty)", SDK_NORTOS, "matrix/empty_nortos/ticlang/empty.map"),
    ("empty_rtos", "Empty project, FreeRTOS (TI empty)", SDK_RTOS, "matrix/empty_rtos/freertos/ticlang/empty.map"),
    ("basic_ble", "Bluetooth (TI basic_ble)", SDK_RTOS, "matrix/basic_ble/freertos/ticlang/basic_ble.map"),
    ("data_stream", "Bluetooth (TI data_stream)", SDK_RTOS, "matrix/data_stream/freertos/ticlang/data_stream.map"),
    ("rfPacketTx", "Proprietary radio packet sender (TI rfPacketTx)", SDK_RTOS, "matrix/rfPacketTx/freertos/ticlang/rfPacketTx.map"),
    ("onoff_switch_zed", "Zigbee end device (TI onoff_switch), logging on", SDK_RTOS, "matrix/onoff_switch_zed/freertos/ticlang/onoff_switch.map"),
    ("onoff_light", "Zigbee router (TI onoff_light), logging on", SDK_RTOS, "zigbee/onoff_light/freertos/ticlang/onoff_light.map"),
    ("ota_on_v1", "Zigbee OTA client (TI onoff_switch_ota_client_onchip), receiver always on", SDK_RTOS, "matrix/ota_on_v1/freertos/ticlang/onoff_switch_ota_client_onchip.map"),
    ("coremark_O3", "CoreMark bare metal, -O3", SDK_NORTOS, "baremetal/build-O3/bench.map"),
]
Z_BUILDS = [
    ("zephyr_blinky", "Zephyr blink, upstream Zephyr 4.4.2", "Upstream Zephyr 4.4.2, Zephyr kernel, GCC 12.2",
     "~/zephyrproject/build-cc2340-blinky/zephyr/zephyr.map", 10108, 4112, "zephyr/logs/build.txt"),
    ("zephyr_ti_peripheral", "Bluetooth peripheral, TI's Zephyr fork (builds, does not run yet)",
     "TI Zephyr fork v3.7.0-ti-1.00.01_ga (Zephyr 3.7.0 base), Zephyr kernel, GCC 12.2",
     "~/zephyr-ti/build-peripheral/zephyr/zephyr.map", 207656, 35122, "zephyr-ti/README.md"),
]
ORDER = T.ORDER
FLASH_KEYS = [c for c in ORDER if c != T.C_CCFG] + [T.C_CCFG]
RAM_KEYS = [c for c in ORDER if c != T.C_CCFG]


def gc_pool_bytes(path):
    _, inputs, _, _ = T.parse(path)
    return sum(s for a, s, lib, o, sec, os_, k in inputs if k == "obj" and lib is None and T.ZB_POOL.search(sec))


builds = []
for bid, label, sdk, rel in TI_BUILDS:
    p = os.path.join(BASE, rel)
    r = T.split(p)
    fl = {k: r["flash"].get(k, 0) for k in FLASH_KEYS}
    rm = {k: r["ram"].get(k, 0) for k in RAM_KEYS}
    fused = r["regions"]["FLASH"]["used"]; rused = r["regions"]["SRAM"]["used"]
    assert sum(v for k, v in fl.items() if k != T.C_CCFG) == fused, bid
    assert sum(rm.values()) == rused, bid
    assert r["gap_flash"] == r["holes_flash"] and r["gap_ram"] == r["holes_ram"], bid
    assert all(v >= 0 for v in list(fl.values()) + list(rm.values())), bid
    reserved = {n: v["length"] for n, v in r["regions"].items() if n in ("NVS_SLOT", "NVRAM")}
    if "NVS_SLOT" in r["regions"]:
        reserved = {"NVS_SLOT": r["regions"]["NVS_SLOT"]["length"]}
    b = dict(id=bid, label=label, sdk=sdk, source=rel, flash=fl, ram=rm,
             check=dict(
                 flash_used_in_map=fused, flash_sum_without_ccfg=sum(v for k, v in fl.items() if k != T.C_CCFG),
                 ram_used_in_map=rused, ram_sum=sum(rm.values()),
                 flash_region=dict(origin=hex(r["regions"]["FLASH"]["origin"]), length=r["regions"]["FLASH"]["length"]),
                 flash_reserved_for_settings=sum(r["regions"][n]["length"] for n in ("NVS_SLOT", "NVRAM") if n in r["regions"]),
                 other_flash_startup_tables=r["linker_generated_flash"], other_flash_padding=r["holes_flash"],
                 other_ram_padding=r["holes_ram"], unexplained_flash=0, unexplained_ram=0))
    g = gc_pool_bytes(p)
    if g:
        b["check"]["zigbee_pools_moved_from_app_to_radio_stack"] = g
    builds.append(b)

for bid, label, sdk, path, uf, ur, src_used in Z_BUILDS:
    r = G.split(os.path.expanduser(path), uf, ur)
    assert r["map_flash_end"] == uf and r["map_ram_end"] == ur, bid
    fl = {k: r["flash"].get(k, 0) for k in FLASH_KEYS}
    fl[T.C_OTHER] += r["gap_flash"]; fl[T.C_CCFG] = r["ccfg"]
    rm = {k: r["ram"].get(k, 0) for k in RAM_KEYS}
    rm[T.C_OTHER] += r["gap_ram"]
    assert sum(v for k, v in fl.items() if k != T.C_CCFG) == uf and sum(rm.values()) == ur, bid
    assert all(v >= 0 for v in list(fl.values()) + list(rm.values())), bid
    builds.append(dict(id=bid, label=label, sdk=sdk, source=path, flash=fl, ram=rm,
                       check=dict(flash_used_in_map=uf, flash_sum_without_ccfg=uf, ram_used_in_map=ur, ram_sum=ur,
                                  used_totals_from=f"map section ends, equal to the build's own report ({src_used})",
                                  other_flash_padding=r["flash"].get(T.C_OTHER, 0) + r["gap_flash"],
                                  other_ram_padding=r["ram"].get(T.C_OTHER, 0) + r["gap_ram"],
                                  unexplained_flash=0, unexplained_ram=0)))

doc = dict(
    board="LP-EM-CC2340R5", chip="CC2340R52", flash_total=524288, ram_total=36864, units="bytes",
    generated="2026-10-06",
    method=("Each object and library in the linker map was put in one component (TI maps: MODULE SUMMARY totals, "
            "placed by memory region from the SECTION ALLOCATION MAP; Zephyr GNU maps: input sections by address), "
            "with the FreeRTOS heap array ucHeap, .sysmem, .stack and RTOS task stacks moved to Heap and stacks, and "
            "linker startup tables and alignment padding counted as Other, so each build's components add up exactly "
            "to the linker's FLASH and SRAM used totals, with CCFG listed apart."),
    flash_total_note="512 KB main flash (FLASH length 0x80000 in the maps that reserve nothing); some builds reserve part of it for settings storage or a bootloader, see check.flash_region.",
    builds=builds,
)
out = os.path.join(BASE, "matrix/memory-breakdown.json")
with open(out, "w") as f:
    json.dump(doc, f, indent=2)
    f.write("\n")
print("wrote", out)

SHORT = {T.C_APP: "Your app"}
def fmt(n): return f"{n:,}"
for b in builds:
    print(f"\n### {b['label']}\n")
    print("| Component | Flash | RAM |")
    print("|---|--:|--:|")
    for k in FLASH_KEYS:
        f = b["flash"][k]; r = b["ram"].get(k)
        print(f"| {k} | {fmt(f)} | {'n/a' if r is None else fmt(r)} |")
    print(f"| **Total used** | **{fmt(b['check']['flash_sum_without_ccfg'])}** + 2,048 CCFG | **{fmt(b['check']['ram_sum'])}** |")
    print(json.dumps(b["check"]))
