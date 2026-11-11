// Flash and check an LP-MSPM0L1306 LaunchPad kit from a web page, through the board's own XDS110 debugger.
//
// The XDS110 offers a standard CMSIS-DAP v1 interface over USB HID ("XDS110 CMSIS-DAP", interface 5), which
// Chrome and Edge open with WebHID and no driver. This file speaks CMSIS-DAP to the chip's debug port, and
// programs main flash with the MSPM0 flash controller's own commands (register names and values from TI's
// MSPM0 SDK 2.11.00.07: hw_flashctl.h and DriverLib's DL_FlashCTL_* sequence, as TI's L1306 flash examples
// use them). It never writes NONMAIN (the boot configuration) and never erases all of flash.
//
// The same code runs under Node for testing (tools/webflash/), with a transport that has the same shape.

import { familyOf } from "./families/index.js";

export const ERR = {
  NO_DEVICE: "no-device", WRONG_CHIP: "wrong-chip", CONNECT: "connect", FLASH: "flash", VERIFY: "verify",
  IMAGE: "image", CHECK: "check",
};

export class FlashError extends Error {
  constructor(kind, message, next) { super(message); this.kind = kind; this.next = next || ""; }
}

// ---- transports ---------------------------------------------------------------------------------------

// WebHID: one 64-byte report out, one in, per command.
export class WebHidTransport {
  constructor(device) { this.device = device; this.queue = []; this.waiters = []; }
  async open() {
    if (!this.device.opened) await this.device.open();
    // One listener per transport, removed on close: the page makes a new transport for each session on the same device,
    // and a listener left behind kept queueing every reply for a transport nobody reads any more.
    if (!this.onReport) {
      this.onReport = (e) => {
        const b = new Uint8Array(e.data.buffer, e.data.byteOffset, e.data.byteLength);
        const w = this.waiters.shift();
        if (w) w(new Uint8Array(b)); else this.queue.push(new Uint8Array(b));
      };
      this.device.addEventListener("inputreport", this.onReport);
    }
  }
  async close() {
    if (this.onReport) { this.device.removeEventListener("inputreport", this.onReport); this.onReport = null; }
    this.queue = []; this.waiters = [];
    try { await this.device.close(); } catch (e) { /* already closed */ }
  }
  async transfer(bytes) {
    const out = new Uint8Array(64);
    out.set(bytes);
    // CMSIS-DAP answers each command once, in order: a report already waiting when a new command goes out is a late answer to a
    // command that timed out, and taking it would shift every later answer by one. Drop it.
    this.queue.length = 0;
    let waiter;
    const reply = new Promise((res, rej) => {
      const t = setTimeout(() => {
        // a waiter left in the list after its timeout swallowed the next command's answer, so every command after one
        // lost packet timed out too (found 2026-10-24 with the fake XDS110, tools/webflash/sim-xds110.test.mjs)
        const i = this.waiters.indexOf(waiter); if (i >= 0) this.waiters.splice(i, 1);
        rej(new FlashError(ERR.CONNECT, "the debugger stopped answering", "unplug the board, plug it back in, and try again"));
      }, this.timeoutMs || 3000);
      waiter = (b) => { clearTimeout(t); res(b); };
      this.waiters.push(waiter);
    });
    try { await this.device.sendReport(0, out); }
    catch (e) {
      const i = this.waiters.indexOf(waiter); if (i >= 0) this.waiters.splice(i, 1);
      reply.catch(() => {});
      throw new FlashError(ERR.CONNECT, "the USB link to the debugger was lost", "plug the board back in, press Connect a board, then use Put back what was there if a write was interrupted");
    }
    return reply;
  }
}

// WebUSB: the XDS110's vendor interface 2 also speaks CMSIS-DAP (DAP_Info "XDS110 with CMSIS-DAP", 1.2.0, read
// 2026-10-14), over bulk endpoints OUT 2 and IN 3 (0x83), in the probe's default configuration. Chrome on Android
// offers WebUSB but not WebHID, so this is the phone route; it works on desktop Chrome too.
export class WebUsbTransport {
  constructor(device, iface = 2, outEp = 2, inEp = 3) { this.device = device; this.iface = iface; this.outEp = outEp; this.inEp = inEp; }
  async open() {
    if (!this.device.opened) await this.device.open();
    if (this.device.configuration === null) await this.device.selectConfiguration(1);
    await this.device.claimInterface(this.iface);
  }
  async close() {
    try { await this.device.releaseInterface(this.iface); } catch (e) { /* not claimed */ }
    try { await this.device.close(); } catch (e) { /* already closed */ }
  }
  async transfer(bytes) {
    const out = new Uint8Array(64);
    out.set(bytes);
    await this.device.transferOut(this.outEp, out);
    // A reply left over from an earlier, interrupted session can be waiting on the bulk endpoint: skip replies
    // that do not answer this command (seen 2026-10-14 after a run was stopped part way).
    for (let i = 0; i < 8; i++) {
      const r = await this.device.transferIn(this.inEp, 64);
      if (r.status !== "ok" || !r.data) break;
      const b = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
      if (b[0] === out[0]) return b;
    }
    throw new FlashError(ERR.CONNECT, "the debugger stopped answering", "unplug the board, plug it back in, and try again");
  }
}

// ---- CMSIS-DAP ----------------------------------------------------------------------------------------

const DP = { IDR: 0x0, ABORT: 0x0, CTRL: 0x4, SELECT: 0x8, RDBUFF: 0xC };
const AP = { CSW: 0x00, TAR: 0x04, DRW: 0x0C, IDR: 0xFC };
const req = (ap, read, addr) => (ap ? 1 : 0) | (read ? 2 : 0) | (addr & 0x0C);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const put32 = (a, v) => a.push(v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF);
const hex = (v) => "0x" + (v >>> 0).toString(16).toUpperCase().padStart(8, "0");

export class Dap {
  constructor(transport, log) { this.t = transport; this.log = log || (() => {}); this.select = -1; this.csw = -1; }

  async cmd(bytes, what) {
    const r = await this.t.transfer(bytes);
    if (!r.length) throw new FlashError(ERR.CONNECT, "the USB link to the debugger was lost", "plug the board back in, press Connect a board, then use Put back what was there if a write was interrupted");
    if (r[0] !== bytes[0]) throw new FlashError(ERR.CONNECT, `the debugger did not understand ${what}`, "update the XDS110 firmware with TI's xdsdfu, or use the terminal route");
    return r;
  }

  async info(id) {
    const r = await this.cmd([0x00, id], "DAP_Info");
    return new TextDecoder().decode(r.slice(2, 2 + r[1])).replace(/\0+$/, "");
  }

