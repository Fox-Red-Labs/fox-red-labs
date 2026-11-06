#!/usr/bin/env python3
"""MSPM0 DriverLib examples carried to the LP-AM13E230: SysConfig translated, app untouched, built with the
13-line shim of portability/shim/mspm0_on_am13e230/, and (with --run) flashed and checked on the board.

The AM13E2x SDK names its SysConfig modules /drivers/gpio, /drivers/uart, /drivers/systick, /drivers/sysctl where
the MSPM0 SDK says /ti/driverlib/GPIO, UART, SYSTICK, SYSCTL. This translates the MSPM0 example's .syscfg module by
module: GPIO groups keep their names and per-pin settings and get pins from a pool on the LaunchPad header
(the MSPM0 pins do not exist here); a UART goes on the backchannel pins (UC4, PA0, PA1) at the example's baud;
SysTick passes through; SYSCTL is the shim's clock block. Any other module is dropped and listed, and every line
the AM13 SysConfig rejects is commented out and listed, nothing silently. A per-example board_pins.h gives the
MSPM0 group macro (GPIO_LEDS_PORT) the AM13 generator does not emit. Rung 3 of the ladder: glue, no app edits.

  crossport_mspm0_am13.py <work dir> <results jsonl> [--run] [--only regex] [FROM board]
Run: DSLite flash (erase, verify, run), 4 s, then CFSR, HFSR and GPIO0 DOUT read; the backchannel UART captured
at the example's baud; TI's 250 ms blink written back at the end (am13e230/blink.sh image).
"""
import json, os, re, shutil, subprocess, sys, glob, tempfile, time, termios, select, threading
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "board_sweep")); import risky
M0 = "/Applications/ti/mspm0_sdk_2_11_00_07"; SDK = "/Applications/ti/am13e230x_sdk_26_01_00_03"
CGT = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
SYSCFG_PATH = "/Applications/ti/sysconfig_1.28.0"; NODE = f"{SYSCFG_PATH}/nodejs/node"
SHIM = os.path.join(ROOT, "portability", "shim", "mspm0_on_am13e230")
DSL = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
CCXML = os.path.join(ROOT, "am13e230", "AM13E23019_AL130001.ccxml"); TTY = "/dev/cu.usbmodemAL1300011"
RESTORE = os.path.abspath("out/am13e230/blink-250/blink.out")
HEADER = ['/**', ' * @cliArgs --device "AM13E230x" --part "AM13E230x" --package "LQFP64_G" --product "AM13E2X SDK@26.01.00"',
          ' * @v2CliArgs --device "AM13E23019" --package "LQFP64_G" --product "AM13E2X SDK@26.01.00"', ' */']
SYSCTL = ['const sysctl = scripting.addModule("/drivers/sysctl", {}, false);', 'sysctl.useHFCLK     = true;',
          'sysctl.SYSPLLSource = "DL_SYSCTL_SYSPLL_REF_HFCLK";', 'sysctl.SYSPLL_Qdiv  = 32;',
          'sysctl.peripheral.$suggestSolution       = "SYSCTL";', 'sysctl.peripheral.x1Pin.$suggestSolution = "PC16_X1";',
          'sysctl.peripheral.x2Pin.$suggestSolution = "PC17_X2";']
UART_FIXED = ['{u}.peripheral.$assign               = "UC4";', '{u}.peripheral.rxPin.$assign         = "PA1";', '{u}.peripheral.txPin.$assign         = "PA0";',
              '{u}.txPinConfig.$name                = "drivers_gpio_v0_gpioPinConfig{n}";', '{u}.txPinConfig.direction            = scripting.forceWrite("OUTPUT");',
              '{u}.txPinConfig.hideOutputInversion  = scripting.forceWrite(false);', '{u}.txPinConfig.onlyInternalResistor = scripting.forceWrite(false);',
              '{u}.txPinConfig.passedPeripheralType = scripting.forceWrite("Digital");', '{u}.rxPinConfig.$name                = "drivers_gpio_v0_gpioPinConfig{m}";',
              '{u}.rxPinConfig.hideOutputInversion  = scripting.forceWrite(false);', '{u}.rxPinConfig.onlyInternalResistor = scripting.forceWrite(false);',
              '{u}.rxPinConfig.passedPeripheralType = scripting.forceWrite("Digital");']
