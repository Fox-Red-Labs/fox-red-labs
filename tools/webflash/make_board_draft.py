#!/usr/bin/env python3
"""Write site/flash/bench/boards-draft/<board>.json from tools/webflash/boards_mspm0.py, the built manifests under
prebuilt/<board>/ and (when present) the hardware result file from tools/webflash/bench-mspm0.mjs.

  python3 tools/webflash/make_board_draft.py mspm0c1104 [--result <scratch>/mspm0c1104-result.json]
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from boards_mspm0 import BOARDS  # noqa: E402

H = lambda v: "0x%08X" % v  # noqa: E731
SDK_H = "source/ti/devices/msp/m0p/"
EXTRA = {
    "mspm0g3507": {
        "printed": "LP-MSPM0G3507",
        "ram": (0x20200000, 0x8000), "flash_kb": 128, "banks": 1, "word_bits": 64, "ecc": True, "cmdbyten": 0x1FF,
        "protect": {"has_weprota": True, "weprota_bits": 32, "weprotb_bits": 12, "weprotc_bits": 0,
                    "rule": "mspm0g350x.h: 32 WEPROTA bits and 12 WEPROTB bits. DL_FlashCTL_unprotectSector uses A for sectors 0 to 31, then one B bit per eight sectors through sector 127.",
                    "bench_writes": "Uses the existing MSPM0 sector programming path; hardware validation on the G3507 is pending."},
        "leds_note": "LED2 RGB: red PB26, green PB27, blue PB22, active high. TI gpio_toggle_output setup initializes all three pins.",
        "jumpers": ["J6 ON for red, J7 ON for green, J5 ON for blue", "J101 15:16 and 13:14 ON for the onboard debugger"],
        "differs": ["128 KB flash with ECC, CMDBYTEN 0x1FF", "32 KB SRAM at 0x20200000", "RGB LED on GPIOB", "USERID part 0xAE2D distinguishes G3507 from G3505/G3506"],
        "headers": ["mspm0g350x.h"], "sysctl_header": "hw_sysctl_mspm0g350x.h",
    },
    "mspm0c1104": {
        "printed": "LP-MSPM0C1104 (the example needs Rev E3 or later: TI's example README)",
        "ram": (0x20000000, 0x400), "flash_kb": 16, "banks": 1, "word_bits": 64, "ecc": False, "cmdbyten": 0xFF,
        "protect": {"has_weprota": True, "weprota_bits": 16, "weprotb_bits": 0, "weprotc_bits": 0,
                    "rule": "DL_FlashCTL_unprotectSector (the branch used when DEVICE_HAS_NO_CMDWEPROTA is not defined): sectors 0 to 31 of a bank are CMDWEPROTA bit = sector number; this part has 16 sectors, so A bits 0 to 15. CMDWEPROTB would be one bit per 8 sectors from sector 32 and CMDWEPROTC from sector 256; neither exists here.",
                    "bench_writes": "CMDWEPROTA = 0 and CMDWEPROTB = 0 before each command, as for the L1306 (B is not implemented here; the write was accepted without error on the board)"},
        "leds_note": "One user LED (LED1, red) on PA22, no RGB. PA2 is a test pin on header J2 pin 13 that mirrors the LED in TI's example. TI's example sets PA22 with the comment 'Default: LED is off', so the LED is active low; see the hardware results.",
        "jumpers": ["J7 must be on for the LED to light (SLAU908B Table 2-1, as quoted in mspm0c1104/FAQ.md)", "J101 11:12 and 13:14 stay ON (SWD to the XDS110, as shipped)"],
        "differs": ["flash 16 KB, not 64 KB", "one LED, no RGB, active low (pin low = lit)", "GPIOA PA22; 1 KB SRAM, so the RAM loader buffer must be sized from board data", "CMDWEPROTA covers sectors 0 to 15 (the L1306 uses A and B)"],
        "headers": ["mspm0c110x.h"], "sysctl_header": "hw_sysctl_mspm0c110x.h",
    },
    "mspm0g3519": {
        "printed": "LP-MSPM0G3519",
        "ram": (0x20200000, 0x20000), "flash_kb": 512, "banks": 2, "word_bits": 64, "ecc": True, "cmdbyten": 0x1FF,
        "protect": {"has_weprota": False, "weprota_bits": 0, "weprotb_bits": 32, "weprotc_bits": 0,
                    "rule": "DEVICE_HAS_NO_CMDWEPROTA is defined (FLASHCTL_SYS_WEPROTAWIDTH is 0). DL_FlashCTL_unprotectSector clears CMDWEPROTB bit (sectorInBank / 8) % 32 for main flash: one bit per 8 sectors (8 KB) of a bank, banks share the bit numbers. 32 bits cover 256 sectors, one 256 KB bank. CMDWEPROTA and C do not exist.",
                    "bench_writes": "CMDWEPROTA = 0 and CMDWEPROTB = 0 before each command, as for the L1306 (A is not implemented here)"},
        "leds_note": "LED2 is the RGB LED on GPIOB. PB14 is a test pin on header J2 pin 11. The board also has a separate red LED that TI's example does not drive.",
        "jumpers": ["none listed in TI's example README"],
        "differs": ["flash 512 KB in 2 banks, table sector at 0x7FC00", "CMDBYTEN 0x1FF (ECC generated), not 0xFF", "no CMDWEPROTA: CMDWEPROTB only", "LEDs on GPIOB (0x400A2000); red PB26, green PB27, blue PB22 (not the L1306 order)", "SRAM at 0x20200000, aliased at 0x20000000", "shipped image sleeps the chip: Session.start() wakes it with pulseReset; a bare reset write right after a restore can fault"],
        "headers": ["mspm0g351x.h"], "sysctl_header": "hw_sysctl_mspm0gx51x.h",
    },
    "mspm0g5187": {
        "printed": "LP-MSPM0G5187",
        "ram": (0x20200000, 0x8000), "flash_kb": 128, "banks": 2, "word_bits": 128, "ecc": True, "cmdbyten": 0x1FF,
        "protect": {"has_weprota": False, "weprota_bits": 0, "weprotb_bits": 16, "weprotc_bits": 0,
                    "rule": "DEVICE_HAS_NO_CMDWEPROTA is defined. Same rule as the G3519: CMDWEPROTB bit (sectorInBank / 8) % 32, 16 bits here. The factory region reports 2 banks of 64 KB, so 64 sectors per bank, 8 bits used. FLASHCTL_SYS_DATAWIDTH is 128 (header: 'minimum 64, maximum 128'), and DEVICE_HAS_FLASH_128_BIT_WORD is set, but TI's flashctl_multiple_size_write for this board still programs 64 bits with ECC generated (CMDBYTEN 0x1FF, one-word command).",
                    "bench_writes": "CMDWEPROTA = 0 and CMDWEPROTB = 0 before each command, as for the L1306"},
        "leds_note": "LED2 is the RGB LED, split across two ports: red PA17, green PA24, blue PB13. PA25 is a test pin on header J1 pin 2.",
        "jumpers": ["J12 [2:1] populated for red", "J13 [2:1] populated for green", "J11 [2:1] populated for blue (TI's example README)"],
        "differs": ["flash 128 KB, 128-bit flash word but TI programs 64 bits with ECC: CMDBYTEN 0x1FF", "no CMDWEPROTA", "RGB LED across two ports: red PA17 and green PA24 on GPIOA, blue PB13 on GPIOB (0x400A2000), each LED needs its jumper J12/J13/J11", "SRAM at 0x20200000, aliased at 0x20000000", "table sector 0x1FC00"],
        "headers": ["mspm0g518x.h"], "sysctl_header": "hw_sysctl_mspm0g518x.h",
    },
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board")
    ap.add_argument("--result")
    a = ap.parse_args()
    b, x = BOARDS[a.board], EXTRA[a.board]
    prebuilt = ROOT / "prebuilt" / a.board
    imgs = {}
    for d in sorted(prebuilt.iterdir()):
        m = json.loads((d / "manifest.json").read_text())
        key = "color_player" if m["recipe"] == "color-player" else "blink"
        imgs[key] = {"dir": str(d.relative_to(ROOT)), "sha256": m["image"]["sha256"], "bytes": m["image"]["bytes"],
                     "segments": m["image"]["segments"], "ends_at": H(max(int(s["addr"], 16) + s["bytes"] for s in m["image"]["segments"]))}
        if key == "color_player":
            imgs[key]["table_addr"] = m["table"]["addr"]
    leds = []
    for ch, (c) in b["channels"].items():
        if c:
            leds.append({"label": c[3], "channel": ch, "port": c[0][-1], "pin": c[1], "gpio": H(c[2]), "color": ch,
                         "rgb": len([v for v in b["channels"].values() if v]) == 3})
    d = {
        "note": "Draft written by tools/webflash/make_board_draft.py. Not yet in data/routes.json.",
        "id": a.board, "kit": b["kit"], "printed": x["printed"], "probe_serial": b["serial"],
        "identity": {"addr": "0x41C40000", "deviceid": H(b["deviceid"]), "deviceid_mask": "0x0FFFFFFF", "part": "0x%04X" % ((b["deviceid"] >> 12) & 0xFFFF)},
        "flash": {"start": "0x00000000", "end": H(b["flash_end"]), "kb": x["flash_kb"], "banks": x["banks"], "sector_bytes": 1024,
                  "word_bits": x["word_bits"], "ecc": x["ecc"], "cmdbyten": "0x%X" % x["cmdbyten"], "program_command": "one 64-bit word per command (CMDTYPE 0x01), CMDDATA0 and CMDDATA1",
                  "erase_command": "sector (CMDTYPE 0x42)", "flashctl_base": "0x400CD000", "table_sector": H(b["table"])},
        "ram": {"start": H(x["ram"][0]), "bytes": x["ram"][1], "why": "SRAM origin and length in the example's ticlang/device_linker.cmd (G3519: SRAM_BANK0 + SRAM_BANK1, 64 KB each, contiguous). The SRAM does not start at 0x20000000 on the G3519 and G5187."},
        "cmdbyten": "0x%X" % x["cmdbyten"],
        "protect": x["protect"],
        "leds": leds, "active_high": b["active_high"], "leds_note": x["leds_note"], "jumpers": x["jumpers"],
        "check_channels": [{"ch": i, "bit": c[1], "gpio": H(c[2]), "label": c[3]} for i, c in enumerate(b["channels"].values()) if c],
        "gpio_offsets": {"dout": "0x1280", "doe": "0x12C0"},
        "reset": {"cpuss_ctl": "0x40401300", "resetlevel": "0x400B0300", "resetcmd": "0x400B0304", "bootrst": 1, "go": "0xE4000001"},
        "images": imgs,
        "differs_from_l1306": x["differs"],
        "sources": [
            {"fact": "LED pins, ports, polarity, jumpers", "file": b["example"] + "/gpio_toggle_output.syscfg, ti_msp_dl_config.h, README.md, gpio_toggle_output.c"},
            {"fact": "FLASH length, SRAM", "file": b["example"] + "/ticlang/device_linker.cmd"},
            {"fact": "FLASHCTL base, GPIO bases, FLASHCTL_SYS_DATAWIDTH, WEPROT widths", "file": SDK_H + x["headers"][0]},
            {"fact": "DEVICE_HAS_NO_CMDWEPROTA, DEVICE_HAS_FLASH_128_BIT_WORD", "file": "source/ti/driverlib/dl_flashctl.h"},
            {"fact": "unprotect rule", "file": "source/ti/driverlib/dl_flashctl.c (DL_FlashCTL_unprotectSector)"},
            {"fact": "CMDBYTEN values", "file": "source/ti/driverlib/dl_flashctl.h (DL_FLASHCTL_PROGRAM_64_WITHOUT_ECC 0xFF, _WITH_ECC 0x1FF) and " + b["example"].replace("gpio_toggle_output", "flashctl_multiple_size_write") + "/flashctl_multiple_size_write.c"},
            {"fact": "FLASHCTL and GPIO register offsets", "file": "source/ti/devices/msp/peripherals/hw_flashctl.h, hw_gpio.h"},
            {"fact": "CPUSS CTL 0x1300 from base 0x40400000", "file": "source/ti/devices/msp/peripherals/m0p/hw_cpuss.h and " + SDK_H + x["headers"][0]},
            {"fact": "RESETLEVEL 0x1300 and RESETCMD 0x1304 from SYSCTL base 0x400AF000; BOOT level 1; KEY 0xE4 and GO", "file": "source/ti/devices/msp/peripherals/m0p/sysctl/" + x["sysctl_header"]},
            {"fact": "FACTORYREGION identity and SRAMFLASH (flash size, bank count)", "file": "source/ti/devices/msp/peripherals/m0p/hw_factoryregion.h"},
        ],
    }
    if "userid" in b:
        d["identity"].update(userid=H(b["userid"]), useridMask=H(b["userid_mask"]))
        d["sources"].append({"fact": "DEVICEID and USERID part identification", "url": "https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf", "section": "SLASEX6C tables 8-21 and 8-22"})
    if a.result and Path(a.result).exists():
        d["hardware"] = json.loads(Path(a.result).read_text())
    out = ROOT / "site" / "flash" / "bench" / "boards-draft" / f"{a.board}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(d, indent=1) + "\n")
    print("wrote", out.relative_to(ROOT))


if __name__ == "__main__":
    main()