  async connect() {
    await this.cmd([0x11, ...le32(1000000)], "DAP_SWJ_Clock");             // 1 MHz
    const c = await this.cmd([0x02, 0x01], "DAP_Connect");
    if (c[1] !== 1) throw new FlashError(ERR.CONNECT, "the debugger would not start SWD", "unplug the board, plug it back in, and try again");
    await this.cmd([0x04, 0x00, 0x50, 0x00, 0x00, 0x00], "DAP_TransferConfigure"); // 80 wait retries
    await this.cmd([0x13, 0x00], "DAP_SWD_Configure");
    if (this.dormant) {
      // CC23xx and CC27xx start their debug port dormant (ack 7 to the switch below): Arm's dormant-to-SWD wake,
      // 8 high clocks, the 128-bit selection alert, 4 low clocks, the SWD activation code 0x1A, then a line reset
      await this.cmd([0x12, 8, 0xFF], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 128, 0x92, 0xF3, 0x09, 0x62, 0x95, 0x2D, 0x85, 0x86, 0xE9, 0xAF, 0xDD, 0xE3, 0xA2, 0x0E, 0xBC, 0x19], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 4, 0x00], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 8, 0x1A], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 51, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 8, 0x00], "DAP_SWJ_Sequence");
    } else {
      // line reset, JTAG-to-SWD switch, line reset, idle
      await this.cmd([0x12, 51, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 16, 0x9E, 0xE7], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 51, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07], "DAP_SWJ_Sequence");
      await this.cmd([0x12, 8, 0x00], "DAP_SWJ_Sequence");
    }
    const idr = await this.dpRead(DP.IDR);
    await this.dpWrite(DP.ABORT, 0x1E);
    await this.dpWrite(DP.SELECT, 0); this.select = 0;
    await this.dpWrite(DP.CTRL, 0x50000000);                                // CSYSPWRUPREQ | CDBGPWRUPREQ
    for (let i = 0; i < 100; i++) {
      if ((((await this.dpRead(DP.CTRL)) & 0xA0000000) >>> 0) === 0xA0000000) return idr;
    }
    throw new FlashError(ERR.CONNECT, "the chip's debug port did not power up", "the program on the board may be in a deep sleep: see the recovery steps below");
  }

  async disconnect() { try { await this.cmd([0x03], "DAP_Disconnect"); } catch (e) { /* ignore */ } }

  // Pulse the board's reset line (nRESET, DAP_SWJ_Pins bit 7), with the debugger's power request set as the chip
  // comes out of reset, so a program that puts the chip into a deep sleep cannot shut the debugger out. It restarts
  // the program on the board; it writes nothing. Seen 2026-10-14: TI's serial example locked out both this page and
  // OpenOCD until this.
  async pulseReset() {
    await this.cmd([0x10, 0x00, 0x80, 0, 0, 0, 0], "DAP_SWJ_Pins");
    await new Promise((r) => setTimeout(r, 50));
    try { await this.connect(); } catch (e) { /* a chip held in reset may not answer yet */ }
    await this.cmd([0x10, 0x80, 0x80, 0, 0, 0, 0], "DAP_SWJ_Pins");
    await new Promise((r) => setTimeout(r, 50));
    this.select = -1; this.csw = -1;
    return this.connect();
  }

  async transfer(list) {   // list of {r, v}; returns read values
    const out = [0x05, 0x00, list.length];
    for (const x of list) { out.push(x.r); if (!(x.r & 2)) put32(out, x.v); }
    const r = await this.cmd(out, "DAP_Transfer");
    if (r[1] !== list.length || r[2] !== 1) {
      const ack = r[2] & 7;
      await this.cmd([0x05, 0x00, 0x01, req(false, false, DP.ABORT), 0x1E, 0, 0, 0], "DAP_Transfer").catch(() => {});
      throw new FlashError(ERR.CONNECT, `the chip did not answer the debugger (ack ${ack}, ${r[1]} of ${list.length})`,
        "unplug the board, plug it back in, and try again");
    }
    const vals = [];
    for (let i = 0; i < r[1] && 3 + 4 * i + 3 < r.length; i++) vals.push(u32(r, 3 + 4 * i));
    return vals;
  }

  async dpRead(a) { return (await this.transfer([{ r: req(false, true, a) }]))[0]; }
  async dpWrite(a, v) { await this.transfer([{ r: req(false, false, a), v }]); }

  async apSelect(apsel, reg) {
    const s = ((apsel & 0xFF) << 24) | (reg & 0xF0);
    if (s !== this.select) { await this.dpWrite(DP.SELECT, s); this.select = s; }
  }
  async apRead(apsel, reg) { await this.apSelect(apsel, reg); return (await this.transfer([{ r: req(true, true, reg) }]))[0]; }
  async apWrite(apsel, reg, v) { await this.apSelect(apsel, reg); await this.transfer([{ r: req(true, false, reg), v }]); }

  async setCsw(v) { if (v !== this.csw) { await this.apWrite(0, AP.CSW, v); this.csw = v; } }

  async read32(addr) {
    await this.setCsw(0x23000002);                                           // 32-bit, no auto-increment
    await this.apSelect(0, 0);
    const v = await this.transfer([{ r: req(true, false, AP.TAR), v: addr }, { r: req(true, true, AP.DRW) }]);
    return v[0];
  }
  async write32(addr, val) {
    await this.setCsw(0x23000002);
    await this.apSelect(0, 0);
    await this.transfer([{ r: req(true, false, AP.TAR), v: addr }, { r: req(true, false, AP.DRW), v: val }]);
  }
  async writeMany(pairs) {   // [[addr, val], ...] in as few packets as fit
    await this.setCsw(0x23000002);
    await this.apSelect(0, 0);
    for (let i = 0; i < pairs.length; i += 6) {
      const list = [];
      for (const [a, v] of pairs.slice(i, i + 6)) list.push({ r: req(true, false, AP.TAR), v: a }, { r: req(true, false, AP.DRW), v });
      await this.transfer(list);
    }
  }
  async readBlock(addr, words) {   // auto-increment, restarted at each 1 KB boundary
    await this.setCsw(0x23000012);
    await this.apSelect(0, 0);
    const out = [];
    while (out.length < words) {
      const a = addr + 4 * out.length;
      // One DAP_Transfer carries the address write and up to 14 data reads: one USB round trip for 56 bytes (the address write and a
      // DAP_TransferBlock of 15 words took two; a flash read-back of 1 MB took 80 s).
      const n = Math.min(words - out.length, 14, (0x400 - (a & 0x3FF)) / 4);
      const list = [{ r: req(true, false, AP.TAR), v: a }];
      for (let i = 0; i < n; i++) list.push({ r: req(true, true, AP.DRW) });
      const vals = await this.transfer(list);
      if (vals.length < n) throw new FlashError(ERR.CONNECT, `block read stopped at ${hex(a)}`, "unplug the board, plug it back in, and try again");
      for (let i = 0; i < n; i++) out.push(vals[i]);   // transfer() lists one value per request; the address write has none, so the first n are the data
    }
    await this.setCsw(0x23000002);
    return out;
  }
  // Several addresses read together, in one packet each round: [{t: ms, v: [values]}]
  async sampleWords(addrs, ms, now) {
    await this.setCsw(0x23000002);
    await this.apSelect(0, 0);
    const per = Math.floor(12 / (2 * addrs.length)) || 1, list = [];
    for (let k = 0; k < per; k++) for (const a of addrs) list.push({ r: req(true, false, AP.TAR), v: a }, { r: req(true, true, AP.DRW) });
    const out = [], t0 = now();
    while (now() - t0 < ms) {
      const vals = await this.transfer(list);
      const t = now() - t0;
      for (let k = 0; k < per; k++) out.push({ t, v: vals.slice(k * addrs.length, (k + 1) * addrs.length) });
    }
    return out;
  }

  async writeBlock(addr, words) {   // auto-increment, restarted at each 1 KB boundary; 14 words a packet
    await this.setCsw(0x23000012);
    await this.apSelect(0, 0);
    let i = 0;
    while (i < words.length) {
      const a = addr + 4 * i;
      const k = Math.min(words.length - i, 14, (0x400 - (a & 0x3FF)) / 4);
      await this.transfer([{ r: req(true, false, AP.TAR), v: a }]);
      const out = [0x06, 0x00, k & 0xFF, k >> 8, req(true, false, AP.DRW)];
      for (let j = 0; j < k; j++) put32(out, words[i + j]);
      const r = await this.cmd(out, "DAP_TransferBlock");
      if ((r[1] | (r[2] << 8)) !== k || r[3] !== 1) throw new FlashError(ERR.CONNECT, `block write stopped at ${hex(a)}`, "unplug the board, plug it back in, and try again");
      i += k;
    }
    await this.setCsw(0x23000002);
  }
  // n reads of one address in one packet each round: [{t: ms, v}]
  async sampleWord(addr, ms, now) {
    await this.setCsw(0x23000002);
    await this.apSelect(0, 0);
    await this.transfer([{ r: req(true, false, AP.TAR), v: addr }]);
    const out = [], t0 = now(), list = Array(12).fill({ r: req(true, true, AP.DRW) });
    while (now() - t0 < ms) {
      const vals = await this.transfer(list);
      const t = now() - t0;
      for (const v of vals) out.push({ t, v });
    }
    return out;
  }
}

function le32(v) { const a = []; put32(a, v); return a; }

// ---- MSPM0 --------------------------------------------------------------------------------------------

const DHCSR = 0xE000EDF0, DEMCR = 0xE000EDFC, AIRCR = 0xE000ED0C;
const CPUSS_CTL = 0x40401300;   // prefetch, instruction cache, literal cache (hw_cpuss.h)
const RESETLEVEL = 0x400B0300, RESETCMD = 0x400B0304, BOOTRST = 1, RESET_GO = 0xE4000001;   // hw_sysctl_mspm0l11xx_l13xx.h
const FC = 0x400CD000, CMDEXEC = FC + 0x1100, CMDTYPE = FC + 0x1104, CMDADDR = FC + 0x1120, CMDBYTEN = FC + 0x1124,
  CMDDATA0 = FC + 0x1130, CMDDATA1 = FC + 0x1134, WEPROTA = FC + 0x11D0, WEPROTB = FC + 0x11D4, STATCMD = FC + 0x13D0;
