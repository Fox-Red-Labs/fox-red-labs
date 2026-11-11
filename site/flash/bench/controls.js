// The no-code controls (pin, timer, bus, PWM) of the Digital Lab, pure functions: the rows the page lists, the table those rows
// pack into for the board's flash, the status block the program answers with, and the line of C each row stands for.
// No I/O here; tools/webflash/controls.test.mjs runs this under node.
//
// A row is {kind, ...}: "pin" (pin, mode read|low|high), "toggle" (pin, on, off), "pwm" (pin, hz, duty), "scan", "read" (addr, reg, n).
// The program has one pin slot and four register reads, so a card holds at most one pin-slot row (pin, toggle or pwm), one scan
// and four reads; rowsToCfg says which rule a list breaks.

export const MODES = { read: 0, low: 1, high: 2, toggle: 3, pwm: 4 };
export const PIN_SLOT = ["pin", "toggle", "pwm"];
export const MAX_READS = 4;
const hex2 = (x) => "0x" + Number(x).toString(16).toUpperCase().padStart(2, "0");

// The rows of a list as the one settings object the table and the checks use. Returns {cfg, problems}.
export function rowsToCfg(rows) {
  const problems = [], cfg = { pin: null, mode: "read", on: 200, off: 300, pwmHz: 1000, pwmDuty: 50, scan: false, reads: [] };
  const slot = rows.filter((r) => PIN_SLOT.includes(r.kind));
  if (slot.length > 1) problems.push("The program drives one pin at a time: keep one of pin, timer or PWM.");
  const p = slot[0];
  if (p) {
    cfg.pin = p.pin;
    if (p.kind === "pin") cfg.mode = p.mode || "read";
    else if (p.kind === "toggle") { cfg.mode = "toggle"; cfg.on = p.on; cfg.off = p.off; }
    else { cfg.mode = "pwm"; cfg.pwmHz = p.hz; cfg.pwmDuty = p.duty; }
  }
  cfg.scan = rows.some((r) => r.kind === "scan");
  cfg.reads = rows.filter((r) => r.kind === "read").map((r) => ({ addr: r.addr, reg: r.reg, n: r.n }));
  if (cfg.reads.length > MAX_READS) problems.push(`The program holds ${MAX_READS} register reads at a time.`);
  return { cfg, problems };
}

// The 21 words of the table (prebuilt/mspm0l1306/controls/manifest.json "table"); the firmware reads them in this order.
// Also accepts the older shape {bus: none|scan|read, addr, reg, n}.
export function packTable(table, cfg) {
  const magic = Number(table.magic), clamp = (x, lo, hi) => Math.max(lo, Math.min(hi, Math.round(Number(x) || 0)));
  const hasReg = (r) => r !== null && r !== undefined && r !== "" && !Number.isNaN(Number(r));
  let scan = !!cfg.scan, reads = (cfg.reads || []).slice(0, 4);
  if (cfg.bus === "scan") scan = true;
  if (cfg.bus === "read") reads = [{ addr: cfg.addr, reg: cfg.reg, n: cfg.n }];
  const pin = cfg.pin === null || cfg.pin === undefined ? table.no_pin : clamp(cfg.pin, 0, 27);
  const mode = MODES[cfg.mode] ?? 0;
  const slot = (r, nmax) => r ? [clamp(r.addr, 8, 0x77), hasReg(r.reg) ? clamp(r.reg, 0, 255) : table.no_reg, clamp(r.n, 1, nmax)] : [0, table.no_reg, 0];
  const s0 = reads.length ? slot(reads[0], table.max_n) : [scan ? table.i2c_scan : 0, table.no_reg, 0];
  const flags = scan && reads.length ? table.flags.scan_too : 0;
  const hz = clamp(cfg.pwmHz ?? 1000, table.pwm_min_hz, table.pwm_max_hz), duty = clamp(cfg.pwmDuty ?? 50, 0, 100);
  return [magic, cfg.seq >>> 0, pin, mode, clamp(cfg.on, table.min_ms, table.max_ms), clamp(cfg.off, table.min_ms, table.max_ms),
    ...s0, hz, duty, flags, ...slot(reads[1], table.max_n_extra), ...slot(reads[2], table.max_n_extra), ...slot(reads[3], table.max_n_extra)];
}

