// A fake XDS110 with an MSPM0-like chip behind it, shaped like a WebHID HIDDevice. It answers the CMSIS-DAP commands the
// Digital Lab sends (DAP_Info, Connect, SWJ, Transfer, TransferBlock), keeps a memory map (main flash, RAM, the factory
// region, the core debug registers, the MSPM0 flash controller) and runs the page's two RAM routines by what they do
// (the flash loader and the CRC-32), so a whole connect, save, write, read-back, run and put-back can be exercised with no
// board. Faults can be injected: an unknown chip, a probe another program holds, a lost answer, an unplug part way.
//
// It is a stand-in for software tests only. A pass against it says the page's logic and messages hold together; it says
// nothing about a real board, a real USB stack or Chrome's chooser. Used by sim-xds110.test.mjs (node) and
// dlab-sim-run.mjs (headless Chrome, where this file is injected as a script, so it uses no Node API).

const LOADER_HEAD = 0x9302B083, CRC_HEAD = 0x2900B672, AT_BKPT = 0xFFFFFFF0, DEMCR = 0xE000EDFC;   // first words of LOADER and CRC_STUB in site/flash/mspm0-dap.js
const DHCSR = 0xE000EDF0, DCRSR = 0xE000EDF4, DCRDR = 0xE000EDF8, AIRCR = 0xE000ED0C;
const FC = 0x400CD000, CMDEXEC = FC + 0x1100, CMDTYPE = FC + 0x1104, CMDADDR = FC + 0x1120, CMDDATA0 = FC + 0x1130, CMDDATA1 = FC + 0x1134, STATCMD = FC + 0x13D0;
const RESETCMD = 0x400B0304;

function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) { c ^= bytes[i]; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1)); }
  return (~c) >>> 0;
}

