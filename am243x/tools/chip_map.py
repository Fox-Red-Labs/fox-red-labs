#!/usr/bin/env python3
"""Write the LP-AM243 chip map the live "Inside the chip" view reads: every block the page draws, with base addresses, interrupt lines,
the power-state register and the pads, all taken from the SDK headers and TI's SysConfig device data.

  python3 am243x/tools/chip_map.py     writes docs/site-redesign/concepts/am243-chip-map.json

Sources (nothing is invented; what a source does not give is left out):
  base addresses   source/drivers/hw_include/am64x_am243x/cslr_soc_baseaddress.h, cslr_r5fss0_baseaddress.h
  power state      csl_soc_psc.h (LPSC numbers) and cslr_psc.h: the module status of LPSC n is at PSC base + 0x800 + 4n; state 3 is enabled.
                   MAIN PSC at 0x400000, MCU PSC at 0x4000000. Blocks with no LPSC are always powered; "enable" names the bit in the block
                   itself that says the program turned it on.
  interrupt lines  cslr_intr_r5fss0_core0.h (the VIM line of R5FSS0 core 0)
  pads             /Applications/ti/sysconfig_1.27.0 AM243x_ALX_beta device data: pad control offset (add 0xF4000 for the pad register,
                   the SDK's pinmux.h agrees: PIN_GPMC0_AD7 = 0x0058), ball on the ALX package, and the signal at each mux mode.
"""
import json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SDK = Path.home() / "ti/mcu_plus_sdk_am243x_12_01_00_33/source/drivers/hw_include/am64x_am243x"
DEV = Path("/Applications/ti/sysconfig_1.27.0/dist/deviceData/AM243x_ALX_beta/AM243x_ALX_beta.json")
OUT = ROOT / "docs/site-redesign/concepts/am243-chip-map.json"
PAD_BASE = 0xF4000

def defs(path, pat):
    return {m[0]: int(m[1].rstrip("UL"), 0) for m in re.findall(pat, path.read_text())}

BASE = defs(SDK / "cslr_soc_baseaddress.h", r"#define\s+CSL_(\w+)_BASE\s+\((0x[0-9a-fA-F]+)UL\)")
BASE.update(defs(SDK / "cslr_r5fss0_baseaddress.h", r"#define\s+CSL_(\w+)_BASE\s+\((0x[0-9a-fA-F]+)UL\)"))
LPSC = defs(SDK / "csl_soc_psc.h", r"#define\s+CSL_(MAIN_LPSC_\w+|MCU_LPSC_\w+)\s+\((\d+)U\)")
IRQ = defs(SDK / "cslr_intr_r5fss0_core0.h", r"#define\s+CSLR_R5FSS0_CORE0_INTR_(\w+)\s+\((\d+)U\)")

def irqs(pat):
    return {k: v for k, v in sorted(IRQ.items(), key=lambda x: x[1]) if re.fullmatch(pat, k)}

def inst(names):
    return {n: BASE[n] for n in names if n in BASE}

def lpsc(name, mcu=False):
    k = ("MCU_LPSC_" if mcu else "MAIN_LPSC_") + name
    n = LPSC[k]
    psc = 0x4000000 if mcu else 0x400000
    return {"name": k.replace("MAIN_", "").replace("MCU_", "MCU "), "id": n, "mdstat": psc + 0x800 + 4 * n}