const PROGRAM_ONEWORD = 0x01, ERASE_SECTOR = 0x42, CLEARSTATUS = 0x05, BYTEN_64 = 0xFF;   // hw_flashctl.h, dl_flashctl.h
const SECTOR = 1024;
const DCRSR = 0xE000EDF4, DCRDR = 0xE000EDF8;
// The RAM flash loader (tools/webflash/loader/mspm0_loader.c, built by build.sh): r0 flash address, r1 data in
// SRAM, r2 bytes, r3 CMDBYTEN; it stops at a breakpoint with r0 = 0, or the address that failed.
const LOADER = "83b00293009072b60192002a26d0154c0026164f05227a6001223a6023685b07fcd400231048036043607a6002987862009835183d6208683863486878633a602268d007fcd0900705d508360831019886421d46ded3284600befde70025284600befde7d0e30c40d0e10c4000e10c40";

// CRC-32 of a memory range, run from RAM (tools/webflash/loader/crc32.c, built by build_crc.sh): r0 address, r1 bytes, r0 = crc.
const CRC_STUB = "72b6002916d00022d3430c4a1c460378634008245d0801261e40734213406b40641ef7d1491e401c0029efd1d943084600befde70021084600befde72083b8ed";
export function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) { c ^= bytes[i]; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1)); }
  return (~c) >>> 0;
}
const pause = (ms) => new Promise((r) => setTimeout(r, ms));
export const RETRIES = 3, RETRY_MS = 150;

// Shared by discovery and the pre-write identity check. Some MSPM0 family IDs cover several flash sizes.
export function matchesIdentity(id, deviceid, userid = null) {
  if (deviceid == null || ((deviceid & Number(id.deviceidMask)) >>> 0) !== ((Number(id.deviceid) & Number(id.deviceidMask)) >>> 0)) return false;
  return id.userid === undefined || (userid != null && ((userid & Number(id.useridMask)) >>> 0) === ((Number(id.userid) & Number(id.useridMask)) >>> 0));
}

export class Mspm0 {
  constructor(dap, board, log) {
    this.dap = dap; this.board = board; this.log = log || (() => {});
    this.fast = board.fast !== false && !!board.ram && board.ramStart !== undefined;   // no RAM data, no loader: one word at a time
    this.byten = board.cmdbyten || BYTEN_64;                         // 0x1FF on parts that program with ECC
    // The flash controller and what differs on the Cortex-M33 parts (LP-MSPM33C321A, LP-AM13E230; research in
    // docs/research/webflash-m33-boards-2026-10-14.md): its address, 2 kB sectors, 16-byte program words, a flash
    // semaphore taken before any flash register answers, write protection put back by hand afterwards, other reset
    // registers, no CPUSS cache switch, and on the MSPM33 an image linked at the secure alias of main flash.
    const fc = board.fc || {};
    const base = fc.base !== undefined ? fc.base : FC;
    this.r = { CMDEXEC: base + 0x1100, CMDTYPE: base + 0x1104, CMDADDR: base + 0x1120, CMDBYTEN: base + 0x1124, CMDDATAINDEX: base + 0x112C,
      CMDDATA0: base + 0x1130, CMDDATA1: base + 0x1134, CMDDATA2: base + 0x1138, CMDDATA3: base + 0x113C,
      WEPROTA: base + 0x11D0, WEPROTB: base + 0x11D4, STATCMD: base + 0x13D0 };
    this.sector = fc.sector || SECTOR;
    this.word = fc.word || 8;
    this.sem = fc.semaphore || null;          // {request, status, clear, held}
    this.atRest = fc.protectAtRest || null;   // {a, b}: written back before the semaphore is released
    this.fam = familyOf(board);
    // the M33 parts' cache register is in their family module (data says cpuss null there: no CPUSS)
    this.cpuss = this.fam.id === "mspm33" ? this.fam.cacheReg : (board.cpuss === null ? null : (this.fam.cacheReg || CPUSS_CTL));
    this.rst = board.reset || { level: RESETLEVEL, cmd: RESETCMD };
    this.alias = board.alias || 0;            // 0x10000000 on the MSPM33: same bytes as 0
    if (this.word !== 8 || base !== FC) this.fast = false;   // the MSPM0 RAM loader is built for the MSPM0 controller
    if (this.fam.loader && this.fam.ramStart !== undefined && board.fast !== false) this.fast = true;   // the M33 family has its own
  }

  // Segments linked at the flash alias, moved to the flash controller's addresses.
  norm(segments) { return segments.map((s) => (this.alias && s.addr >= this.alias && s.addr < this.alias + this.board.flash.end ? { addr: s.addr - this.alias, data: s.data } : s)); }

  async takeSemaphore() {
    if (!this.sem || this.semHeld) return;
    await this.dap.write32(this.sem.request, 1);
    for (let i = 0; i < 50; i++) {
      const st = await this.dap.read32(this.sem.status);
      if ((st >>> 0) === (this.sem.held >>> 0)) { this.semHeld = true; return; }
    }
    throw new FlashError(ERR.FLASH, "the chip's flash semaphore was not granted to the debugger", "unplug the board, plug it back in, and try again");
  }
  async giveSemaphore() {
    if (!this.sem || !this.semHeld) return;
    // the protection restore reports its failure (Codex review 2026-10-16); the semaphore is released either way
    try { if (this.atRest) await this.dap.writeMany([[this.r.WEPROTA, this.atRest.a], [this.r.WEPROTB, this.atRest.b]]); }
    finally { await this.dap.write32(this.sem.clear, 1).catch(() => {}); this.semHeld = false; }
  }

  // Program one flash word: 8 bytes (MSPM0) or 16 bytes (MSPM33, AM13) at d[o], to addr.
  async programWord(addr, d, o) {
    if (this.word === 16) {
      return this.command(PROGRAM_ONEWORD, addr, [[this.r.CMDBYTEN, this.byten], [this.r.CMDDATAINDEX, (addr >>> 4) & 3],
        [this.r.CMDDATA0, u32(d, o)], [this.r.CMDDATA1, u32(d, o + 4)], [this.r.CMDDATA2, u32(d, o + 8)], [this.r.CMDDATA3, u32(d, o + 12)]]);
    }
    return this.command(PROGRAM_ONEWORD, addr, [[this.r.CMDBYTEN, this.byten], [this.r.CMDDATA0, u32(d, o)], [this.r.CMDDATA1, u32(d, o + 4)]]);
  }

  async identify() {
    const id = this.board.identity;
    // A program in a deep sleep can fault the first reads while the debugger's power request wakes the chip
    // (seen 2026-10-14 after TI's serial example): try for about a second before giving up.
    let w;
    for (let tries = 0; ; tries++) {
      try {
        w = [];
        for (let i = 0; i < 3; i++) w.push(await this.dap.read32(id.addr + 4 * i));
        break;
      } catch (e) {
        if (tries >= 10 || !(e instanceof FlashError) || e.kind !== ERR.CONNECT) throw e;
        await new Promise((r) => setTimeout(r, 100));
        this.dap.select = -1; this.dap.csw = -1;
        await this.dap.dpWrite(DP.ABORT, 0x1E).catch(() => {});
        await this.dap.dpWrite(DP.CTRL, 0x50000000).catch(() => {});
      }
    }
    const got = { traceid: hex(w[0]), deviceid: hex(w[1]), userid: hex(w[2]) };
    if (!matchesIdentity(id, w[1], w[2])) {
      throw new FlashError(ERR.WRONG_CHIP, `this chip reports DEVICEID ${got.deviceid} and USERID ${got.userid}, which do not match the ${this.board.kit}`,
        `nothing was written. If other boards are plugged in, press the button again and pick another XDS110; this page only flashes the ${this.board.kit}`);
    }
    return got;
  }