// opts: { serial, deviceid, traceid, userid, idAddr, flashBytes, ramStart, ramBytes, led: {gpio, dout, bit, pattern: [ms...]},
//         fault: { busy, deadChip, unplugAt: {kind: "read"|"erase"|"program", after: n}, dropAnswer: n } }
export function makeFakeXds110(opts = {}) {
  const o = { serial: "SIM00001", deviceid: 0x0BB8202F, traceid: 0x5A5A1234, userid: 0, idAddr: 0x41C40000,
    flashBytes: 0x10000, ramStart: 0x20000000, ramBytes: 4096, ...opts };
  const fault = o.fault || {};
  const flash = new Uint8Array(o.flashBytes);
  // the image already on the board: a recognizable pattern, so a save and a put-back can be compared byte for byte
  for (let i = 0; i < flash.length; i++) flash[i] = i < 0x2000 ? (i * 7 + 3) & 0xFF : 0xFF;
  const original = flash.slice();
  const ram = new Uint8Array(o.ramBytes), other = new Map(), regs = new Uint32Array(20);
  const listeners = { inputreport: new Set() };
  let halted = false, running = "original", runT0 = 0, csw = 0, tar = 0, select = 0, ctrl = 0, gone = false;
  const counts = { read: 0, erase: 0, program: 0, transfers: 0 };
  const fc = { type: 0, addr: 0, d0: 0xFFFFFFFF, d1: 0xFFFFFFFF, stat: 0 };
  const now = () => (typeof performance !== "undefined" ? performance.now() : Date.now());

  const rd8 = (buf, i) => buf[i];
  function readByte(a) {
    if (a < flash.length) return rd8(flash, a);
    if (a >= o.ramStart && a < o.ramStart + ram.length) return ram[a - o.ramStart];
    return null;
  }
  function read32(a) {
    a >>>= 0;
    if (a === DHCSR) return ((halted ? 1 << 17 : 0) | (1 << 16) | (halted ? 3 : 1)) >>> 0;
    if (a === DCRDR) return regs[19];
    if (a === STATCMD) return fc.stat;
    if (a === o.idAddr) return o.traceid >>> 0;
    if (a === o.idAddr + 4) return o.deviceid >>> 0;
    if (a === o.idAddr + 8) return o.userid >>> 0;
    if (a === o.idAddr + 0x18) return ((o.ramBytes / 1024) << 16 | (o.flashBytes / 1024)) >>> 0;
    if (o.led && a === o.led.gpio + o.led.dout) return ledLevel() ? (1 << o.led.bit) >>> 0 : 0;
    if (o.led && a === o.led.gpio + (o.led.doe || 0x12C0)) return (1 << o.led.bit) >>> 0;
    const b0 = readByte(a);
    if (b0 !== null) return (readByte(a) | (readByte(a + 1) << 8) | (readByte(a + 2) << 16) | (readByte(a + 3) << 24)) >>> 0;
    return other.get(a) || 0;
  }
  function ledLevel() {   // a program that blinks the LED with the recipe's pattern once anything new was written and run
    if (halted || running !== "written" || !o.led || !o.led.pattern) return 0;
    const p = o.led.pattern, cyc = p.reduce((s, x) => s + x, 0);
    let t = (now() - runT0) % cyc;
    for (let i = 0; i < p.length; i++) { if (t < p[i]) return i % 2 === 0 ? 1 : 0; t -= p[i]; }
    return 0;
  }
  function count(kind) {
    counts[kind]++;
    const u = fault.unplugAt;
    if (u && u.kind === kind && counts[kind] >= (u.after || 1)) { unplug(); return true; }
    return false;
  }
  function write32(a, v) {
    a >>>= 0; v >>>= 0;
    if (a === DHCSR) {
      if ((v >>> 16) !== 0xA05F) return;
      const wasHalted = halted; halted = !!(v & 2);
      if (wasHalted && !halted) startCore();
      return;
    }
    if (a === DCRSR) { const n = v & 0x1F; if (v & (1 << 16)) regs[n] = regs[19]; else regs[19] = regs[n]; return; }
    if (a === DCRDR) { regs[19] = v; return; }
    if (a === AIRCR && (v >>> 16) === 0x05FA && (v & 4)) { halted = !!((other.get(DEMCR) || 0) & 1); regs[15] = 0; runT0 = now(); return; }
    if (a === RESETCMD && v === 0xE4000001) { halted = false; runT0 = now(); return; }
    if (a === CMDTYPE) { fc.type = v; return; }
    if (a === CMDADDR) { fc.addr = v; return; }
    if (a === CMDDATA0) { fc.d0 = v; return; }
    if (a === CMDDATA1) { fc.d1 = v; return; }
    if (a === CMDEXEC && v === 1) { flashCommand(); return; }
    if (a >= o.ramStart && a < o.ramStart + ram.length) { for (let i = 0; i < 4; i++) ram[a - o.ramStart + i] = (v >>> (8 * i)) & 0xFF; return; }
    other.set(a, v);
  }
  function flashCommand() {
    if (fc.type === 0x05) { fc.stat = 0; return; }
    if (fc.type === 0x42) {
      if (count("erase")) return;
      const s = fc.addr & ~0x3FF; flash.fill(0xFF, s, s + 0x400); fc.stat = 3; running = "written"; return;
    }
    if (fc.type === 0x01) {
      if (count("program")) return;
      const a = fc.addr;
      for (let i = 0; i < 4; i++) { flash[a + i] &= (fc.d0 >>> (8 * i)) & 0xFF; flash[a + 4 + i] &= (fc.d1 >>> (8 * i)) & 0xFF; }
      fc.stat = 3; running = "written"; return;
    }
    fc.stat = 1;   // done, not passed: an unknown command
  }
  // The core starts at regs[15]. The two RAM routines are recognized by their first word and done here; anything else runs.
  function startCore() {
    const pc = regs[15], head = pc === o.ramStart ? read32(o.ramStart) : 0;
    if (head === LOADER_HEAD) {
      const [dst, src, n] = [regs[0], regs[1], regs[2]];
      if (count("program")) return;
      for (let i = 0; i < n; i++) flash[dst + i] &= ram[src - o.ramStart + i];
      regs[0] = 0; regs[15] = AT_BKPT; halted = true; running = "written"; return;
    }
    if (head === CRC_HEAD) {
      const [a, n] = [regs[0], regs[1]];
      regs[0] = crc32(flash.subarray(a, a + n)); regs[15] = AT_BKPT; halted = true; return;
    }
    if (pc === AT_BKPT) { halted = true; return; }   // resumed on its breakpoint: it stops again at once
    runT0 = now();
  }

  // ---- CMSIS-DAP
  function dpRead(addr) {
    if (addr === 0x0) return 0x0BC12477;
    if (addr === 0x4) return (ctrl & 0x50000000) ? (ctrl | 0xA0000000) >>> 0 : ctrl;
    if (addr === 0xC) return 0;
    return 0;
  }
  function dpWrite(addr, v) { if (addr === 0x4) ctrl = v >>> 0; else if (addr === 0x8) select = v >>> 0; }
  function apRead(reg) {
    const apsel = select >>> 24, a = (select & 0xF0) | reg;
    if (apsel !== 0) return 0;
    if (a === 0xFC) return 0x04770031;
    if (a === 0x00) return csw;
    if (a === 0x04) return tar;
    if (a === 0x0C) { if (count("read")) return null; const v = read32(tar); if (csw & 0x10) tar = (tar + 4) >>> 0; return v; }
    return 0;
  }
  function apWrite(reg, v) {
    const apsel = select >>> 24, a = (select & 0xF0) | reg;
    if (apsel !== 0) return;
    if (a === 0x00) csw = v >>> 0; else if (a === 0x04) tar = v >>> 0;
    else if (a === 0x0C) { write32(tar, v); if (csw & 0x10) tar = (tar + 4) >>> 0; }
  }
  const le = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF];
  const u32 = (b, i) => (b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24)) >>> 0;
  function answer(b) {
    const c = b[0];
    if (c === 0x00) {
      const s = b[1] === 0x03 ? o.serial : b[1] === 0x04 ? "2.1.0" : b[1] === 0x02 ? "XDS110 (sim) with CMSIS-DAP" : "";
      const t = [...new TextEncoder().encode(s)];
      return [0x00, t.length, ...t];
    }
    if (c === 0x02) return [0x02, 0x01];
    if ([0x03, 0x04, 0x11, 0x12, 0x13].includes(c)) return [c, 0x00];
    if (c === 0x10) return [0x10, b[1]];
    if (c === 0x05) {
      const n = b[2], out = []; let i = 3, done = 0;
      if (fault.deadChip) return [0x05, 0, 7];
      for (let k = 0; k < n; k++) {
        const r = b[i++], ap = r & 1, rd = r & 2, a = r & 0x0C;
        if (rd) { const v = ap ? apRead(a) : dpRead(a); if (v === null || gone) return null; out.push(...le(v >>> 0)); }
        else { const v = u32(b, i); i += 4; if (ap) apWrite(a, v); else dpWrite(a, v); if (gone) return null; }
        done++;
      }
      return [0x05, done, 1, ...out];
    }
    if (c === 0x06) {
      const n = b[2] | (b[3] << 8), r = b[4], out = [];
      let i = 5;
      for (let k = 0; k < n; k++) {
        if (r & 2) { const v = apRead(r & 0x0C); if (v === null || gone) return null; out.push(...le(v)); }
        else { apWrite(r & 0x0C, u32(b, i)); i += 4; if (gone) return null; }
      }
      return [0x06, n & 0xFF, n >> 8, 1, ...out];
    }
    return [0xFF];
  }

  const device = {
    vendorId: 0x0451, productId: 0xBEF3, productName: "XDS110 (sim) with CMSIS-DAP",
    collections: [{ usagePage: 0xFF00, usage: 1 }], opened: false,
    async open() {
      if (gone) throw Object.assign(new Error("Failed to open the device."), { name: "NotFoundError" });
      if (fault.busy) throw Object.assign(new Error("Failed to open the device."), { name: "NotAllowedError" });
      this.opened = true;
    },
    async close() { this.opened = false; },
    addEventListener(t, f) { (listeners[t] || (listeners[t] = new Set())).add(f); },
    removeEventListener(t, f) { if (listeners[t]) listeners[t].delete(f); },
    async sendReport(id, data) {
      if (gone || !this.opened) throw Object.assign(new Error("Failed to write the report."), { name: "NotAllowedError" });
      counts.transfers++;
      if (fault.dropAnswer && counts.transfers === fault.dropAnswer) return;   // one command never answered
      const reply = answer(new Uint8Array(data.buffer || data));
      if (!reply) return;   // unplugged while answering
      const bytes = new Uint8Array(64); bytes.set(reply.slice(0, 64));
      setTimeout(() => { for (const f of [...listeners.inputreport]) f({ data: new DataView(bytes.buffer) }); }, 0);
    },
  };
  const onUnplug = new Set();
  function unplug() { if (gone) return; gone = true; device.opened = false; for (const f of onUnplug) f(device); }
  function replug() { gone = false; csw = 0; select = 0; ctrl = 0; halted = false; fault.unplugAt = null; }

  return { device, flash, original, counts, onUnplug, unplug, replug, get gone() { return gone; }, get halted() { return halted; },
    setFault(f) { Object.assign(fault, f); } };
}