PIN_POOL = ["PA5", "PA6", "PA7", "PA8", "PA9", "PA10", "PA11", "PA12", "PA13", "PA14"]   # PA5 is LED4 (red) on the LaunchPad
GPIO_PIN_PROPS = {"direction", "internalResistor", "initialValue", "interruptEn", "polarity", "inputFilter", "hysteresisControl", "driveStrength", "ioStructure", "invert"}
UART_PROPS = {"targetBaudRate", "enableFIFO", "rxFifoThreshold", "txFifoThreshold", "enabledInterrupts", "enableInternalLoopback", "interruptPriority", "parity", "stopBits", "wordLength", "direction", "ovrsRate"}
LOWPOWER = re.compile(r"standby|shutdown|stop|sleep|lpm|wwdt", re.I)
MAX_DROPS = 40

def translate(src):
    """MSPM0 .syscfg text -> (AM13 .syscfg text, dropped modules, pin map, group names)."""
    lines = open(src).read().split("\n")
    out = list(HEADER); dropped = []; groups = {}; pins = {}; pool = list(PIN_POOL); uarts = []; n_pc = 0
    inst_mod = {}                      # instance var -> module
    for l in lines:
        m = re.match(r'\s*const\s+(\w+)\s*=\s*scripting\.addModule\("([^"]+)"', l)
        if m: inst_mod[m.group(1)] = m.group(2); continue
        m = re.match(r'\s*const\s+(\w+)\s*=\s*(\w+)\.addInstance\(', l)
        if m: inst_mod[m.group(1)] = inst_mod.get(m.group(2), "?"); continue
    mods = set(inst_mod.values())
    if "/ti/driverlib/GPIO" in mods: out.append('const gpio = scripting.addModule("/drivers/gpio", {}, false);')
    if "/ti/driverlib/UART" in mods: out.append('const uart = scripting.addModule("/drivers/uart", {}, false);')
    if "/ti/driverlib/SYSTICK" in mods: out.append('const systick = scripting.addModule("/drivers/systick");')
    if "/ti/driverlib/TIMER" in mods: out.append('const timer = scripting.addModule("/drivers/timer", {}, false);')
    for var, mod in inst_mod.items():
        if mod == "/ti/driverlib/GPIO" and var != "GPIO": out.append(f"const {var} = gpio.addInstance();")
        if mod == "/ti/driverlib/UART" and var != "UART": out.append(f"const {var} = uart.addInstance();"); uarts.append(var)
        if mod == "/ti/driverlib/TIMER" and var != "TIMER": out.append(f"const {var} = timer.addInstance();")
    for mod in sorted(mods - {"/ti/driverlib/GPIO", "/ti/driverlib/UART", "/ti/driverlib/SYSTICK", "/ti/driverlib/SYSCTL", "/ti/driverlib/Board", "/ti/project_config/ProjectConfig", "/ti/driverlib/TIMER"}):
        dropped.append(mod)
    for l in lines:
        s = l.strip()
        m = re.match(r'(\w+)\.(.+?)\s*=\s*(.+);', s)
        if not m: 
            m2 = re.match(r'(\w+)\.associatedPins\.create\((\d+)\)', s)
            if m2 and inst_mod.get(m2.group(1)) == "/ti/driverlib/GPIO": out.append(s)
            continue
        var, prop, val = m.groups(); mod = inst_mod.get(var)
        if mod == "/ti/driverlib/GPIO":
            if prop == "$name": groups[var] = val.strip('"'); out.append(s); continue
            pm = re.match(r'associatedPins\[(\d+)\]\.(.+)', prop)
            if pm:
                i, pp = pm.groups()
                if pp == "$name":
                    out.append(s); key = (var, int(i))
                    if key not in pins and pool:
                        pins[key] = pool.pop(0); out.append(f'{var}.associatedPins[{i}].pin.$assign = "{pins[key]}";')
                elif pp in GPIO_PIN_PROPS: out.append(s)
                # assignedPort, assignedPin, pin.$assign, launchPadShortcut: MSPM0 pins, replaced by the pool
            # GPIO1.port = "PORTB": MSPM0 group port, dropped
        elif mod == "/ti/driverlib/UART":
            if prop == "$name" or prop in UART_PROPS: out.append(s)
        elif mod == "/ti/driverlib/SYSTICK":
            out.append(s.replace(var + ".", "systick.", 1))
        elif mod == "/ti/driverlib/TIMER":
            if not prop.startswith("peripheral."): out.append(s)      # timer properties pass through; the fixer lists what the AM13 module rejects
    for u in uarts:
        out += [x.format(u=u, n=n_pc, m=n_pc + 1) for x in UART_FIXED]; n_pc += 2
    out += SYSCTL
    return "\n".join(out) + "\n", dropped, pins, groups