  // Stop the core before any write. First a core reset caught at its first instruction (DEMCR VC_CORERESET, then
  // SYSRESETREQ), so the board's program has not run and cannot have put the chip into a deep sleep; a core halted
  // inside such a sleep left SRAM unreachable (seen 2026-10-14 after TI's serial example). Plain halt if that fails.
  // A board fresh from the box runs TI's out-of-box image, which sleeps the chip: a debugger access then answers "ack 4". Wake it
  // with the nRESET pulse (power request set as it leaves reset) and halt again; a family with a power wrapper catches the core instead.
  async halt() {
    try { return await this.halt1(); } catch (e) {
      if (!(e instanceof FlashError) || e.kind !== ERR.CONNECT) throw e;
      this.log("asleep: waking the chip");
      this.dap.select = -1; this.dap.csw = -1;
      if (this.fam.safeConnect) { await this.dap.connect(); await this.fam.safeConnect(this.dap); } else await this.dap.pulseReset();
      return this.halt1();
    }
  }
  async halt1() {
    // With the first flash word blank (no program to start) a reset leaves the boot ROM keeping the debugger out
    // (the board's FAQ, and the MSPM0G3519 on 2026-10-15), and no program can have slept the chip: plain halt only.
    // Safe connect (families with a power wrapper): TI's wait-for-debug holds the core in its reset handler, so the
    // program on the chip never runs again: no hard fault from a half-written image, no stale caches, no deep sleep.
    this.held = false;
    if (this.fam.safeConnect) {
      try { this.held = !!(await this.fam.safeConnect(this.dap)); } catch (e) {
        this.log("safe connect failed: " + (e.message || e));
        this.dap.select = -1; this.dap.csw = -1; await this.dap.connect().catch(() => {});
      }
    }
    const blank = this.held || (await this.dap.read32(this.board.flash.start).catch(() => 0)) === 0xFFFFFFFF;
    if (!blank) try {
      await this.dap.write32(DHCSR, 0xA05F0001);
      await this.dap.write32(DEMCR, 1);
      await this.dap.write32(AIRCR, 0x05FA0004).catch(() => {});
      await new Promise((r) => setTimeout(r, 20));
      for (let i = 0; i < 50; i++) {
        let d = await this.dap.read32(DHCSR).catch(() => null);
        if (d === null) {
          // the core reset dropped the debug link (seen on the MSPM0G3519, 2026-10-15): power the debug port up again
          this.dap.select = -1; this.dap.csw = -1;
          await this.dap.connect().catch(() => {});
          d = await this.dap.read32(DHCSR).catch(() => 0);
        }
        if (d & (1 << 17)) { await this.dap.write32(DEMCR, 0); break; }
      }
    } catch (e) { /* fall through to a plain halt */ }
    this.dap.select = -1; this.dap.csw = -1;
    await this.dap.write32(DHCSR, 0xA05F0003);
    for (let i = 0; i < 50; i++) {
      if ((await this.dap.read32(DHCSR)) & (1 << 17)) {
        // Caching off, so reading the flash back after a write shows the flash, not a line cached before it.
        // Seen 2026-10-14: with it on, one word read back as the old image after a correct write. A system
        // reset request (SYSRESETREQ) does not restore this register (the next program ran 1.74 times
        // slower), so resetRun() writes the saved value back and then does a boot reset.
        if (this.cpuss) {
          if (this.cpussCtl === undefined) this.cpussCtl = await this.dap.read32(this.cpuss);
          await this.dap.write32(this.cpuss, 0);
        }
        await this.takeSemaphore();
        this.halted = true;
        return;
      }
    }
    throw new FlashError(ERR.CONNECT, "the processor did not stop for the debugger", "unplug the board, plug it back in, and try again");
  }

  async waitCmd(what, addr) {
    for (let i = 0; i < 2000; i++) {
      const s = await this.dap.read32(this.r.STATCMD);
      if (s & 1) {
        if (s & 2) return;
        throw new FlashError(ERR.FLASH, `the flash controller reported a failed ${what} at ${hex(addr)} (this.r.STATCMD ${hex(s)})`,
          "the board was not left half-written on purpose: run the flash again; if it fails twice, use the terminal route");
      }
    }
    throw new FlashError(ERR.FLASH, `the flash controller did not finish the ${what} at ${hex(addr)}`, "run the flash again");
  }

  // Each failed block is tried again up to three times with a 150 ms pause before it is reported (a flash command that
  // came back "not passed", a debugger packet that got no answer). A dropped link is re-opened between tries.
  async retry(fn) {
    for (let i = 1; ; i++) {
      try { return await fn(); } catch (e) {
        if (!(e instanceof FlashError) || (e.kind !== ERR.FLASH && e.kind !== ERR.CONNECT) || i >= RETRIES) throw e;
        this.retries = (this.retries || 0) + 1;
        await pause(RETRY_MS);
        this.dap.select = -1; this.dap.csw = -1;
        await this.dap.dpWrite(DP.ABORT, 0x1E).catch(() => {});
      }
    }
  }
  readBlock(addr, words) { return this.retry(() => this.dap.readBlock(addr, words)); }

  async command(type, addr, extra) {
    if (addr < this.board.flash.start || addr >= this.board.flash.end) {
      throw new FlashError(ERR.IMAGE, `refused to touch ${hex(addr)}, outside main flash`, "this image is not for this board");
    }
    await this.retry(() => this.command1(type, addr, extra));
  }
  async command1(type, addr, extra) {
    await this.dap.writeMany([[this.r.CMDTYPE, CLEARSTATUS], [this.r.CMDEXEC, 1]]);
    await this.dap.writeMany([[this.r.WEPROTA, 0], [this.r.WEPROTB, 0], [this.r.CMDTYPE, type], [this.r.CMDADDR, addr], ...(extra || []), [this.r.CMDEXEC, 1]]);
    await this.waitCmd(type === ERASE_SECTOR ? "sector erase" : "program", addr);
  }

  async coreReg(n, v) {
    await this.dap.write32(DCRDR, v);
    await this.dap.write32(DCRSR, (1 << 16) | n);
    for (let i = 0; i < 50; i++) if ((await this.dap.read32(DHCSR)) & (1 << 16)) return;
    throw new FlashError(ERR.CONNECT, "the processor did not take a register write", "unplug the board, plug it back in, and try again");
  }
  async readCoreReg(n) {
    await this.dap.write32(DCRSR, n);
    for (let i = 0; i < 50; i++) if ((await this.dap.read32(DHCSR)) & (1 << 16)) return this.dap.read32(DCRDR);
    throw new FlashError(ERR.CONNECT, "the processor did not give a register", "unplug the board, plug it back in, and try again");
  }

  // Program bytes (a multiple of 8) through the RAM loader: one block of data per run of the loader instead
  // of several debugger round trips per 8 bytes. The caller has halted the core and erased the sectors.
  // Run RAM code and return when it reaches its breakpoint. The code goes in at the family's RAM start, the parameter
  // block at +0x300 (r0 points to it), the stack top is the block, buffers start at +0x400.
  async runRam(hexCode, params) {
    const RAM = this.fam.ramStart, PB = RAM + 0x300;
    if (this.ramCode !== hexCode) {
      const code = hexCode.match(/../g).map((h) => parseInt(h, 16));
      while (code.length % 4) code.push(0);
      const cw = []; for (let i = 0; i < code.length; i += 4) cw.push(u32(code, i));
      await this.dap.writeBlock(RAM, cw); this.ramCode = hexCode;
    }
    await this.dap.writeBlock(PB, params);
    await this.coreReg(0, PB); await this.coreReg(13, PB); await this.coreReg(15, RAM); await this.coreReg(16, 0x01000000);
    await this.dap.write32(DHCSR, 0xA05F0001);
    for (let i = 0; i < 6000; i++) {
      if ((await this.dap.read32(DHCSR)) & (1 << 17)) return this.dap.readBlock(PB, params.length);
    }
    await this.dap.write32(DHCSR, 0xA05F0003);
    this.ramCode = null;
    throw new FlashError(ERR.FLASH, "the flash loader did not finish", "run the flash again");
  }