# page block id -> (name, kind, instance bases, interrupt pattern, LPSC, enable bit, pad interface-name pattern)
# kind uses the page's five groups: compute, memory, realtime, connect, system
B = {
 "c0": ("R5F core 0", "compute", inst(["R5FSS0_CORE0_ATCM", "R5FSS0_CORE0_BTCM"]), None, lpsc("PULSAR_0_R5_0"), None, None),
 "c1": ("R5F core 1", "compute", inst(["R5FSS0_CORE1_ATCM", "R5FSS0_CORE1_BTCM"]), None, lpsc("PULSAR_0_R5_1"), None, None),
 "c2": ("R5F core 2", "compute", inst(["R5FSS1_CORE0_ATCM", "R5FSS1_CORE0_BTCM"]), None, lpsc("PULSAR_1_R5_0"), None, None),
 "c3": ("R5F core 3", "compute", inst(["R5FSS1_CORE1_ATCM", "R5FSS1_CORE1_BTCM"]), None, lpsc("PULSAR_1_R5_1"), None, None),
 "vim": ("Interrupt controller (VIM)", "system", {"R5FSS0_VIC_CFG": BASE["R5FSS0_VIC_CFG"]}, None, None, None, None),
 "ram": ("On-chip RAM, 8 banks of 256 kB", "memory", inst([f"MSRAM_256K{i}_RAM" for i in range(6)]), None, None, None, None),
 "dm": ("Device manager", "system", {}, None, lpsc("DMSC"), None, None),
 "m4": ("Cortex-M4F", "compute", inst(["MCU_M4FSS0_IRAM", "MCU_M4FSS0_DRAM"]), None, lpsc("M4F", mcu=True), None, None),
 "pwm": ("ePWM", "realtime", inst([f"EPWM{i}_EPWM" for i in range(9)]), r"EPWM\d_EPWM_(TRIPZINT|ETINT)_\d", None, ("TBCTL", 0x00, "counter mode bits 1:0 not 3"), r"EHRPWM\d?"),
 "ecap": ("eCAP", "realtime", inst([f"ECAP{i}_CTL_STS" for i in range(3)]), r"ECAP\d_ECAP_INT_\d", None, None, r"ECAP\d"),
 "eqep": ("eQEP", "realtime", inst([f"EQEP{i}_REG" for i in range(3)]), r"EQEP\d_EQEP_INT_\d", None, None, r"EQEP\d"),
 "adc": ("ADC", "realtime", inst(["ADC0"]), None, lpsc("ADC"), None, r"ADC\d"),
 "fsi": ("FSI", "realtime", inst([f"FSIRX{i}_CFG" for i in range(6)] + [f"FSITX{i}_CFG" for i in range(2)]), r"FSI(RX|TX)\d_FSI_(RX|TX)_OINT\d_\d", None, None, r"FSI_[RT]X\d"),
 "pru0": ("PRU-ICSSG 0", "realtime", inst(["PRU_ICSSG0_PR1_CFG_SLV"]), r"PRU_ICSSG0_PR\d_HOST_INTR_PEND_\d", lpsc("ICSSG_0"), None, r"PRU_ICSSG0_.*"),
 "pru1": ("PRU-ICSSG 1", "realtime", inst(["PRU_ICSSG1_PR1_CFG_SLV"]), r"PRU_ICSSG1_PR\d_HOST_INTR_PEND_\d", lpsc("ICSSG_1"), None, r"PRU_ICSSG1_.*"),
 "cpsw": ("Ethernet switch (CPSW3G)", "connect", inst(["CPSW0_NUSS"]), None, lpsc("CPSW3G"), None, r"CPSW|MDIO\d"),
 "gpio": ("GPIO", "connect", inst(["GPIO0", "GPIO1"]), None, None, ("DIR01", 0x10, "a 0 bit is an output"), r"GPIO\d_\d+"),
 "timer": ("Timers", "system", inst([f"TIMER{i}_CFG" for i in range(12)]), r"TIMER\d+_INTR_PEND_0", None, ("TCLR", 0x38, "bit 0 set means counting"), r"DMTIMER\d+"),
 "uart": ("UART", "connect", inst([f"UART{i}" for i in range(7)]), r"UART\d_USART_IRQ_0", None, ("LCR", 0x0C, "word length bits, set by the driver"), r"USART\d"),
 "spi": ("SPI and I2C", "connect", inst([f"MCSPI{i}_CFG" for i in range(5)] + [f"I2C{i}_CFG" for i in range(4)]), r"MCSPI\d_INTR_SPI_0|I2C\d_POINTRPEND_0", None, ("I2C_CON", 0xA4, "bit 15 set means the I2C module is enabled"), r"SPI\d|INC\d"),
 "can": ("CAN (MCAN)", "connect", inst(["MCAN0_CFG", "MCAN1_CFG"]), r"MCAN\d_MCANSS_MCAN_LVL_INT_\d", lpsc("MCAN_0"), None, r"MCAN\d"),
 "usb": ("USB 2.0", "connect", inst(["USB0_MMR_MMRVBP_USBSS_CMN"]), r"USB0_IRQ_\d", lpsc("USB"), None, r"USB\d"),
 "ospi": ("Flash controller (OSPI)", "memory", inst(["FSS0_OSPI0_CTRL"]), r"FSS0_OSPI_0_OSPI_LVL_INTR_0", None, None, r"OSPI\d"),
 "mcu": ("M4F-domain I/O", "connect", inst(["MCU_UART0", "MCU_UART1", "MCU_MCSPI0_CFG", "MCU_MCSPI1_CFG", "MCU_I2C0_CFG", "MCU_I2C1_CFG", "MCU_GPIO0"]), None, None, None, r"MCU_.*"),
}