// A navigator.hid stand-in around fake devices. mode: "grant" (the chooser returns the first device), "cancel" (an empty list,
// as when the person closes the chooser), or "blocked" (a policy or insecure origin: SecurityError).
export function makeFakeHid(sims, mode = "grant") {
  const ev = { connect: new Set(), disconnect: new Set() };
  const hid = {
    granted: [],
    mode,
    async requestDevice() {
      if (hid.mode === "blocked") throw Object.assign(new Error("Access denied by a policy."), { name: "SecurityError" });
      if (hid.mode === "cancel") return [];
      const d = sims.find((s) => !s.gone);
      if (!d) return [];
      if (!hid.granted.includes(d.device)) hid.granted.push(d.device);
      return [d.device];
    },
    async getDevices() { return hid.granted.filter((d) => sims.some((s) => s.device === d && !s.gone)); },
    addEventListener(t, f) { if (ev[t]) ev[t].add(f); },
    removeEventListener(t, f) { if (ev[t]) ev[t].delete(f); },
    fire(t, device) { for (const f of ev[t]) f({ device }); },
  };
  for (const s of sims) {
    s.onUnplug.add((device) => hid.fire("disconnect", device));
  }
  hid.replug = (s) => { s.replug(); hid.fire("connect", s.device); };
  return hid;
}