  // Cortex-M33 family: whole buffers per run of the RAM loader (16 kB at a time) instead of one word per debug round trip.
  async programLoader(addr, bytes, progress) {
    const RAM = this.fam.ramStart, buf = RAM + 0x400, chunk = 16384;
    for (let o = 0; o < bytes.length; o += chunk) {
      const part = bytes.subarray(o, Math.min(bytes.length, o + chunk));
      if (part.every((x) => x === 0xFF)) continue;                    // erased already
      const w = []; for (let i = 0; i < part.length; i += 4) w.push(u32(part, i));
      await this.retry(async () => {
        await this.dap.writeBlock(buf, w);
        const r = await this.runRam(this.fam.loader, [addr + o, buf, part.length, this.byten, this.fam.coreFc, 0]);
        if (r[5] !== 0) throw new FlashError(ERR.FLASH, `the flash controller reported a failed program at ${hex(r[5])}`, "run the flash again; if it fails twice, use the terminal route");
      });
      if (progress) progress(`writing ${Math.round(100 * Math.min(bytes.length, o + chunk) / bytes.length)}%`);
    }
  }

  // CRC-32 of every sector of [start, start + count * sector) computed on the chip; one word per sector comes back.
  async sectorCrcs(start, count) {
    const RAM = this.fam.ramStart, out = RAM + 0x400, all = [];
    for (let i = 0; i < count; i += 256) {
      const n = Math.min(256, count - i);
      await this.runRam(this.fam.crcSectors, [start + i * this.sector, n, this.sector, out]);
      all.push(...(await this.dap.readBlock(out, n)));
    }
    return all.map((v) => v >>> 0);
  }

  async programFast(addr, bytes, progress) {
    if (this.fam.loader) return this.programLoader(addr, bytes, progress);
    const p = progress || (() => {});
    const ramSize = this.board.ram, RAM = this.board.ramStart;
    if (!ramSize || RAM === undefined) throw new FlashError(ERR.IMAGE, "this board's RAM start and size are not in its data, so the fast loader will not run", "add \"ram\" and \"ramStart\" to the board's data");
    // RAM does not start at the same address on every part (0x20200000 on the MSPM0G3519 and G5187).
    // Seen 2026-10-14: a 4 KB default on the 1 KB MSPM0C1104 ran the buffer past the end of RAM after sector 0
    // was erased, which locked the debugger out until a mass erase.
    const code = LOADER.match(/../g).map((h) => parseInt(h, 16));
    while (code.length % 4) code.push(0);
    const cw = []; for (let i = 0; i < code.length; i += 4) cw.push(u32(code, i));
    const bufAt = RAM + 0x80, buf = Math.min(1024, (ramSize - 0x80 - 0x40) & ~7);   // 64 bytes spare at the top
    if (buf < 64) throw new FlashError(ERR.IMAGE, `${ramSize} bytes of RAM is too little for the fast loader`, "");
    await this.dap.writeBlock(RAM, cw);
    for (let o = 0; o < bytes.length; o += buf) {
      const chunk = bytes.subarray(o, Math.min(bytes.length, o + buf));
      const w = []; for (let i = 0; i < chunk.length; i += 4) w.push(u32(chunk, i));
      await this.dap.writeBlock(bufAt, w);
      await this.coreReg(0, addr + o); await this.coreReg(1, bufAt); await this.coreReg(2, chunk.length); await this.coreReg(3, this.byten);
      await this.coreReg(13, bufAt); await this.coreReg(15, RAM); await this.coreReg(16, 0x01000000);
      await this.dap.write32(DHCSR, 0xA05F0001);                              // run, debug still enabled
      let halted = false;
      for (let i = 0; i < 400 && !halted; i++) halted = !!((await this.dap.read32(DHCSR)) & (1 << 17));
      if (!halted) { await this.dap.write32(DHCSR, 0xA05F0003); throw new FlashError(ERR.FLASH, `the flash loader did not finish the block at ${hex(addr + o)}`, "run the flash again"); }
      const r = await this.readCoreReg(0);
      if (r !== 0) throw new FlashError(ERR.FLASH, `the flash controller reported a failed program at ${hex(r)}`, "run the flash again; if it fails twice, use the terminal route");
      p(`writing ${Math.round(100 * Math.min(bytes.length, o + buf) / bytes.length)}%`);
    }
  }

  async flash(segments, progress) {
    segments = this.norm(segments);
    // Every segment, padded to whole words, inside main flash before anything is stopped or erased (Codex review 2026-10-16)
    const { start, end } = this.board.flash;
    for (const s of segments) {
      const padded = Math.ceil(s.data.length / this.word) * this.word;
      if (s.addr < start || s.addr % this.word || s.addr + padded > end) {
        throw new FlashError(ERR.IMAGE, `the image has data at ${hex(s.addr)} (${s.data.length} bytes), outside this board's main flash`, "nothing was written: this image is not for this board");
      }
    }
    const p = progress || (() => {});
    await this.halt();
    let total = 0, done = 0;
    for (const s of segments) total += s.data.length;
    const sectors = new Set();
    for (const s of segments) for (let a = s.addr & ~(this.sector - 1); a < s.addr + s.data.length; a += this.sector) sectors.add(a);
    for (const a of [...sectors].sort((x, y) => x - y)) { p(`erasing ${hex(a)}`); await this.command(ERASE_SECTOR, a); }
    for (const s of segments) {
      const d = new Uint8Array(Math.ceil(s.data.length / this.word) * this.word).fill(0xFF);
      d.set(s.data);
      if (this.fast) {
        try { await this.programFast(s.addr, d, p); continue; } catch (e) {
          // A core stopped inside the boot ROM (flash blank at its start, MSPM0G3519 2026-10-15) did not run the
          // loader to its breakpoint: write this image one word at a time instead.
          if (!(e instanceof FlashError) || !/loader did not finish/.test(e.message)) throw e;
          this.fast = false;
          await this.dap.write32(DHCSR, 0xA05F0003);
          for (const a of [...new Set([s.addr & ~(this.sector - 1)])]) await this.command(ERASE_SECTOR, a);
          for (let a = (s.addr & ~(this.sector - 1)) + this.sector; a < s.addr + d.length; a += this.sector) await this.command(ERASE_SECTOR, a);
        }
      }
      for (let o = 0; o < d.length; o += this.word) {
        await this.programWord(s.addr + o, d, o);
        done += this.word;
        if (o % 256 === 0) p(`writing ${Math.min(100, Math.round(100 * done / total))}%`);
      }
    }
  }

  // Run a small routine from RAM and return r0 at its breakpoint (the page's loader mechanism, as programFast uses).
  async runStub(hexCode, regs) {
    const RAM = this.fam.ramStart !== undefined ? this.fam.ramStart : this.board.ramStart;
    if (RAM === undefined) throw new FlashError(ERR.IMAGE, "no RAM address for the on-chip check", "");
    const code = hexCode.match(/../g).map((h) => parseInt(h, 16));
    while (code.length % 4) code.push(0);
    const cw = []; for (let i = 0; i < code.length; i += 4) cw.push(u32(code, i));
    await this.dap.writeBlock(RAM, cw);
    for (const [i, v] of regs.entries()) await this.coreReg(i, v >>> 0);
    await this.coreReg(13, RAM + 0x400); await this.coreReg(15, RAM); await this.coreReg(16, 0x01000000);
    await this.dap.write32(DHCSR, 0xA05F0001);
    for (let i = 0; i < 4000; i++) {
      if ((await this.dap.read32(DHCSR)) & (1 << 17)) return this.readCoreReg(0);
    }
    await this.dap.write32(DHCSR, 0xA05F0003);
    throw new FlashError(ERR.FLASH, "the on-chip checksum routine did not finish", "the flash is read back instead");
  }
  // CRC-32 of flash on the chip: nothing crosses USB but the answer.
  crcOnChip(addr, bytes) { return this.runStub(CRC_STUB, [addr, bytes]); }