// The status block (manifest "status"): what the program did with the table it last applied.
export function decodeStatus(words, cpuclkFallback = 32000000) {
  const w = words.map((x) => x >>> 0); while (w.length < 21) w.push(0);
  const [magic, seq, level, toggles, i2c, b0, b1, s0, s1, s2, s3, cpuclk, period, cc, div] = w;
  const le = (x, n) => Array.from({ length: n }, (_, i) => (x >>> (8 * i)) & 0xFF);
  const bytes = [...le(b0, 4), ...le(b1, 4)];
  const scan = [];
  [s0, s1, s2, s3].forEach((x, k) => { for (let i = 0; i < 32; i++) if ((x >>> i) & 1) scan.push(32 * k + i); });
  const names = ["none", "acknowledged", "no answer"];
  const clk = cpuclk || cpuclkFallback;
  const pwm = period ? { period, cc, div, hz: clk / (div * period), duty: 100 * (period - cc) / period } : null;
  const extra = [0, 1, 2].map((k) => ({ result: names[w[15 + 2 * k]] || "unknown", bytes: le(w[16 + 2 * k], 4) }));
  return { ok: magic === 0x4C545354, seq, level, toggles, i2c: names[i2c] || "unknown", bytes, scan, cpuclk, pwm, extra };
}

// The answers of the register reads in list order: slot 0 then the extra slots.
export function readAnswers(cfg, st) {
  return (cfg.reads || []).map((r, i) => i === 0 ? { result: st.i2c, bytes: st.bytes.slice(0, r.n) } : { result: st.extra[i - 1].result, bytes: st.extra[i - 1].bytes.slice(0, r.n) });
}

function pinC(row, pins) {
  const p = pins.find((x) => x.pin === row.pin), name = p ? p.name : `PA${row.pin}`, pm = `DL_GPIO_PIN_${row.pin}`, out = [];
  if (row.kind === "pin" && row.mode === "read") return [`DL_GPIO_initDigitalInput(IOMUX_PINCM${row.pin + 1});   /* ${name} as an input */`, `level = DL_GPIO_readPins(GPIOA, ${pm}) ? 1 : 0;`];
  out.push(`DL_GPIO_initDigitalOutput(IOMUX_PINCM${row.pin + 1});   /* ${name} as an output */`, `DL_GPIO_enableOutput(GPIOA, ${pm});`);
  if (row.kind === "pin") out.push(row.mode === "high" ? `DL_GPIO_setPins(GPIOA, ${pm});` : `DL_GPIO_clearPins(GPIOA, ${pm});`);
  else if (row.kind === "toggle") out.push(`while (1) {`, `    DL_GPIO_setPins(GPIOA, ${pm});   delay_cycles(${row.on}u * CYCLES_PER_MS);`, `    DL_GPIO_clearPins(GPIOA, ${pm}); delay_cycles(${row.off}u * CYCLES_PER_MS);`, `}`);
  else out.push(...pwmC(row, pins));
  return out;
}

// the timer divide and period the program picks (the same arithmetic as controls.c apply_pwm)
export function pwmSetup(hz, duty, clk = 32000000) {
  const h = Math.max(100, Math.min(20000, Math.round(hz))), d = Math.max(0, Math.min(100, Math.round(duty)));
  let div = Math.floor((Math.floor(clk / h) + 65534) / 65535); div = Math.max(1, Math.min(256, div));
  const period = Math.floor(clk / (div * h)), cc = Math.floor((period * (100 - d)) / 100);
  return { hz: h, duty: d, div, period, cc, steady: d === 0 || d === 100, actualHz: clk / (div * period) };
}
export const PWM_TIMERS = { 26: ["TIMG1", 0, "IOMUX_PINCM27_PF_TIMG1_CCP0", 0x40086000], 27: ["TIMG1", 1, "IOMUX_PINCM28_PF_TIMG1_CCP1", 0x40086000], 13: ["TIMG0", 1, "IOMUX_PINCM14_PF_TIMG0_CCP1", 0x40084000],
  12: ["TIMG0", 0, "IOMUX_PINCM13_PF_TIMG0_CCP0", 0x40084000], 3: ["TIMG2", 0, "IOMUX_PINCM4_PF_TIMG2_CCP0", 0x40088000], 6: ["TIMG0", 1, "IOMUX_PINCM7_PF_TIMG0_CCP1", 0x40084000] };
