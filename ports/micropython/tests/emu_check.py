#!/usr/bin/env python3
"""Start a built image in a Cortex-M0 emulator (Unicorn) and talk to its REPL.

Host check only: Unicorn runs the Arm core; the chip's peripherals are a few stubs written
here (UART0 transmit data, the system-control clock status and frequency counter, everything
else reads zero). Console input is handed to the firmware by replacing mp_hal_stdin_rx_chr,
so the UART receive interrupt is not exercised. No interrupt runs; the millisecond counter is
advanced by a hook (one tick per 2000 translated blocks), so delays end but do not take real time.

Usage: emu_check.py <g3519|g3507> <full|min|min-heap16k> [--steps N]   (the name after the board is the build folder suffix)
Needs: pip install unicorn; arm-none-eabi-nm on PATH. Exit 0 when every expected text appears.
"""
import subprocess, struct, sys, pathlib
from unicorn import UC_HOOK_BLOCK, Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE, UcError
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_LR, UC_CPU_ARM_CORTEX_M0

board, variant = sys.argv[1], sys.argv[2]
steps = int(sys.argv[sys.argv.index("--steps") + 1]) if "--steps" in sys.argv else 400_000_000
here = pathlib.Path(__file__).resolve().parent.parent
build = here / f"build-{board}-{variant}"
flash_len, ram_len = (0x80000, 0x20000) if board == "g3519" else (0x20000, 0x8000)
image = (build / "firmware.bin").read_bytes()
nm = subprocess.run(["arm-none-eabi-nm", str(build / "firmware.elf")], capture_output=True, text=True, check=True).stdout
sym = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}

UART_TX = 0x40108000 + 0x1120
SYSCTL = 0x400AF000
CLKSTATUS, FCC = SYSCTL + 0x1204, SYSCTL + 0x1150

if variant.startswith("min"):
    script = ["print(6*7)", "x = [i*i for i in range(5)]", "print(x)", "import machine",
              "p = machine.Pin('A0', machine.Pin.OUT)", "p.value(1)", "print(p)", "print(machine.freq())",
              "import time", "time.sleep_ms(3)", "print(time.ticks_ms() >= 3)", "import gc", "gc.collect()", "print('heap free', gc.mem_free())", "1//0"]
    expect = ["MicroPython v1.29.0-frl1", "42", "[0, 1, 4, 9, 16]", "Pin(PA0)", "True", "heap free", "ZeroDivisionError"]
else:
    script = ["print(6*7)", "print(1.5*3)", "import math", "print(math.sqrt(2))", "import machine", "p = machine.Pin('B22', machine.Pin.OUT)",
              "p.toggle()", "print(p)", "i2c = machine.SoftI2C(machine.Pin('A1'), machine.Pin('A2'))", "print(i2c)", "print(i2c.scan())",
              "spi = machine.SoftSPI(1000000, sck=machine.Pin('A3'), mosi=machine.Pin('A4'), miso=machine.Pin('A5'))", "spi.write(b'ab')", "print(spi.read(2))",
              "import gc", "gc.collect()", "print('heap free', gc.mem_free())", "print(machine.freq())",
              "import time", "time.sleep_ms(3)", "print(time.ticks_ms() >= 3)", "1/0"]
    expect = ["MicroPython v1.29.0-frl1", "42", "4.5", "1.41421", "Pin(PB22)", "SoftI2C(", "[]", "b'\\xff\\xff'", "heap free", "True", "ZeroDivisionError"]

feed = ("\r\n".join(script) + "\r\n").encode()
out = bytearray()
fed = 0
state = {"fcc": 0}

def mmio_read(uc, off, size, ud):
    addr = ud + off
    if addr == CLKSTATUS:
        return 0xFFFFFFFF
    if addr == FCC:
        state["fcc"] += 1
        return 1250 if state["fcc"] % 2 else 1000    # PLL count, then oscillator count: ratio 1.25
    if addr in (0x400A0000 + 0x1380, 0x400A2000 + 0x1380):
        return 0xFFFFFFFF    # GPIO data in: every pin reads high, as a bus with pull-ups and no device
    return 0

def mmio_write(uc, off, size, value, ud):
    if ud + off == UART_TX:
        out.append(value & 0xFF)

# Unicorn halts the core on WFI and nothing wakes it, so turn each WFI in the image into a NOP.
dis = subprocess.run(["arm-none-eabi-objdump", "-d", str(build / "firmware.elf")], capture_output=True, text=True, check=True).stdout
image = bytearray(image)
for line in dis.splitlines():
    parts = line.split("\t")
    if len(parts) >= 3 and parts[2].startswith("wfi"):
        a = int(parts[0].strip().rstrip(":"), 16)
        image[a:a + 2] = b"\x00\xbf"
image = bytes(image)

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M0) if hasattr(uc, "ctl_set_cpu_model") else None
uc.mem_map(0, flash_len)
uc.mem_write(0, image)
uc.mem_map(0x20200000, ram_len)
uc.mmio_map(0x40000000, 0x800000, mmio_read, 0x40000000, mmio_write, 0x40000000)
uc.mem_map(0x41C00000, 0x200000)    # factory region and configuration areas, read as zero
uc.mmio_map(0xE0000000, 0x100000, lambda *a: 0, None, lambda *a: None, None)

def on_code(uc, address, size, ud):
    global fed
    if address == (sym["mp_hal_stdin_rx_chr"] & ~1):
        if fed >= len(feed):
            uc.emu_stop()
            return
        uc.reg_write(UC_ARM_REG_R0, feed[fed])
        fed += 1
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

blocks = {"n": 0}
def on_block(uc, address, size, ud):
    # SysTick stand-in: add one millisecond to the firmware's tick counter every 2000 blocks run
    blocks["n"] += 1
    if blocks["n"] % 2000 == 0:
        t = struct.unpack("<I", uc.mem_read(sym["tick_ms"], 4))[0]
        uc.mem_write(sym["tick_ms"], struct.pack("<I", (t + 1) & 0xFFFFFFFF))

uc.hook_add(UC_HOOK_BLOCK, on_block)
uc.hook_add(UC_HOOK_CODE, on_code, begin=sym["mp_hal_stdin_rx_chr"] & ~1, end=(sym["mp_hal_stdin_rx_chr"] & ~1) + 2)
sp, pc = struct.unpack("<II", image[:8])
uc.reg_write(UC_ARM_REG_SP, sp)
try:
    uc.emu_start(pc, 0, count=steps)
except UcError as e:
    print("emulator stopped:", e, "pc=%#x" % uc.reg_read(UC_ARM_REG_PC))

if fed < len(feed):
    pcv = uc.reg_read(UC_ARM_REG_PC)
    near = max((a, n) for n, a in sym.items() if a <= pcv)
    print("blocks", blocks["n"], "tick_ms", struct.unpack("<I", uc.mem_read(sym["tick_ms"], 4))[0], "lr=%#x" % uc.reg_read(UC_ARM_REG_LR))
    print(f"stopped before all input was taken: pc={pcv:#x} in {near[1]}+{pcv - near[0]:#x}")
text = out.decode("latin-1")
print(text)
missing = [e for e in expect if e not in text]
print("---")
print(f"{board} {variant}: {len(out)} bytes of console output, {fed} of {len(feed)} input bytes taken")
print("PASS" if not missing else f"FAIL, missing: {missing}")
sys.exit(1 if missing else 0)