  // "full" reads every word back over USB; the default checks each segment with the chip's own CRC-32 and reads back
  // only to name the first wrong word when the checksums differ (or the routine cannot run).
  async verify(segments, opts = {}) {
    for (const s of this.norm(segments)) {
      const words = Math.ceil(s.data.length / 4);
      if (!opts.full && this.fam.crc && s.data.length >= 256) {
        try {
          const want = new Uint8Array(words * 4).fill(0xFF); want.set(s.data);
          if (((await this.crcOnChip(s.addr, want.length)) >>> 0) === crc32(want)) { this.crcChecked = (this.crcChecked || 0) + 1; continue; }
        } catch (e) { if (!(e instanceof FlashError)) throw e; await this.dap.write32(DHCSR, 0xA05F0003).catch(() => {}); }
      }
      const got = await this.readBlock(s.addr, words);
      const want = new Uint8Array(words * 4).fill(0xFF);
      want.set(s.data);
      for (let i = 0; i < words; i++) {
        if (got[i] !== u32(want, 4 * i)) {
          throw new FlashError(ERR.VERIFY, `flash at ${hex(s.addr + 4 * i)} reads ${hex(got[i])}, the image has ${hex(u32(want, 4 * i))}`,
            "run the flash again; if it fails twice, use the terminal route");
        }
      }
    }
  }

  // Rewrite one flash sector with these 32-bit words (the colour player's table), then read it back.
  async writeSector(addr, words) {
    if (addr % this.sector) throw new FlashError(ERR.IMAGE, `the table address ${hex(addr)} is not at the start of a sector`, "");
    const bytes = new Uint8Array(Math.ceil(words.length * 4 / this.word) * this.word).fill(0xFF);
    words.forEach((w, i) => { bytes[4 * i] = w & 0xFF; bytes[4 * i + 1] = (w >>> 8) & 0xFF; bytes[4 * i + 2] = (w >>> 16) & 0xFF; bytes[4 * i + 3] = w >>> 24; });
    if (bytes.length > this.sector) throw new FlashError(ERR.IMAGE, "the table is bigger than one sector", "use fewer steps");
    await this.halt();
    await this.command(ERASE_SECTOR, addr);
    for (let o = 0; o < bytes.length; o += this.word) await this.programWord(addr + o, bytes, o);
    await this.verify([{ addr, data: bytes }]);
  }

  // Is this image already on the board? (Its loaded bytes compared; nothing written.)
  async holds(segments) {
    try { await this.verify(segments); return true; } catch (e) { if (e.kind === ERR.VERIFY) return false; throw e; }
  }

  // All of main flash, as it is now.
  async readAll(progress) {
    const p = progress || (() => {});
    const { start, end } = this.board.flash, out = new Uint8Array(end - start);
    for (let a = start; a < end; a += this.sector) {
      const w = await this.readBlock(a, this.sector / 4);
      w.forEach((v, i) => { out[a - start + 4 * i] = v & 0xFF; out[a - start + 4 * i + 1] = (v >>> 8) & 0xFF; out[a - start + 4 * i + 2] = (v >>> 16) & 0xFF; out[a - start + 4 * i + 3] = v >>> 24; });
      p(`reading ${Math.round(100 * (a + this.sector - start) / (end - start))}%`);
    }
    return out;
  }

  // Put a saved copy of main flash back: every sector erased, every word that is not blank programmed, then read back.
  async restoreAll(bytes, progress) {
    const p = progress || (() => {});
    const { start, end } = this.board.flash;
    if (bytes.length !== end - start) throw new FlashError(ERR.IMAGE, `the saved copy is ${bytes.length} bytes; main flash is ${end - start}`, "choose the file this page saved");
    await this.halt();
    // Only the sectors that differ from the copy are erased and written: fewer writes, and on large parts the
    // sectors (and banks) nothing changed are never touched.
    const changed = [];
    let crcs = null;
    if (this.fam.crcSectors) {
      try { p("comparing with the chip's own checksums"); crcs = await this.sectorCrcs(start, bytes.length / this.sector); } catch (e) { if (!(e instanceof FlashError)) throw e; crcs = null; await this.dap.write32(DHCSR, 0xA05F0003).catch(() => {}); }
    }
    if (crcs) {
      for (let k = 0; k < crcs.length; k++) if (crcs[k] !== crc32(bytes.subarray(k * this.sector, (k + 1) * this.sector))) changed.push(k * this.sector);
    } else for (let o = 0; o < bytes.length; o += this.sector) {
      const want = bytes.subarray(o, o + this.sector), got = await this.readBlock(start + o, this.sector / 4);
      if (got.some((v, i) => v !== u32(want, 4 * i))) changed.push(o);
      if (o % (16 * this.sector) === 0) p(`comparing ${Math.round(100 * o / bytes.length)}%`);
    }
    // The sector with the vector table goes last: a restore cut off part way leaves the old table, not a blank one.
    if (changed[0] === 0) changed.push(changed.shift());
    if (this.fast) {
      for (const [k, o] of changed.entries()) {
        await this.command(ERASE_SECTOR, start + o);
        const blk = bytes.subarray(o, o + this.sector);
        if (blk.some((x) => x !== 0xFF)) await this.programFast(start + o, blk, () => {});
        p(`writing ${Math.round(100 * (k + 1) / changed.length)}% (${changed.length} of ${bytes.length / this.sector} sectors differed)`);
      }
      for (const o of changed) await this.verify([{ addr: start + o, data: bytes.subarray(o, o + this.sector) }], { full: true });
      return;
    }
    // Slow path: only the sectors that differ, vector table last (a cut-off restore can be run again: it compares first).
    for (const [k, o] of changed.entries()) {
      await this.command(ERASE_SECTOR, start + o);
      for (let q = o; q < o + this.sector; q += this.word) if (bytes.subarray(q, q + this.word).some((x) => x !== 0xFF)) await this.programWord(start + q, bytes, q);
      p(`writing ${Math.round(100 * (k + 1) / changed.length)}% (${changed.length} of ${bytes.length / this.sector} sectors differed)`);
    }
    await this.verify([{ addr: start, data: bytes }], { full: true });
  }

  // Leave the chip as we found it on every exit path, errors included: caches back, semaphore and protection put back,
  // the hold bit of the wait-for-debug connect cleared, and the core released with a reset if this page had stopped it.
  async release() {
    if (!this.halted && !this.held) return;
    try { await this.resetRun("cpu"); } catch (e) { /* the link is gone: nothing more to do */ }
    if (this.held && this.fam.release) await this.fam.release(this.dap).catch(() => {});
    this.held = false;
  }

  // Start the program. "boot" (the default) is a boot reset (BOOTRST: CPU, peripherals and the boot sequence, as
  // at power-up); the debug link drops and the caller reconnects. "cpu" is a core reset request (SYSRESETREQ)
  // that keeps the debug link up. TI's serial example printed its lines after a boot reset; after a core reset
  // its output stopped part way through a line (2026-10-14), so the pages use "boot".
  async resetRun(level = "boot") {
    await this.giveSemaphore();
    this.halted = false;
    if (this.cpussCtl !== undefined) { await this.dap.write32(this.cpuss, this.cpussCtl).catch(() => {}); this.cpussCtl = undefined; }
    await this.dap.write32(DEMCR, 0);
    await this.dap.write32(DHCSR, 0xA05F0000);
    if (level === "cpu") { await this.dap.write32(AIRCR, 0x05FA0004).catch(() => {}); return; }
    if (this.board.start === "pwrap" && this.fam.start && (await this.fam.start(this.dap, "pwrap"))) return;
    if (this.rst === "aircr") {
      // LP-MSPM33C321A, 2026-10-16: a boot reset with the debugger attached hard-faulted the new program, and a quiet pin
      // reset left the memory port closed until a power cycle; the Arm system reset request restarted it cleanly.
      await this.dap.write32(AIRCR, 0x05FA0004).catch(() => {});
      return;
    }
    if (this.rst === "pin") {
      // The LP-MSPM33C321A faulted (CFSR IACCVIOL, HardFault) when started by a boot reset with the debugger attached,
      // and ran normally after nRESET with the debugger quiet for a moment (2026-10-16): pulse the reset pin, let go.
      await this.dap.cmd([0x10, 0x00, 0x80, 0, 0, 0, 0], "DAP_SWJ_Pins");
      await new Promise((r) => setTimeout(r, 50));
      await this.dap.cmd([0x10, 0x80, 0x80, 0, 0, 0, 0], "DAP_SWJ_Pins");
      await this.dap.disconnect();
      await new Promise((r) => setTimeout(r, 2000));
      return;
    }
    try {
      await this.dap.write32(this.rst.level, BOOTRST);
      await this.dap.write32(this.rst.cmd, RESET_GO).catch(() => {});
    } catch (e) {
      // the boot reset registers did not take the write (first run on a new part): the core reset request instead
      this.dap.select = -1; this.dap.csw = -1;
      await this.dap.write32(AIRCR, 0x05FA0004).catch(() => {});
    }
  }
}