// The timer registers the page reads through the debugger (GPTIMER: CTR 0x1800, CTRCTL 0x1804, LOAD 0x1808, CC_01[cc] 0x1810 + 4 cc).
export function pwmRegs(pin) { const t = PWM_TIMERS[pin]; return t ? { base: t[3], ctr: t[3] + 0x1800, ctrctl: t[3] + 0x1804, load: t[3] + 0x1808, cc: t[3] + 0x1810 + 4 * t[1] } : null; }
// Judge the timer from what the debugger read: the load and compare registers, the enable bit, and a series of counter samples.
// Edge-aligned counting down: the pad is high while the counter is above the compare value, so the share of samples above it is the duty.
export function pwmJudge(cfg, regs, ctrSamples) {
  const s = pwmSetup(cfg.pwmHz, cfg.pwmDuty);
  const distinct = new Set(ctrSamples).size, above = ctrSamples.filter((c) => c > s.cc).length, share = ctrSamples.length ? (100 * above) / ctrSamples.length : 0;
  const regsOk = regs.load === s.period - 1 && regs.cc === s.cc && (regs.ctrctl & 1) === 1;
  const moving = distinct > Math.min(20, ctrSamples.length / 4), shareOk = Math.abs(share - s.duty) <= 8;
  return { ok: regsOk && moving && shareOk, regsOk, moving, shareOk, share, distinct, n: ctrSamples.length, setup: s };
}
function pwmC(row, pins) {
  const s = pwmSetup(row.hz, row.duty), t = PWM_TIMERS[row.pin], name = (pins.find((x) => x.pin === row.pin) || { name: `PA${row.pin}` }).name;
  if (!t) return [`/* ${name} has no timer output: PWM is not offered on it */`];
  if (s.steady) return [`/* ${s.duty}% is a steady level: the pin is held ${s.duty === 100 ? "high" : "low"}, no timer */`, s.duty === 100 ? `DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_${row.pin});` : `DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_${row.pin});`];
  return [`DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM${row.pin + 1}, ${t[2]});   /* ${name} to ${t[0]} CC${t[1]} */`,
    `clk = {DL_TIMER_CLOCK_BUSCLK, DL_TIMER_CLOCK_DIVIDE_1, ${s.div - 1}};   /* 32 MHz / ${s.div} */`,
    `DL_TimerG_setClockConfig(${t[0]}, &clk);  DL_TimerG_initPWMMode(${t[0]}, &{EDGE_ALIGN, period = ${s.period}});`,
    `DL_TimerG_setCaptureCompareValue(${t[0]}, ${s.cc}, DL_TIMER_CC_${t[1]}_INDEX);   /* ${s.hz} Hz, ${s.duty}% high */`, `DL_TimerG_startCounter(${t[0]});`];
}

function busC(row) {
  if (row.kind === "scan") return [`for (addr = 0x08; addr <= 0x77; addr++) {   /* I2C0 at 400 kHz, PA0 SDA, PA1 SCL */`,
    `    DL_I2C_startControllerTransfer(I2C0, addr, DL_I2C_CONTROLLER_DIRECTION_RX, 1);`,
    `    found[addr] = !(DL_I2C_getControllerStatus(I2C0) & DL_I2C_CONTROLLER_STATUS_ADDR_ACK);`, `}`];
  const a = hex2(row.addr), out = [];
  if (row.reg !== null && row.reg !== undefined && row.reg !== "") out.push(`reg = ${hex2(row.reg)}; DL_I2C_fillControllerTXFIFO(I2C0, &reg, 1);`, `DL_I2C_startControllerTransfer(I2C0, ${a}, DL_I2C_CONTROLLER_DIRECTION_TX, 1);`);
  out.push(`DL_I2C_startControllerTransfer(I2C0, ${a}, DL_I2C_CONTROLLER_DIRECTION_RX, ${row.n});`, `for (i = 0; i < ${row.n}; i++) bytes[i] = DL_I2C_receiveControllerData(I2C0);`);
  return out;
}