def pads():
    d = json.load(DEV.open())
    ball = {}
    for p in d["packages"].values():
        for pp in p["packagePin"]:
            ball[pp["devicePinID"]] = pp["ball"]
    name = {k: v["name"] for k, v in d["devicePins"].items()}
    out = {}
    for k, p in d["pinCommonInfos"].items():
        if not p["controlRegisterOffset"].startswith("0x"): continue  # a few non-mux pads (oscillators, resets) have no pad register
        out[name[p["devicePinID"]]] = {"ball": ball.get(p["devicePinID"]), "reg": PAD_BASE + int(p["controlRegisterOffset"], 16),
                                       "modes": {int(m["mode"]): m["signalName"] for m in p["pinModeInfo"]},
                                       "ifaces": {int(m["mode"]): m["interfaceName"] for m in p["pinModeInfo"]}}
    return out

def main():
    P = pads()
    blocks = {}
    for bid, (n, kind, bases, ipat, lp, en, ppat) in B.items():
        b = {"name": n, "kind": kind, "bases": {k: hex(v) for k, v in bases.items()}}
        if ipat: b["irq"] = irqs(ipat)
        if lp: b["lpsc"] = lp
        if en: b["enable"] = {"reg": en[0], "offset": en[1], "meaning": en[2]}
        if ppat:
            b["pads"] = {pn: {str(m): p["modes"][m] for m in p["ifaces"] if re.fullmatch(ppat, p["ifaces"][m])}
                         for pn, p in P.items() if any(re.fullmatch(ppat, i) for i in p["ifaces"].values())}
        blocks[bid] = b
    used = ["GPMC0_AD7", "UART0_RXD", "UART0_TXD", "I2C0_SCL", "I2C0_SDA"]
    out = {"chip": "AM2434 (ALX package) on the LP-AM243", "pad_register_base": hex(PAD_BASE),
           "pad_register_bits": "mode 3:0, pull disable 16, pull up 17, input enable 18, tx disable 21, slew 22",
           "psc": {"main": hex(0x400000), "mcu": hex(0x4000000), "state": "MDSTAT bits 5:0: 0 reset, 1 sync reset, 2 disabled, 3 enabled; bit 16 clock out"},
           "vim": {"base": hex(BASE["R5FSS0_VIC_CFG"]), "enable_set": "base + 0x408 + 0x20 * (line / 32), bit line % 32; core-local, not visible to the debug port's system view"},
           "blocks": blocks, "pads": {k: {"ball": P[k]["ball"], "reg": hex(P[k]["reg"]), "modes": {str(m): s for m, s in P[k]["modes"].items()}} for k in used}}
    OUT.write_text(json.dumps(out, indent=1) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)}: {len(blocks)} blocks, {sum(len(b.get('pads', {})) for b in blocks.values())} pad entries")

main()
