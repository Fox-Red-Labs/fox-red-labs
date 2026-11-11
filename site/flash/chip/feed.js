// Live feed for the chip view: reads a running MSPM0 through the debug port and never halts it, resets it or writes its memory.
// The same class runs in the browser (WebHID) and in Node (node-hid); only the transport differs. The chip comes from a map
// made from the SDK device header (tools/webflash/chip_maps.py, written to site/flash/chip/maps/).
const A = { PCSR: 0xE000101C, DHCSR: 0xE000EDF0, ICSR: 0xE000ED04, SYST_CSR: 0xE000E010, SYST_RVR: 0xE000E014, ISER: 0xE000E100 };
const S_SLEEP = 1 << 18, S_HALT = 1 << 17;
const VECT = { 2: "NMI", 3: "HardFault", 11: "SVCall", 14: "PendSV", 15: "SysTick" };

// Function symbols from an ELF image, sorted by address. Enough for a 32-bit little-endian Arm .out file.
export function elfFunctions(buf) {
  const d = new DataView(buf), out = [], u8 = new Uint8Array(buf);
  if (d.getUint32(0) !== 0x7F454C46) return out;
  const sh = d.getUint32(32, true), n = d.getUint16(48, true), es = d.getUint16(46, true);
  for (let i = 0; i < n; i++) {
    const h = sh + i * es;
    if (d.getUint32(h + 4, true) !== 2) continue;
    const off = d.getUint32(h + 16, true), size = d.getUint32(h + 20, true), str = sh + d.getUint32(h + 24, true) * es, so = d.getUint32(str + 16, true);
    for (let p = off; p < off + size; p += 16) {
      if ((u8[p + 12] & 15) !== 2) continue;
      let q = so + d.getUint32(p, true), s = "";
      while (u8[q]) s += String.fromCharCode(u8[q++]);
      out.push({ name: s, addr: d.getUint32(p + 4, true) & ~1, size: d.getUint32(p + 8, true) });
    }
  }
  return out.sort((a, b) => a.addr - b.addr);
}

// Pick the map whose identity matches the chip's DEVICEID.
export function matchMap(index, deviceid) { return index.find((m) => ((deviceid ^ m.deviceid) & m.mask) === 0) || null; }