// The line or lines of C one row stands for.
export function rowC(row, pins) { return (PIN_SLOT.includes(row.kind) ? pinC(row, pins) : busC(row)).join("\n"); }

// What the reader would have typed for the whole list, in list order.
export function cLines(cfg, pins) {
  const rows = [];
  if (cfg.pin !== null && cfg.pin !== undefined) {
    if (cfg.mode === "toggle") rows.push({ kind: "toggle", pin: cfg.pin, on: cfg.on, off: cfg.off });
    else if (cfg.mode === "pwm") rows.push({ kind: "pwm", pin: cfg.pin, hz: cfg.pwmHz, duty: cfg.pwmDuty });
    else rows.push({ kind: "pin", pin: cfg.pin, mode: cfg.mode });
  }
  if (cfg.bus === "scan" || cfg.scan) rows.push({ kind: "scan" });
  if (cfg.bus === "read") rows.push({ kind: "read", addr: cfg.addr, reg: cfg.reg, n: cfg.n });
  for (const r of cfg.reads || []) rows.push({ kind: "read", ...r });
  return rows.map((r) => rowC(r, pins)).join("\n");
}

// The pin levels a toggle should show, in the shape sequenceCheck wants.
export function expectedToggle(cfg) { return [[1, Number(cfg.on)], [0, Number(cfg.off)]]; }

// Do the timer registers the program reports match what the page asked for? (the period and compare value are exact)
export function pwmMatches(cfg, st) {
  const s = pwmSetup(cfg.pwmHz, cfg.pwmDuty);
  if (s.steady) return !st.pwm;
  return !!st.pwm && st.pwm.period === s.period && st.pwm.cc === s.cc && st.pwm.div === s.div;
}

// One sentence of what the status proves, in the page's words.
export function verdict(cfg, st, pins) {
  const p = pins.find((x) => x.pin === cfg.pin), name = p ? `${p.name} (${p.role})` : `PA${cfg.pin}`;
  const parts = [];
  if (cfg.pin !== null && cfg.pin !== undefined) {
    if (cfg.mode === "read") parts.push(`${name} reads ${st.level ? "high" : "low"}`);
    else if (cfg.mode === "toggle") parts.push(`${name} toggled ${st.toggles} times so far`);
    else if (cfg.mode === "pwm") { const s = pwmSetup(cfg.pwmHz, cfg.pwmDuty); parts.push(s.steady ? `${name} is held ${s.duty === 100 ? "high" : "low"} (${s.duty}% needs no timer)` : `${name} runs PWM at ${s.actualHz.toFixed(1)} Hz, ${s.duty}% high (period ${s.period}, compare ${s.cc})`); }
    else parts.push(`${name} is driven ${cfg.mode}`);
  }
  if (cfg.bus === "scan" || cfg.scan) parts.push(st.scan.length ? `the bus answered at ${st.scan.map(hex2).join(", ")}` : "nothing answered on the bus (addresses 0x08 to 0x77)");
  const reads = cfg.bus === "read" ? [{ addr: cfg.addr, reg: cfg.reg, n: cfg.n }] : cfg.reads || [];
  const ans = cfg.bus === "read" ? [{ result: st.i2c, bytes: st.bytes.slice(0, cfg.n) }] : readAnswers(cfg, st);
  reads.forEach((r, i) => parts.push(ans[i].result === "acknowledged" ? `${hex2(r.addr)} answered: ${ans[i].bytes.map((b) => b.toString(16).toUpperCase().padStart(2, "0")).join(" ")}` : `no answer from ${hex2(r.addr)}`));
  return parts.join("; ") + ".";
}