def generate(path, outdir):
    p = subprocess.run([NODE, f"{SYSCFG_PATH}/dist/cli.js", "--product", f"{SDK}/.metadata/product.json", "--output", outdir, path], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr

def fix(path, outdir):
    dropped, ok, last = [], False, ""
    for _ in range(MAX_DROPS):
        shutil.rmtree(outdir, ignore_errors=True); os.makedirs(outdir)
        rc, log = generate(path, outdir)
        nerr = re.search(r"(\d+) error\(s\)", log)
        if rc == 0 and (nerr is None or nerr.group(1) == "0") and not re.search(r"^\s*error:", log, re.M):
            ok = True; break
        m = re.search(re.escape(path) + r":(\d+):\d+", log); lines = open(path).read().split("\n"); n = None
        if m: n = int(m.group(1))
        else:
            mm = re.search(r"^\s*error: (\w+)\(/[\w/.]+\) ([\w.\[\]$]+):", log, re.M)
            if mm:
                prop = mm.group(2).split(".")[-1]
                c = [i for i, l in enumerate(lines) if re.search(r"\." + re.escape(prop) + r"\s*=", l) and not l.lstrip().startswith("//")]
                n = c[0] + 1 if c else None
        if n is None or lines[n - 1].lstrip().startswith("// PORT-DROPPED:"):
            last = next((l for l in log.splitlines() if re.match(r"\s*error:", l)), log.strip()[-200:])[:200]; break
        msg = next((l for l in log.splitlines() if re.match(r"\s*(\w*Error|error):", l)), "")[:160]
        dropped.append({"line": n, "code": lines[n - 1].strip()[:160], "why": msg.strip()})
        lines[n - 1] = "// PORT-DROPPED: " + lines[n - 1]; open(path, "w").write("\n".join(lines))
    else: last = f"gave up after {MAX_DROPS} drops"
    return ok, dropped, last

ARCH = "-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard".split()
INC = [f"-I{SDK}/source/compiler/m33_ti_arm_clang", f"-I{SDK}/source/device/am13e230x/include", f"-I{SDK}/source/device/am13e230x/include/hw",
       f"-I{SDK}/ti_sdk_config/am13e230x/default/device_support/include", f"-I{SDK}/source/arch/include", f"-I{SDK}/source/arch/m33/include",
       f"-I{SDK}/ti_sdk_config/am13e230x/default/arch_cfg", f"-I{SDK}/ti_sdk_config/am13e230x/default/arch_cfg/m33", f"-I{SDK}/source/cmsis/Core/Include",
       f"-I{SDK}/source/driverlib/am13e230x", f"-I{SDK}/source/utils/log/include", f"-I{SDK}/ti_sdk_config/am13e230x/default/utils_cfg", f"-I{SDK}/source", f"-I{SDK}/examples/empty/am13e230x_lp"]
LIBS = [f"-Wl,-i{SDK}/build/am13e230x/lib/Release", "-llibti_sdk_cfg_default_m33_ti_arm_clang.a", "-llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a",
        "-llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a", "-llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a", "-llibam13e230x_m33_ti_arm_clang.a"]

def build(d, name, srcs):
    gen, obj = f"{d}/gen", f"{d}/obj"; os.makedirs(obj, exist_ok=True); log = open(f"{d}/build.log", "w")
    for f in srcs + glob.glob(f"{gen}/*.c"):
        p = subprocess.run([f"{CGT}/bin/tiarmclang", "-O2", *ARCH, "-g", "-Wall", "-include", f"{d}/board_pins.h", f"-I{d}", f"-I{SHIM}", f"-I{gen}", *INC, "-c", f, "-o", f"{obj}/{os.path.splitext(os.path.basename(f))[0]}.o"], capture_output=True, text=True)
        log.write(p.stdout + p.stderr)
        if p.returncode: log.close(); return False, first_err(f"{d}/build.log")
    p = subprocess.run([f"{CGT}/bin/tiarmclang", *ARCH, *glob.glob(f"{obj}/*.o"), *LIBS, f"{SDK}/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd",
                        "-Wl,--rom_model", "-Wl,--reread_libs", "-Wl,--diag_suppress=10063", f"-Wl,-m,{d}/{name}.map", f"-L{CGT}/lib", "-llibc.a", "-o", f"{d}/{name}.out"], capture_output=True, text=True)
    log.write(p.stdout + p.stderr); log.close(); shutil.rmtree(obj, ignore_errors=True)
    return (p.returncode == 0 and os.path.exists(f"{d}/{name}.out")), (None if p.returncode == 0 else first_err(f"{d}/build.log"))

def first_err(log):
    for l in open(log, errors="ignore"):
        if "error:" in l or "undefined symbol" in l: return re.sub(r"^.*error: ", "", l.strip())[:200]
    return "see build.log"

class Uart(threading.Thread):
    def __init__(self, baud): super().__init__(daemon=True); self.buf, self.stop, self.baud = b"", False, baud
    def run(self):
        try:
            fd = os.open(TTY, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK); a = termios.tcgetattr(fd)
            a[4] = a[5] = getattr(termios, f"B{self.baud}", termios.B115200); a[2] |= termios.CLOCAL | termios.CREAD; a[3] = a[0] = a[1] = 0
            termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
        except OSError: return
        while not self.stop:
            if select.select([fd], [], [], 0.1)[0]:
                try: self.buf += os.read(fd, 512)
                except OSError: break
        os.close(fd)

def read_words(addr, n):
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        q = subprocess.run([DSL, "memory", "-c", CCXML, "-r", f"{addr:#x},{n}", "-s", str(4 * n), "-o", f.name], capture_output=True, text=True, timeout=60)
        if "-260" in q.stdout + q.stderr: raise RuntimeError("-260")
        b = open(f.name, "rb").read() if q.returncode == 0 else b""
    return [int.from_bytes(b[i:i + 4], "little") for i in range(0, len(b) - 3, 4)]

def run_on_board(out, baud, secs=4.0):
    u = Uart(baud); u.start(); time.sleep(0.3); t0 = time.time()
    p = subprocess.run([DSL, "flash", "-c", CCXML, "-e", "-f", "-v", "-r", "1", "-u", "-t", "60", out], capture_output=True, text=True, timeout=120)
    rec = dict(flash_s=round(time.time() - t0, 1), flashed="verification successful" in (p.stdout + p.stderr))
    if "-260" in p.stdout + p.stderr: rec["stop"] = "-260"; u.stop = True; return rec
    if rec["flashed"]:
        time.sleep(secs)
        try:
            w = read_words(0xE000ED28, 2); rec.update(cfsr=hex(w[0]), hfsr=hex(w[1]), clean=(w[0] == 0 and w[1] == 0))
            ds = []
            for _ in range(4): ds.append(read_words(0x400F1280, 2)[0]); time.sleep(0.2)   # GPIO0 DOUT (PA0 to PA31), four reads
            rec.update(gpio0_dout=[hex(x) for x in ds], gpio0_changed=hex(ds[0] ^ ds[1] | ds[1] ^ ds[2] | ds[2] ^ ds[3]))
        except Exception as e: rec.update(clean=False, read=str(e)[:80])
    else: rec.update(clean=False, flash_tail=(p.stdout + p.stderr)[-300:])
    u.stop = True; time.sleep(0.1); rec["uart"] = u.buf.decode("latin1")[:300]
    return rec

def main():
    W, OUT = sys.argv[1], sys.argv[2]; do_run = "--run" in sys.argv
    only = re.compile(sys.argv[sys.argv.index("--only") + 1]) if "--only" in sys.argv else None
    FROM = next((a for a in sys.argv[3:] if a.startswith("LP_")), "LP_MSPM0G3519")
    inv = [json.loads(l) for l in open(os.path.join(HERE, "data", "inventory.jsonl"))]
    ok_mods = {"/ti/driverlib/GPIO", "/ti/driverlib/UART", "/ti/driverlib/SYSCTL", "/ti/driverlib/Board", "/ti/project_config/ProjectConfig", "/ti/driverlib/SYSTICK", "/ti/driverlib/TIMER"}
    cands = [r for r in inv if r["board"] == FROM and r["category"] == "driverlib" and r["modules"] and set(r["modules"]) <= ok_mods]
    done = {json.loads(l)["src"] for l in open(OUT) if l.strip()} if os.path.exists(OUT) else set()
    res = open(OUT, "a"); ran = 0
    try:
        for r in cands:
            e = r["example"]; src = f"{M0}/examples/nortos/{FROM}/driverlib/{e}"
            if only and not only.search(e): continue
            row = dict(src=f"{FROM}:driverlib/{e}", dst="LP_AM13E230:driverlib/" + e, mode="syscfg-translate+shim", date="2026-10-10")
            if row["src"] in done and not do_run: continue
            d = f"{W}/am13/{e}"; shutil.rmtree(d, ignore_errors=True); os.makedirs(d)
            sc = glob.glob(f"{src}/*.syscfg")
            if not sc: row.update(ok=False, first_error="no .syscfg"); res.write(json.dumps(row) + "\n"); continue
            text, dropped_mods, pins, groups = translate(sc[0]); open(f"{d}/example.syscfg", "w").write(text)
            row.update(dropped_modules=dropped_mods, pins={f"{v}[{i}]": p for (v, i), p in pins.items()})
            ok, dropped, last = fix(f"{d}/example.syscfg", f"{d}/gen"); row["dropped_lines"] = dropped
            if not ok: row.update(ok=False, first_error="syscfg: " + last); res.write(json.dumps(row) + "\n"); res.flush(); print("SYSCFG FAIL", e, last[:90]); continue
            hdr = open(f"{d}/gen/ti_sdk_dl_config.h", errors="ignore").read() if os.path.exists(f"{d}/gen/ti_sdk_dl_config.h") else ""
            bp = ["/* generated: MSPM0 group port macros from the AM13 per-pin macros, and the interrupt aliases the MSPM0",
                  "   generator emits and the AM13 one does not. All pool pins are on GPIO0 (port A); MSPM0 GPIO interrupts arrive",
                  "   through group 1, the AM13's through GPIO0_INT, so the group query answers with the GPIO index. */",
                  "#define DL_INTERRUPT_GROUP_1 1", "#define DL_Interrupt_getPendingGroup(g) (g)", "#define GROUP1_IRQHandler GPIO0_IRQHandler"]
            for g in groups.values():
                m = re.search(rf"#define\s+({re.escape(g)}_\w+?_PORT)\s", hdr)
                if m: bp.append(f"#ifndef {g}_PORT\n#define {g}_PORT {m.group(1)}\n#endif")
                bp += [f"#define {g}_INT_IRQN GPIO0_INT_IRQn", f"#define {g}_INT_IIDX 1"]
            if "enabledInterrupts" in text:   # AM13 SysConfig 26.01 writes the MSPM0 list's bare word (RX) into its C; the enum exists under the shared name
                bp.append("#define RX DL_UART_INTERRUPT_RX")
            for tn in re.findall(r'^\s*\w+\.\$name\s*=\s*"(TIMER_\w+)"', text, re.M):
                m = re.search(rf"#define\s+{re.escape(tn)}_INST\s+\(?(\w+?)(_INST_PTR)?\)?\s", hdr)
                if m and not re.search(rf"#define\s+{re.escape(tn)}_INST_INT_IRQN", hdr):
                    base = m.group(1) + "_0"      # the AM13 device header names every timer interrupt TIMGn_0_INT_IRQn (hw_ints.h); the generator emits no alias
                    bp += [f"#define {tn}_INST_INT_IRQN {base}_INT_IRQn", f"#define {tn}_INST_IRQHandler {base}_IRQHandler"]
            for un in re.findall(r'^\s*\w+\.\$name\s*=\s*"(UART_\w+)"', text, re.M):
                bp += [f"#define {un}_INST_INT_IRQN UC4_INT_IRQn", f"#define {un}_INST_IRQHandler UC4_IRQHandler"]
            open(f"{d}/board_pins.h", "w").write("\n".join(bp) + "\n")
            # copies, not the SDK files: a quoted include looks beside the source first, and TI ships a pre-generated
            # MSPM0 ti_msp_dl_config.h there that would shadow the shim's stand-in
            srcs = []
            for f in glob.glob(f"{src}/*.c") + glob.glob(f"{src}/*.cpp") + glob.glob(f"{src}/*.h"):
                if os.path.basename(f).startswith("ti_msp_dl_config"): continue
                shutil.copy(f, d)
                if not f.endswith(".h"): srcs.append(f"{d}/{os.path.basename(f)}")
            bok, err = build(d, e, srcs); row.update(ok=bok, first_error=err)
            if bok:
                row["out"] = f"{d}/{e}.out"
                sz = subprocess.run([f"{CGT}/bin/tiarmsize", row["out"]], capture_output=True, text=True).stdout.strip().splitlines()[-1].split(); row["text"] = int(sz[0])
            print("ok  " if bok else "FAIL", e, row.get("text", ""), f"dropped_mods={dropped_mods} dropped_lines={len(dropped)}", (err or "")[:80])
            if bok and do_run:
                rs = risky.classify(src, e, "driverlib")
                if rs or LOWPOWER.search(e) or e.startswith("flashctl") or e in ("sram_parity", "cinit_bypass"):
                    row["run"] = {"skipped": "risky, low-power, flash-writing or memory-fault example"}
                else:
                    baud = 115200
                    m = re.search(r"targetBaudRate\s*=\s*(\d+)", open(sc[0]).read()); baud = int(m.group(1)) if m else 115200
                    row["run"] = run_on_board(row["out"], baud); ran += 1
                    print("   ran" if row["run"].get("clean") else "   RUN-FAIL", row["run"])
                    if row["run"].get("stop") == "-260": res.write(json.dumps(row) + "\n"); print("STOP: probe dropped"); break
            res.write(json.dumps(row) + "\n"); res.flush()
    finally:
        res.close()
        if do_run and ran:
            p = subprocess.run([DSL, "flash", "-c", CCXML, "-e", "-f", "-v", "-r", "1", "-u", "-t", "60", RESTORE], capture_output=True, text=True, timeout=120)
            print("restore 250 ms blink:", "ok" if "verification successful" in p.stdout + p.stderr else "FAILED " + (p.stdout + p.stderr)[-200:])

if __name__ == "__main__":
    main()