export class ChipFeed {
  constructor(dap, map, now) {
    this.dap = dap; this.map = map; this.now = now; this.fns = [];
    this.ports = Object.keys(map.peripherals).filter((n) => /^GPIO[A-C]$/.test(n)).map((n) => ({ n, port: n[4], base: map.peripherals[n].base }));
    this.pinList = Object.entries(map.pins).map(([k, p]) => ({ cm: +k, ...p }));
    this.windows = []; this.keep = 60; this.faults = 0;
    this.cells = 1024; this.cellBytes = Math.max(4, map.ram.bytes / this.cells); this.ramPrev = null; this.ramChanged = new Uint8Array(this.cells);
    this.state = { chip: map.chip, kit: map.kit, board: map.board, faults: 0 };
  }
  async recover() {
    this.faults++; this.dap.select = -1; this.dap.csw = -1;
    await this.dap.dpWrite(0, 0x1E).catch(() => {}); await this.dap.dpWrite(4, 0x50000000).catch(() => {});
  }
  async read(a) { try { return (await this.dap.read32(a)) >>> 0; } catch (e) { await this.recover(); return null; } }
  // What changes rarely: which blocks have power, how each pin is set, interrupts, clock, RAM contents.
  async slow() {
    const m = this.map, st = this.state, on = {};
    for (const [n, p] of Object.entries(m.peripherals)) on[n] = !!((await this.read(p.base + 0x800)) & 1);
    st.power = on;
    const iser = await this.read(A.ISER), csr = await this.read(A.SYST_CSR), rvr = await this.read(A.SYST_RVR);
    const osc = await this.read(m.sysctl + 0x1100), mclk = await this.read(m.sysctl + 0x1104);
    if (mclk & 0x10000) st.clock = { hz: null, source: "the high-speed clock (PLL or crystal)" };
    else if (mclk & 0x100000) st.clock = { hz: 32768, source: "the 32 kHz clock" };
    else { const hz = [32e6, 4e6, null, 48e6][osc & 3]; st.clock = { hz: hz == null ? null : hz / ((mclk & 15) + 1), source: "the internal oscillator" }; }
    st.systick = { on: !!(csr & 1), irq: !!(csr & 2), reload: rvr, periodMs: csr & 1 && st.clock.hz ? ((rvr + 1) / st.clock.hz) * 1000 : null };
    st.enabled = []; for (let i = 0; i < 32; i++) if ((iser >>> i) & 1) st.enabled.push(i);
    const doe = {}; for (const p of this.ports) doe[p.port] = on[p.n] ? await this.read(p.base + 0x12C0) : 0;
    const pins = [];
    for (const p of this.pinList) {
      const v = await this.read(m.iomux + 4 * p.cm), name = p.port ? `P${p.port}${p.pin}` : `PINCM${p.cm}`;
      if (v == null) { pins.push({ cm: p.cm, name, port: p.port, pin: p.pin, connected: false, unread: true }); continue; }
      const f = v & 0x3F, conn = !!(v & 0x80), gpio = conn && f === 1, output = gpio && p.port != null && !!((doe[p.port] >>> p.pin) & 1);
      pins.push({ cm: p.cm, name, port: p.port, pin: p.pin, connected: conn, fn: conn ? (gpio ? (output ? "GPIO output" : "GPIO input") : p.fn[f] || "function " + f) : null,
        pullUp: !!(v & (1 << 17)), pullDown: !!(v & (1 << 16)), gpio, output });
    }
    st.pins = pins;
    st.detail = {};
    for (const [n, p] of Object.entries(m.peripherals)) {
      if (!on[n]) continue;
      let src = null;
      if (p.kind === "timer" || p.kind === "uart") {
        const sel = await this.read(p.base + 0x1008), div = ((await this.read(p.base + 0x1000)) & 7) + 1;
        const base = sel & 8 ? st.clock.hz : sel & 4 ? 4e6 : sel & 2 ? 32768 : null;
        src = base ? base / div : null;
      }
      if (p.kind === "timer") {
        const ctl = await this.read(p.base + 0x1804), load = await this.read(p.base + 0x1808), cps = await this.read(p.base + 0x110C), pre = cps == null ? 1 : (cps & 255) + 1;
        st.detail[n] = { running: !!(ctl & 1), load, prescale: pre, periodMs: src ? ((load + 1) * pre / src) * 1000 : null };
      }
      if (p.kind === "uart") { const ctl = await this.read(p.base + 0x1100), i = await this.read(p.base + 0x1110), f = await this.read(p.base + 0x1114); const over = [16, 8, 3][(ctl >>> 15) & 3] || 16, d = i + f / 64; st.detail[n] = { enabled: !!(ctl & 1), baud: src && d ? Math.round(src / (over * d)) : null }; }
      if (p.kind === "i2c") { const c = await this.read(p.base + 0x1228); st.detail[n] = { controller: !!(c & 1) }; }
      if (p.kind === "spi") { const c = await this.read(p.base + 0x1104); st.detail[n] = { enabled: !!(c & 1) }; }
      if (p.kind === "adc") { const c = await this.read(p.base + 0x1100); st.detail[n] = { powered: !!(c & 1) }; }
    }
    await this.readRam();
    if (!st.flashSectors) {
      const f = [];
      for (let i = 0; i < m.flash / 1024; i++) { const v = await this.read(i * 1024); f.push(v != null && v !== 0xFFFFFFFF); }
      st.flashSectors = f;
    }
    return st;
  }
  // RAM up to 4 kB is read whole; larger RAM is checked one word per 1/1024th, so a change elsewhere in that cell is not seen.
  async readRam() {
    const m = this.map; let words;
    try {
      if (this.cellBytes === 4) words = Array.from(await this.dap.readBlock(m.ram.start, this.cells), (x) => x >>> 0);
      else { words = []; for (let i = 0; i < this.cells; i++) words.push((await this.dap.read32(m.ram.start + i * this.cellBytes)) >>> 0); }
    } catch (e) { await this.recover(); return; }
    if (this.ramPrev) for (let i = 0; i < this.cells; i++) if (words[i] !== this.ramPrev[i]) this.ramChanged[i] = 1;
    this.ramPrev = words;
    const changed = this.ramChanged.reduce((a, b) => a + b, 0);
    this.state.ram = { bytes: m.ram.bytes, cellBytes: this.cellBytes, changedCells: changed, changed: this.cellBytes === 4 ? changed * 4 : null, map: Array.from(this.ramChanged) };
  }
  // What changes fast: where the CPU is, whether it sleeps, interrupts, and output levels, sampled for `ms`.
  async fast(ms) {
    const P = this.ports, addrs = [A.PCSR, A.DHCSR, A.ICSR, ...P.map((p) => p.base + 0x1280)];
    let s; try { s = await this.dap.sampleWords(addrs, ms, this.now); } catch (e) { await this.recover(); this.state.answering = false; return this.state; }
    this.state.answering = true;
    let sleep = 0, halted = 0; const pcs = new Map(), vec = {}, flips = P.map(() => new Array(32).fill(0)); let last = null;
    for (const x of s) {
      const v = x.v.map((w) => w >>> 0);
      if (v[1] & S_HALT) halted++;
      if (v[1] & S_SLEEP) sleep++; else pcs.set(v[0] & ~1, (pcs.get(v[0] & ~1) || 0) + 1);
      const a = v[2] & 0x1FF; if (a) vec[a] = (vec[a] || 0) + 1;
      if (last) P.forEach((p, k) => { const d = v[3 + k] ^ last[3 + k]; if (d) for (let b = 0; b < 32; b++) if ((d >>> b) & 1) flips[k][b]++; });
      last = v;
    }
    const n = s.length || 1, secs = s.length ? Math.max(s[s.length - 1].t, 1) / 1000 : 1;
    this.windows.push({ awake: 100 * (1 - sleep / n), pcs, vec, flips, secs, n });
    if (this.windows.length > this.keep) this.windows.shift();
    const st = this.state; st.awake = 100 * (1 - sleep / n); st.halted = halted / n > 0.5;
    st.level = {}; P.forEach((p, k) => { st.level[p.port] = last ? last[3 + k] : 0; });
    st.history = this.windows.map((w) => +w.awake.toFixed(1));
    this.summarize(); return st;
  }
  summarize() {
    const st = this.state, P = this.ports, W = this.windows.slice(-20), n = W.reduce((a, w) => a + w.n, 0) || 1, secs = W.reduce((a, w) => a + w.secs, 0) || 1;
    const pc = new Map(), vec = {}, fl = P.map(() => new Array(32).fill(0));
    for (const w of W) { for (const [k, v] of w.pcs) pc.set(k, (pc.get(k) || 0) + v); for (const k in w.vec) vec[k] = (vec[k] || 0) + w.vec[k]; w.flips.forEach((f, k) => f.forEach((v, b) => { fl[k][b] += v; })); }
    const by = new Map(); for (const [a, v] of pc) { const f = this.lookup(a); by.set(f, (by.get(f) || 0) + v); }
    st.functions = [...by].sort((a, b) => b[1] - a[1]).slice(0, 8).map(([name, v]) => ({ name, pct: +(100 * v / n).toFixed(1) }));
    st.interrupts = Object.entries(vec).map(([a, v]) => ({ vector: +a, name: VECT[a] || (a >= 16 ? this.irqName(a - 16) : "vector " + a), pct: +(100 * v / n).toFixed(1) })).sort((a, b) => b.pct - a.pct);
    st.toggles = []; P.forEach((p, k) => fl[k].forEach((v, b) => { if (v) st.toggles.push({ name: `P${p.port}${b}`, changesPerSec: +(v / secs).toFixed(1), tooFast: v > n * 0.3 }); }));
    st.sampleRate = +(n / secs).toFixed(0); st.faults = this.faults;
  }
  // With no symbols, nearby addresses are one stretch of code: named by the start of its 64-byte block.
  lookup(a) {
    if (!this.fns.length) return "code at 0x" + ((a & ~63) >>> 0).toString(16).padStart(8, "0");
    let lo = 0, hi = this.fns.length - 1, f = null;
    while (lo <= hi) { const m = (lo + hi) >> 1; if (this.fns[m].addr <= a) { f = this.fns[m]; lo = m + 1; } else hi = m - 1; }
    return f && a < f.addr + Math.max(f.size, 2) ? f.name : "0x" + a.toString(16).padStart(8, "0");
  }
  irqName(i) { return Object.entries(this.map.peripherals).filter(([, p]) => p.irq === i).map(([n]) => n).join(" or ") || "line " + i; }
}