// ---- image and check ----------------------------------------------------------------------------------

export function elfSegments(buf) {
  const b = new Uint8Array(buf), dv = new DataView(buf);
  if (b[0] !== 0x7F || b[1] !== 0x45 || b[4] !== 1 || b[5] !== 1) throw new FlashError(ERR.IMAGE, "the image is not a 32-bit Arm ELF file", "reload the page");
  const phoff = dv.getUint32(28, true), phentsize = dv.getUint16(42, true), phnum = dv.getUint16(44, true);
  const segs = [];
  for (let k = 0; k < phnum; k++) {
    const o = phoff + k * phentsize;
    const type = dv.getUint32(o, true), off = dv.getUint32(o + 4, true), paddr = dv.getUint32(o + 12, true), filesz = dv.getUint32(o + 16, true);
    if (type === 1 && filesz) segs.push({ addr: paddr, data: b.slice(off, off + filesz) });
  }
  return segs;
}

export async function sha256(buf) {
  const d = await crypto.subtle.digest("SHA-256", buf);
  return [...new Uint8Array(d)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

// The same rule as tools/frl/verify.py: the observed levels repeat the asked sequence for at least one full cycle,
// in some phase, each level within the tolerance or 10 percent. A level is one LED's bit (blink) or LED2's three
// colour bits together (colour player). Asked steps with the same level next to each other are one level on the pin.
export function sequenceCheck(samples, levelOf, want, tolMs) {
  const merged = [];
  for (const [lvl, ms] of want) {
    if (merged.length && merged[merged.length - 1][0] === lvl) merged[merged.length - 1][1] += ms; else merged.push([lvl, ms]);
  }
  if (merged.length > 1 && merged[0][0] === merged[merged.length - 1][0]) { merged[0][1] += merged.pop()[1]; }
  const trans = [];
  let last = null;
  for (const x of samples) {
    const lvl = levelOf(x.v);
    if (lvl !== last) { trans.push([x.t, lvl]); last = lvl; }
  }
  const segs = trans.slice(1, -1).map(([t, l], i) => [l, trans[i + 2][0] - t]);
  const n = merged.length;
  const span = samples.length ? samples[samples.length - 1].t - samples[0].t : 0;
  const rate = samples.length / Math.max(span, 1) * 1000;
  let best = 0, ok = false;
  if (n === 1) {   // one steady colour: no changes at all, and the pin holds it
    ok = trans.length === 1 && trans[0][1] === merged[0][0];
  }
  for (let start = 0; start < segs.length && !ok; start++) {
    for (let phase = 0; phase < n && !ok; phase++) {
      let k = 0;
      while (start + k < segs.length) {
        const [lvl, ms] = segs[start + k], [wl, wms] = merged[(phase + k) % n];
        if (lvl !== wl || Math.abs(ms - wms) > Math.max(tolMs, 0.1 * wms)) break;
        k++;
      }
      best = Math.max(best, k);
      if (k >= n) ok = true;
    }
  }
  return { ok, levels: segs.map(([l, ms]) => [l, Math.round(ms)]), samples: samples.length, span: Math.round(span), rate: Math.round(rate), best,
    first: trans.length ? trans[0][1] : null };
}

export function patternCheck(samples, bit, pattern, tolMs) {
  return sequenceCheck(samples, (v) => (v >>> bit) & 1, pattern.map((ms, i) => [i % 2 === 0 ? 1 : 0, ms]), tolMs);
}

// LED2's three pins as one colour number: bit 0 red, bit 1 green, bit 2 blue (the colour player's table format).
export function colorOf(pins) { return (v) => ((v >>> pins.red) & 1) | (((v >>> pins.green) & 1) << 1) | (((v >>> pins.blue) & 1) << 2); }

// One connection to one board: identify first, then any of save, flash, restore and check.
export class Session {
  constructor(transport, board, ui) {
    this.t = transport; this.board = board; this.ui = ui; this.say = ui.say || (() => {});
    this.dap = new Dap(transport); this.m = new Mspm0(this.dap, board);
  }
  async start() {
    await this.t.open();
    this.serial = await this.dap.info(0x03).catch(() => "");          // the XDS110's serial number (DAP_Info)
    this.say(`Connecting through the XDS110${this.serial ? " " + this.serial : ""}`);
    try {
      await this.dap.connect();
      try {
        // Reads only: a factory-region word and the core's debug status. Never a DHCSR write here: writing it
        // releases a core another session left halted, and on the MSPM0G3519 the program that then ran put the
        // chip where the debugger could not reach it (2026-10-15).
        await this.dap.read32(this.board.identity.addr + 4);
        await this.dap.read32(DHCSR);
      } catch (e) {
        if (!(e instanceof FlashError) || e.kind !== ERR.CONNECT) throw e;
        if (this.m.fam.safeConnect) {
          // A chip with a power wrapper that will not answer (a half-written image faulting at boot, or a program asleep):
          // TI's wait-for-debug holds the core in its reset handler (nothing is written).
          this.say("The chip does not answer: catching it in its reset handler with the wait-for-debug method (nothing is written)");
          await this.safeCatch();
        } else {
          this.say("The board's program has the chip asleep: resetting it to wake it (nothing is written)");
          await this.dap.pulseReset();
        }
      }
    } catch (e) {
      // Seen 2026-10-14 with eight XDS110s on one Mac: the chooser shows them all with one name, and a probe on
      // a board whose chip does not answer Arm SWD (CC13xx, CC26xx, C2000) gives ack 7 here.
      if (e instanceof FlashError && e.kind === ERR.CONNECT) {
        throw new FlashError(ERR.WRONG_CHIP, `the XDS110${this.serial ? " " + this.serial : ""} did not reach an Arm chip, so it is not on an ${this.board.kit} (or that board's program is in a deep sleep)`,
          "nothing was written. If other boards are plugged in, press the button again and pick another XDS110, or unplug the others");
      }
      throw e;
    }
    try { this.identity = await this.m.identify(); } catch (e) {
      if (!(e instanceof FlashError) || e.kind !== ERR.CONNECT || !this.m.fam.safeConnect) throw e;
      this.say("The chip does not answer: catching it in its reset handler with the wait-for-debug method (nothing is written)");
      await this.safeCatch();
      this.identity = await this.m.identify();
    }
    this.say(`Found the ${this.board.kit} (chip ${this.identity.deviceid})`);
    return this.identity;
  }
  async safeCatch() {
    this.dap.select = -1; this.dap.csw = -1;
    await this.dap.connect();
    if (!(await this.m.fam.safeConnect(this.dap))) throw new FlashError(ERR.CONNECT, "the chip does not answer and has no wait-for-debug port", "unplug the board, plug it back in, and try again");
    this.m.held = true;
  }
  async save() {
    // Stop the core first (a plain halt, no reset): a program that sleeps the chip made reads fault part way
    // through a 512 KB copy on the MSPM0G3519 (2026-10-15). Everything after a save writes and restarts anyway.
    await this.dap.write32(DHCSR, 0xA05F0003);
    for (let i = 0; i < 50; i++) if ((await this.dap.read32(DHCSR)) & (1 << 17)) break;
    this.m.halted = true;                                              // end() lets the program run again
    const bytes = await this.m.readAll((s) => this.say(`Saving what is on the board: ${s}`));
    return { format: "frl-webflash-backup-1", kit: this.board.kit, ...this.identity, start: this.board.flash.start,
      bytes: bytes.length, sha256: await sha256(bytes.buffer), saved: new Date().toISOString(), data: b64(bytes) };
  }
  async flashImage(image) {
    const t0 = this.ui.now();
    await this.m.flash(image.segments, (s) => this.say(`Flashing: ${s}`));
    this.say("Reading the flash back to compare");
    await this.m.verify(image.segments);
    return Math.round(this.ui.now() - t0);
  }
  async restore(backup) {
    if (backup.format !== "frl-webflash-backup-1") throw new FlashError(ERR.IMAGE, "that file is not a copy this page saved", "choose the .json file this page saved");
    if (backup.deviceid !== this.identity.deviceid || backup.traceid !== this.identity.traceid) {
      throw new FlashError(ERR.WRONG_CHIP, `that copy was saved from another board (trace ID ${backup.traceid}; this one is ${this.identity.traceid})`,
        "nothing was written. Plug in the board the copy came from");
    }
    const bytes = unb64(backup.data);
    if (await sha256(bytes.buffer) !== backup.sha256) throw new FlashError(ERR.IMAGE, "the saved copy is damaged: its contents do not match its checksum", "nothing was written");
    const t0 = this.ui.now();
    await this.m.restoreAll(bytes, (s) => this.say(`Putting it back: ${s}`));
    return Math.round(this.ui.now() - t0);
  }
  // The colour player: put the program on if it is not there yet, then write the sequence table.
  async sendColors(image, table, steps) {
    const t0 = this.ui.now();
    let flashed = false;
    if (!(await this.m.holds(image.segments))) {
      await this.flashImage(image);
      flashed = true;
    }
    this.say("Writing your sequence to the board's flash");
    const words = [parseInt(table.magic, 16), steps.length];
    for (const [c, ms] of steps) words.push(c, ms);
    await this.m.writeSector(parseInt(table.addr, 16), words);
    return { ms: Math.round(this.ui.now() - t0), flashed };
  }
  async checkColors(table, steps) {
    const b = this.board;
    // the colour pins may sit on two GPIO ports (the MSPM0G5187's blue is on GPIOB); LEDs may light when low
    const ch = ["red", "green", "blue"].filter((c) => table.pins[c]);
    const ports = [...new Set(ch.map((c) => parseInt(table.pins[c].gpio || table.gpio, 16)))];
    const inv = table.active_high === false;
    const levelOf = (vals) => ch.reduce((acc, c) => {
      const p = table.pins[c], v = vals[ports.indexOf(parseInt(p.gpio || table.gpio, 16))];
      const on = ((v >>> p.bit) & 1) ^ (inv ? 1 : 0);
      return acc | (on << table.bits[c]);
    }, 0);
    const mask = ch.reduce((m, c) => m | (1 << table.bits[c]), 0);
    // two whole cycles at least, whatever the sequence's length (one cycle is at most 6 s)
    const cycle = steps.reduce((t, [, ms]) => t + ms, 0);
    const secs = Math.min(14, Math.max(b.check.seconds, (2.2 * cycle + 300) / 1000));
    this.say(`Watching the LED for ${secs.toFixed(1)} s`);
    const samples = await this.dap.sampleWords(ports.map((g) => g + b.gpio.dout), secs * 1000, this.ui.now);
    return sequenceCheck(samples, levelOf, steps.map(([c, ms]) => [c & mask, ms]), b.check.tolMs);
  }
  // CoreMark writes its report into RAM (our port's cm_log, cm_len); poll its length until the score line is in.
  async readCoreMark(c, onTick) {
    const t0 = this.ui.now(), limit = (c.seconds || 30) * 1000, addr = parseInt(c.log_addr, 16), lenAt = parseInt(c.len_addr, 16);
    let text = "";
    while (this.ui.now() - t0 < limit) {
      await this.ui.sleep(1000);
      const len = Math.min(await this.dap.read32(lenAt), c.log_bytes);
      if (len > 0) {
        const w = await this.dap.readBlock(addr, Math.ceil(len / 4));
        const b = new Uint8Array(w.length * 4); w.forEach((v, i) => { b[4 * i] = v & 0xFF; b[4 * i + 1] = (v >>> 8) & 0xFF; b[4 * i + 2] = (v >>> 16) & 0xFF; b[4 * i + 3] = v >>> 24; });
        text = new TextDecoder().decode(b.subarray(0, len));
        if (/Correct operation validated|Errors detected|ERROR/.test(text)) break;
      }
      if (onTick) onTick(Math.round((this.ui.now() - t0) / 1000));
    }
    // Our port prints whole seconds; the score is iterations over (ticks / CPU clock), the way the recorded
    // 78.27 was computed (mspm0l1306/bench/coremark/report.py: 1000 iterations, 408,864,304 ticks, 32 MHz).
    const it = text.match(/Iterations\s*:\s*(\d+)/), ticks = text.match(/Total ticks\s*:\s*(\d+)/);
    const score = it && ticks ? Math.round(100 * parseInt(it[1], 10) * c.clock_mhz * 1e6 / parseInt(ticks[1], 10)) / 100 : null;
    return { text, score, valid: /Correct operation validated/.test(text), seconds: Math.round((this.ui.now() - t0) / 1000) };
  }

  async run(level = "boot") {
    await this.m.resetRun(level);
    await this.ui.sleep(300);
    if (level === "cpu") { this.dap.select = -1; this.dap.csw = -1; await this.dap.dpWrite(0x4, 0x50000000).catch(() => {}); return; }
    await this.dap.disconnect();
    for (let tries = 0; ; tries++) {
      try { this.dap.select = -1; this.dap.csw = -1; await this.dap.connect(); break; } catch (e) {
        if (tries >= 5) throw e;
        await this.ui.sleep(150);
      }
    }
    // After the quiet pin reset the LP-MSPM33C321A answered the debugger with zeros for a few seconds (DEVICEID 0
    // at 2.3 s, real values at 4 s; 2026-10-16): wait until the chip's ID reads back before any read that matters.
    if (this.m.rst === "pin" || this.m.rst === "aircr") {
      const id = this.board.identity, t0 = this.ui.now();
      while (this.ui.now() - t0 < 8000) {
        const v = await this.dap.read32(id.addr + 4).catch(() => 0);
        if (((v & id.deviceidMask) >>> 0) === ((id.deviceid & id.deviceidMask) >>> 0)) return;
        await this.ui.sleep(400);
        this.dap.select = -1; this.dap.csw = -1; await this.dap.connect().catch(() => {});
      }
      throw new FlashError(ERR.CONNECT, "the chip did not answer the debugger after its reset", "unplug the board, plug it back in, and try again");
    }
  }
  async check(pattern) {
    const b = this.board;
    this.say(`Timing the LED for ${b.check.seconds} s`);
    const doe = await this.dap.read32(b.led.gpio + b.gpio.doe);
    const samples = await this.dap.sampleWord(b.led.gpio + b.gpio.dout, b.check.seconds * 1000, this.ui.now);
    const inv = b.led.activeHigh === false ? 1 : 0;
    const res = sequenceCheck(samples, (v) => ((v >>> b.led.bit) & 1) ^ inv, pattern.map((ms, i) => [i % 2 === 0 ? 1 : 0, ms]), b.check.tolMs);
    res.outputEnabled = !!((doe >>> b.led.bit) & 1);
    return res;
  }
  async end() {
    // put the write protection back and release the flash semaphore on every exit, not only after a clean start
    try { await this.m.giveSemaphore(); } catch (e) { this.say("Could not put the flash protection back: " + (e.message || e)); }
    try { await this.m.release(); } catch (e) { /* best effort */ }
    await this.dap.disconnect(); await this.t.close();
  }
}

function b64(bytes) { let s = ""; for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode(...bytes.subarray(i, i + 0x8000)); return btoa(s); }
function unb64(text) { const s = atob(text), out = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i); return out; }

// The demo route in one call: identify, flash, read back, run, time the LED.
export async function run(transport, board, image, ui) {
  const s = new Session(transport, board, ui);
  try {
    const id = await s.start();
    const flashMs = await s.flashImage(image);
    await s.run();
    const res = await s.check(image.pattern);
    return { ...res, flashMs, identity: id, crcChecked: s.m.crcChecked || 0, retries: s.m.retries || 0 };
  } finally {
    await s.end();
  }
}

// Estimated seconds for the UI: { bytes: image bytes to write, total: flash bytes read back at the end (default: bytes), erased: sectors }.
// Rates are per family (families/*.js `timing`, measured). A restore writes only the sectors that differ and reads those back.
export function estimateSeconds(board, { bytes = 0, total = bytes, erased = 0, read = 0 } = {}) {
  const t = familyOf(board).timing || { readBps: 15000, writeBps: 15000, eraseSectorMs: 10 };
  const sector = (board.fc && board.fc.sector) || SECTOR;
  return Math.round((bytes / t.writeBps + (total + read) / t.readBps + (erased || Math.ceil(bytes / sector)) * t.eraseSectorMs / 1000) * 10) / 10;
}
